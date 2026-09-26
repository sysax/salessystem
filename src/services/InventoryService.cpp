#include "InventoryService.h"

#include <QSqlQuery>

#include "../core/EventBus.h"

InventoryService::InventoryService(QSqlDatabase db, ProductRepository *products,
                                   InventoryRepository *inventory, EventBus *bus, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_products(products), m_inventory(inventory), m_bus(bus)
{
}

Result<InventoryService::StockResult>
InventoryService::registerPurchase(int productId, double qty, double cost, const QString &supplier,
                                   const QString &invoice, const QString &user)
{
    if (qty <= 0)
        return Result<StockResult>::failure(QStringLiteral("La cantidad debe ser mayor a 0"));
    const auto p = m_products->findById(productId);
    if (!p)
        return Result<StockResult>::failure(QStringLiteral("Producto %1 no existe").arg(productId));

    const double newStock = p->stock + qty;
    const double newCost = (p->stock * p->priceBuy + qty * cost) / (newStock > 0 ? newStock : 1);
    Product upd = *p;
    upd.stock = newStock;
    upd.priceBuy = newCost;
    m_products->update(p->sku, upd);
    m_inventory->record(p->sku, p->name, QStringLiteral("Entrada"), qty, p->stock, newStock,
                        QStringLiteral("Compra %1 uds a $%2 (%3)%4")
                            .arg(qty)
                            .arg(cost, 0, 'f', 0)
                            .arg(supplier)
                            .arg(invoice.isEmpty() ? QString() : QStringLiteral(" ") + invoice),
                        user);
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
    m_products->setStockBySku(sku, newStock);
    const QString type = delta > 0 ? QStringLiteral("Entrada") : QStringLiteral("Salida");
    m_inventory->record(sku, p->name, type, delta, p->stock, newStock, reason, user);
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
    const double newStock = p->stock - qty;
    m_products->setStockBySku(sku, newStock);
    const QString detail = reason.trimmed().isEmpty() ? QStringLiteral("merma") : reason.trimmed();
    m_inventory->record(sku, p->name, QStringLiteral("Merma"), -qty, p->stock, newStock, detail,
                        user);
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
    Product upd = *p;
    upd.location = toLocation.trimmed();
    auto r = m_products->update(sku, upd);
    if (!r.ok())
        return StatusResult::failure(r.error());
    m_inventory->record(sku, p->name, QStringLiteral("Transferencia"), qty, p->stock, p->stock,
                        QStringLiteral("%1->%2 %3")
                            .arg(p->location, toLocation.trimmed(), reason.trimmed().left(30)),
                        user);
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
