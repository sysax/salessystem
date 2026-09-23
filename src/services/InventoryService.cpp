#include "InventoryService.h"

#include "../core/EventBus.h"

InventoryService::InventoryService(QSqlDatabase db, ProductRepository *products,
                                   InventoryRepository *inventory, EventBus *bus, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_products(products), m_inventory(inventory),
      m_bus(bus)
{
}

Result<InventoryService::StockResult> InventoryService::registerPurchase(
    int productId, double qty, double cost, const QString &supplier, const QString &invoice,
    const QString &user)
{
    if (qty <= 0)
        return Result<StockResult>::failure(
            QStringLiteral("La cantidad debe ser mayor a 0"));
    const auto p = m_products->findById(productId);
    if (!p)
        return Result<StockResult>::failure(
            QStringLiteral("Producto %1 no existe").arg(productId));

    const double newStock = p->stock + qty;
    const double newCost =
        (p->stock * p->priceBuy + qty * cost) / (newStock > 0 ? newStock : 1);
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

Result<InventoryService::StockResult> InventoryService::registerAdjustment(
    const QString &sku, double delta, const QString &reason, const QString &user)
{
    if (delta == 0)
        return Result<StockResult>::failure(
            QStringLiteral("La cantidad debe ser diferente de 0"));
    if (reason.trimmed().isEmpty())
        return Result<StockResult>::failure(
            QStringLiteral("Justificación requerida para ajuste"));
    const auto p = m_products->findBySku(sku);
    if (!p)
        return Result<StockResult>::failure(
            QStringLiteral("SKU %1 no encontrado").arg(sku));
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
        return StatusResult::failure(
            QStringLiteral("Cantidad inválida: stock %1").arg(p->stock));
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
