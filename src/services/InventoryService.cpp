#include "InventoryService.h"

#include <QSqlQuery>

#include "../core/EventBus.h"
#include "../core/Transaction.h"

InventoryService::InventoryService(QSqlDatabase db, ProductRepository *products,
                                   InventoryRepository *inventory, EventBus *bus, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_products(products), m_inventory(inventory), m_bus(bus)
{
}

Result<InventoryService::StockResult>
InventoryService::registerPurchase(int productId, double qty, double cost, const QString &supplier,
                                   const QString &invoice, const QString &user, const QString &lote,
                                   const QString &vencimiento)
{
    if (qty <= 0)
        return Result<StockResult>::failure(QStringLiteral("La cantidad debe ser mayor a 0"));
    const auto p = m_products->findById(productId);
    if (!p)
        return Result<StockResult>::failure(QStringLiteral("Producto %1 no existe").arg(productId));

    const double newStock = p->stock + qty;
    const double newCost = (p->stock * p->priceBuy + qty * cost) / (newStock > 0 ? newStock : 1);
    // Fase 1: costo + stock + movimiento en una transacción.
    Transaction tx(m_db);
    if (!tx.isValid())
        return Result<StockResult>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    Product upd = *p;
    upd.stock = newStock;
    upd.priceBuy = newCost;
    if (!m_products->update(p->sku, upd).ok())
        return Result<StockResult>::failure(
            QStringLiteral("No se pudo actualizar el producto %1").arg(p->sku));
    if (!m_inventory->record(
            p->sku, p->name, QStringLiteral("Entrada"), qty, p->stock, newStock,
            QStringLiteral("Compra %1 uds a $%2 (%3)%4")
                .arg(qty)
                .arg(cost, 0, 'f', 0)
                .arg(supplier)
                .arg(invoice.isEmpty() ? QString() : QStringLiteral(" ") + invoice),
            user)) {
        return Result<StockResult>::failure(QStringLiteral("No se pudo registrar el movimiento"));
    }
    // Fase 5: abrir lote PEPS con el costo de esta entrada (misma tx: sin
    // lote huérfano ni entrada sin lote).
    if (!m_inventory
             ->addLot(p->sku, lote.trimmed().isEmpty() ? invoice.trimmed() : lote.trimmed(),
                      vencimiento.trimmed(), qty, cost)
             .ok())
        return Result<StockResult>::failure(QStringLiteral("No se pudo abrir el lote PEPS"));
    if (!tx.commit())
        return Result<StockResult>::failure(QStringLiteral("No se pudo confirmar la entrada"));
    if (m_bus)
        m_bus->publish(EventBus::InventoryUpdated,
                       {{"product_id", productId}, {"quantity_change", qty}});
    StockResult r;
    r.sku = p->sku;
    r.newStock = newStock;
    r.newCost = newCost;
    return Result<StockResult>::success(r);
}

Result<InventoryService::StockResult> InventoryService::registerAdjustment(const QString &sku,
                                                                           double delta,
                                                                           const QString &reason,
                                                                           const QString &user)
{
    if (delta == 0)
        return Result<StockResult>::failure(QStringLiteral("La cantidad debe ser diferente de 0"));
    if (reason.trimmed().isEmpty())
        return Result<StockResult>::failure(QStringLiteral("Justificación requerida para ajuste"));
    const auto p = m_products->findBySku(sku);
    if (!p)
        return Result<StockResult>::failure(QStringLiteral("SKU %1 no encontrado").arg(sku));
    const double newStock = p->stock + delta;
    if (newStock < -1e-9)
        return Result<StockResult>::failure(
            QStringLiteral("No se puede ajustar. Stock actual: %1, Ajuste: %2. "
                           "Resultaría en stock negativo.")
                .arg(p->stock)
                .arg(delta));
    // Fase 1: ajuste + movimiento en una transacción.
    Transaction adjTx(m_db);
    if (!adjTx.isValid())
        return Result<StockResult>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    if (!m_products->setStockBySku(sku, newStock))
        return Result<StockResult>::failure(
            QStringLiteral("No se pudo ajustar el stock de %1").arg(sku));
    const QString type = delta > 0 ? QStringLiteral("Entrada") : QStringLiteral("Salida");
    if (!m_inventory->record(sku, p->name, type, delta, p->stock, newStock, reason, user))
        return Result<StockResult>::failure(QStringLiteral("No se pudo registrar el movimiento"));
    if (!adjTx.commit())
        return Result<StockResult>::failure(QStringLiteral("No se pudo confirmar el ajuste"));
    if (m_bus)
        m_bus->publish(EventBus::InventoryUpdated,
                       {{"product_id", p->id}, {"quantity_change", delta}});
    StockResult r;
    r.sku = sku;
    r.newStock = newStock;
    r.newCost = p->priceBuy;
    return Result<StockResult>::success(r);
}

Result<InventoryService::StockResult> InventoryService::registerWaste(const QString &sku,
                                                                      double qty,
                                                                      const QString &reason,
                                                                      const QString &user)
{
    // Fase 4: merma (abarrotes): salida tipo "Merma", nunca stock negativo.
    if (qty <= 1e-9)
        return Result<StockResult>::failure(QStringLiteral("Cantidad >0"));
    const auto p = m_products->findBySku(sku);
    if (!p)
        return Result<StockResult>::failure(QStringLiteral("SKU %1 no encontrado").arg(sku));
    if (p->stock < qty - 1e-9)
        return Result<StockResult>::failure(
            QStringLiteral("Merma mayor al stock (%1)").arg(p->stock));
    // Fase 1: merma + movimiento en una transacción.
    Transaction wasteTx(m_db);
    if (!wasteTx.isValid())
        return Result<StockResult>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    const double newStock = p->stock - qty;
    if (!m_products->setStockBySku(sku, newStock))
        return Result<StockResult>::failure(
            QStringLiteral("No se pudo descontar la merma de %1").arg(sku));
    const QString detail = reason.trimmed().isEmpty() ? QStringLiteral("merma") : reason.trimmed();
    if (!m_inventory->record(sku, p->name, QStringLiteral("Merma"), -qty, p->stock, newStock,
                             detail, user)) {
        return Result<StockResult>::failure(QStringLiteral("No se pudo registrar el movimiento"));
    }
    if (!wasteTx.commit())
        return Result<StockResult>::failure(QStringLiteral("No se pudo confirmar la merma"));
    if (m_bus)
        m_bus->publish(EventBus::InventoryUpdated,
                       {{"product_id", p->id}, {"quantity_change", -qty}});
    StockResult r;
    r.sku = sku;
    r.newStock = newStock;
    r.newCost = p->priceBuy;
    return Result<StockResult>::success(r);
}

StatusResult InventoryService::transfer(const QString &sku, double qty, const QString &toLocation,
                                        const QString &reason, const QString &user)
{
    const auto p = m_products->findBySku(sku);
    if (!p)
        return StatusResult::failure(QStringLiteral("SKU %1 no encontrado").arg(sku));
    if (toLocation.trimmed().isEmpty())
        return StatusResult::failure(QStringLiteral("Ubicación destino requerida"));
    if (reason.trimmed().isEmpty())
        return StatusResult::failure(QStringLiteral("Motivo requerido"));
    if (qty <= 0 || qty > p->stock)
        return StatusResult::failure(QStringLiteral("Cantidad inválida: stock %1").arg(p->stock));
    // Fase 1: cambio de ubicación + movimiento en una transacción.
    Transaction tx(m_db);
    if (!tx.isValid())
        return StatusResult::failure(QStringLiteral("No se pudo iniciar la transacción"));
    Product upd = *p;
    upd.location = toLocation.trimmed();
    auto r = m_products->update(sku, upd);
    if (!r.ok())
        return StatusResult::failure(r.error());
    if (!m_inventory->record(sku, p->name, QStringLiteral("Transferencia"), qty, p->stock, p->stock,
                             QStringLiteral("%1->%2 %3")
                                 .arg(p->location, toLocation.trimmed(), reason.trimmed().left(30)),
                             user)) {
        return StatusResult::failure(QStringLiteral("No se pudo registrar el movimiento"));
    }
    if (!tx.commit())
        return StatusResult::failure(QStringLiteral("No se pudo confirmar la transferencia"));
    return StatusResult::success({});
}

QList<Product> InventoryService::lowStock(double multiplier) const
{
    QList<Product> out;
    for (const Product &p : m_products->list()) {
        if (p.stock < p.stockMin * multiplier)
            out << p;
    }
    return out;
}

InventoryService::Valuation InventoryService::valuation() const
{
    Valuation v;
    const InventoryValue val = m_inventory->value();
    v.totalValue = val.costValue;
    for (const Product &p : m_products->list()) {
        if (p.stock * p.priceBuy > 0)
            ++v.productsCount;
    }
    return v;
}

QList<InventoryMovement> InventoryService::movementsBySku(const QString &sku) const
{
    return m_inventory->movementsBySku(sku);
}

namespace
{
// Transacción inline (BEGIN/COMMIT/ROLLBACK explícitos): no depende de
// Transaction.h de Fase 1; reserva + movimiento quedan atómicos.
struct InlineTx
{
    explicit InlineTx(QSqlDatabase &db) : m_db(db)
    {
        QSqlQuery q(m_db);
        m_ok = q.exec(QStringLiteral("BEGIN IMMEDIATE"));
    }
    ~InlineTx()
    {
        if (m_ok && !m_done) {
            QSqlQuery q(m_db);
            q.exec(QStringLiteral("ROLLBACK"));
        }
    }
    bool commit()
    {
        QSqlQuery q(m_db);
        m_done = q.exec(QStringLiteral("COMMIT"));
        return m_done;
    }
    bool valid() const
    {
        return m_ok;
    }
    QSqlDatabase m_db;
    bool m_ok = false;
    bool m_done = false;
};
} // namespace

Result<InventoryService::ReserveResult> InventoryService::reserveStock(const QString &sku,
                                                                       double qty,
                                                                       const QString &reason,
                                                                       const QString &user)
{
    if (qty <= 0)
        return Result<ReserveResult>::failure(QStringLiteral("Cantidad >0"));
    if (reason.trimmed().isEmpty())
        return Result<ReserveResult>::failure(QStringLiteral("Justificación requerida"));
    const auto p = m_products->findBySku(sku);
    if (!p)
        return Result<ReserveResult>::failure(QStringLiteral("SKU %1 no encontrado").arg(sku));
    InlineTx resTx(m_db);
    if (!resTx.valid())
        return Result<ReserveResult>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    if (!m_products->reserveAtomic(p->id, qty))
        return Result<ReserveResult>::failure(
            QStringLiteral("Sin disponible para apartar (disponible %1)").arg(p->available()));
    const auto cur = m_products->findBySku(sku);
    if (!cur)
        return Result<ReserveResult>::failure(QStringLiteral("SKU %1 no encontrado").arg(sku));
    if (!m_inventory->record(sku, p->name, QStringLiteral("Apartado"), -qty, p->available(),
                             cur->available(), reason, user))
        return Result<ReserveResult>::failure(QStringLiteral("No se pudo registrar el movimiento"));
    if (!resTx.commit())
        return Result<ReserveResult>::failure(QStringLiteral("No se pudo confirmar el apartado"));
    if (m_bus)
        m_bus->publish(EventBus::InventoryUpdated,
                       {{"product_id", p->id}, {"quantity_change", 0.0}});
    ReserveResult r;
    r.sku = sku;
    r.reserved = cur->reserved;
    r.available = cur->available();
    return Result<ReserveResult>::success(r);
}

Result<InventoryService::ReserveResult>
InventoryService::releaseStock(const QString &sku, double qty, const QString &user)
{
    if (qty <= 0)
        return Result<ReserveResult>::failure(QStringLiteral("Cantidad >0"));
    const auto p = m_products->findBySku(sku);
    if (!p)
        return Result<ReserveResult>::failure(QStringLiteral("SKU %1 no encontrado").arg(sku));
    InlineTx relTx(m_db);
    if (!relTx.valid())
        return Result<ReserveResult>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    if (!m_products->releaseAtomic(p->id, qty))
        return Result<ReserveResult>::failure(QStringLiteral("No se pudo liberar %1").arg(sku));
    const auto cur = m_products->findBySku(sku);
    if (!cur)
        return Result<ReserveResult>::failure(QStringLiteral("SKU %1 no encontrado").arg(sku));
    if (!m_inventory->record(sku, p->name, QStringLiteral("Liberado"), qty, p->available(),
                             cur->available(), QStringLiteral("liberación manual"), user))
        return Result<ReserveResult>::failure(QStringLiteral("No se pudo registrar el movimiento"));
    if (!relTx.commit())
        return Result<ReserveResult>::failure(QStringLiteral("No se pudo confirmar"));
    if (m_bus)
        m_bus->publish(EventBus::InventoryUpdated,
                       {{"product_id", p->id}, {"quantity_change", 0.0}});
    ReserveResult r;
    r.sku = sku;
    r.reserved = cur->reserved;
    r.available = cur->available();
    return Result<ReserveResult>::success(r);
}

InventoryService::Valuation InventoryService::valuation(const QString &method) const
{
    // Fase 5: "peps" suma lotes (costo histórico por entrada); cualquier
    // otro valor usa el promedio ponderado de products.priceBuy.
    if (method.trimmed().toLower() == QLatin1String("peps")) {
        Valuation v;
        v.totalValue = m_inventory->lotsValue();
        for (const Product &p : m_products->list()) {
            if (p.stock > 1e-9)
                ++v.productsCount;
        }
        return v;
    }
    return valuation();
}

Result<double> InventoryService::consumeFifo(const QString &sku, double qty)
{
    // Fase 5: salida PEPS sin tocar products.stock (el llamador descuenta
    // el físico en la misma transacción que la venta/ajuste).
    if (qty <= 1e-9)
        return Result<double>::failure(QStringLiteral("Cantidad >0"));
    Transaction tx(m_db);
    if (!tx.isValid())
        return Result<double>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    double need = qty;
    double cogs = 0.0;
    for (const Lot &l : m_inventory->lotsBySku(sku)) {
        if (need <= 1e-9)
            break;
        const double take = qMin(need, l.qty);
        if (!m_inventory->reduceLot(l.id, take))
            return Result<double>::failure(
                QStringLiteral("No se pudo consumir el lote %1").arg(l.id));
        need -= take;
        cogs += take * l.cost;
    }
    if (need > 1e-9)
        return Result<double>::failure(
            QStringLiteral("Lotes insuficientes para %1 (faltan %2)").arg(sku).arg(need));
    if (!tx.commit())
        return Result<double>::failure(QStringLiteral("No se pudo confirmar el consumo PEPS"));
    return Result<double>::success(cogs);
}

Result<InventoryCount> InventoryService::startCount(const QString &sku, double counted,
                                                   const QString &reason, const QString &user)
{
    const auto p = m_products->findBySku(sku);
    if (!p)
        return Result<InventoryCount>::failure(QStringLiteral("SKU %1 no encontrado").arg(sku));
    return m_inventory->startCount(sku, p->stock, counted, reason, user);
}

Result<InventoryCount> InventoryService::applyCount(int countId, const QString &user)
{
    const auto c = m_inventory->findCount(countId);
    if (!c)
        return Result<InventoryCount>::failure(
            QStringLiteral("Conteo %1 no existe").arg(countId));
    if (c->status != QLatin1String("Pendiente"))
        return Result<InventoryCount>::failure(
            QStringLiteral("Conteo %1 ya aplicado").arg(countId));
    // La diferencia se vuelve ajuste justificado (con motivo del conteo).
    if (qAbs(c->diff) > 1e-9) {
        auto adj = registerAdjustment(
            c->sku, c->diff,
            QStringLiteral("Conteo #%1: %2").arg(c->id).arg(c->reason.trimmed().left(200)), user);
        if (!adj.ok())
            return Result<InventoryCount>::failure(adj.error());
    }
    if (!m_inventory->markCountApplied(countId))
        return Result<InventoryCount>::failure(
            QStringLiteral("No se pudo cerrar el conteo %1").arg(countId));
    return Result<InventoryCount>::success(*m_inventory->findCount(countId));
}

QList<InventoryCount> InventoryService::listCounts(const QString &status) const
{
    return m_inventory->listCounts(status);
}

QList<Lot> InventoryService::expiringLots(int days) const
{
    return m_inventory->expiringLots(days);
}
