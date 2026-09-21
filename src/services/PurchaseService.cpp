#include "PurchaseService.h"

#include <QDate>

#include "../repositories/Counters.h"

PurchaseService::PurchaseService(QSqlDatabase db, PurchaseRepository *purchases,
                                 ProductRepository *products, SupplierRepository *suppliers,
                                 InventoryRepository *inventory, PayablesRepository *payables,
                                 AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_purchases(purchases), m_products(products),
      m_suppliers(suppliers), m_inventory(inventory), m_payables(payables), m_audit(audit)
{
}

Result<Purchase> PurchaseService::create(const QString &supplierName, const QString &sku,
                                         int qty, const QString &user)
{
    const auto sup = m_suppliers->findByName(supplierName);
    if (!sup)
        return Result<Purchase>::failure(
            QStringLiteral("Proveedor %1 no encontrado").arg(supplierName));
    const auto prod = m_products->findBySku(sku);
    if (!prod)
        return Result<Purchase>::failure(QStringLiteral("SKU %1 no encontrado").arg(sku));
    if (qty <= 0)
        return Result<Purchase>::failure(QStringLiteral("Cantidad >0"));
    const double unitCost = prod->priceBuy > 0 ? prod->priceBuy : prod->price * 0.7;
    Purchase p;
    p.id = Counters::next(m_db, QStringLiteral("PURCHASE_COUNTER"), QStringLiteral("OC"));
    p.date = QDate::currentDate().toString(Qt::ISODate);
    p.supplier = sup->name;
    p.total = unitCost * qty;
    p.status = QStringLiteral("Pendiente");
    p.items = {PurchaseItem{sku, qty, unitCost}};
    p.notes = QStringLiteral("Pedido %1 x%2").arg(sku).arg(qty);
    auto r = m_purchases->insert(p);
    if (!r.ok())
        return r;
    if (m_audit)
        m_audit->log(user, QStringLiteral("compra_creada"),
                     QStringLiteral("%1 %2 %3 x%4 $%5")
                         .arg(p.id, sup->name, sku)
                         .arg(qty)
                         .arg(p.total, 0, 'f', 0));
    return r;
}

Result<Purchase> PurchaseService::receive(const QString &folio, const QString &user)
{
    const auto po = m_purchases->find(folio);
    if (!po)
        return Result<Purchase>::failure(
            QStringLiteral("Orden %1 no encontrada").arg(folio));
    if (po->status == QLatin1String("Recibida"))
        return Result<Purchase>::failure(
            QStringLiteral("Orden %1 ya recibida").arg(folio));
    if (po->status == QLatin1String("Cancelada"))
        return Result<Purchase>::failure(
            QStringLiteral("Orden %1 cancelada").arg(folio));

    for (const PurchaseItem &it : po->items) {
        const auto prod = m_products->findBySku(it.sku);
        if (!prod)
            continue;
        const int before = prod->stock;
        m_products->setStockBySku(it.sku, before + it.qty);
        m_inventory->record(it.sku, prod->name, QStringLiteral("Entrada"), it.qty, before,
                            before + it.qty, QStringLiteral("Recepción %1").arg(folio), user);
    }
    m_purchases->setStatus(folio, QStringLiteral("Recibida"));

    // CxP automática a 30 días (2 % pronto pago con TecnoMayorista)
    Payable cxp;
    cxp.id = folio;
    cxp.supplier = po->supplier;
    cxp.due = QDate::currentDate().addDays(30).toString(Qt::ISODate);
    cxp.amount = po->total;
    cxp.paid = 0.0;
    cxp.balance = po->total;
    cxp.discountEarly = po->supplier.contains(QLatin1String("TecnoMayorista")) ? 2.0 : 0.0;
    cxp.status = QStringLiteral("Pendiente");
    m_payables->create(cxp);

    if (m_audit)
        m_audit->log(user, QStringLiteral("compra_recibida"),
                     QStringLiteral("%1 %2 $%3").arg(folio, po->supplier).arg(po->total, 0, 'f', 0));
    return Result<Purchase>::success(*m_purchases->find(folio));
}

Result<Purchase> PurchaseService::cancel(const QString &folio, const QString &user)
{
    const auto po = m_purchases->find(folio);
    if (!po)
        return Result<Purchase>::failure(
            QStringLiteral("Orden %1 no encontrada").arg(folio));
    if (po->status == QLatin1String("Recibida"))
        return Result<Purchase>::failure(
            QStringLiteral("No se puede cancelar orden ya recibida"));
    m_purchases->setStatus(folio, QStringLiteral("Cancelada"));
    if (m_audit)
        m_audit->log(user, QStringLiteral("compra_cancelada"), folio);
    return Result<Purchase>::success(*m_purchases->find(folio));
}
