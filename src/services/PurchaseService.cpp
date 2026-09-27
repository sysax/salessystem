#include "PurchaseService.h"

#include <QDate>
#include <QSqlQuery>

#include "../core/Transaction.h"
#include "../repositories/Counters.h"
#include "../services/SettingsService.h"

PurchaseService::PurchaseService(QSqlDatabase db, PurchaseRepository *purchases,
                                 ProductRepository *products, SupplierRepository *suppliers,
                                 InventoryRepository *inventory, PayablesRepository *payables,
                                 AuditRepository *audit, QObject *parent, SettingsService *settings)
    : QObject(parent), m_db(std::move(db)), m_purchases(purchases), m_products(products),
      m_suppliers(suppliers), m_inventory(inventory), m_payables(payables), m_audit(audit),
      m_settings(settings)
{
}

Result<Purchase> PurchaseService::create(const QString &supplierName, const QString &sku,
                                         double qty, const QString &user)
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
    const Money unitCost = prod->priceBuy.isPositive() ? prod->priceBuy : prod->price * 0.7;
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
                          .arg(p.total.toCop(), 0, 'f', 0));
    return r;
}

Result<Purchase> PurchaseService::receive(const QString &folio, const QString &user)
{
    const auto po = m_purchases->find(folio);
    if (!po)
        return Result<Purchase>::failure(QStringLiteral("Orden %1 no encontrada").arg(folio));
    // Recepción total = entregar todo lo pendiente de cada línea.
    QMap<QString, double> rest;
    for (const PurchaseItem &it : po->items) {
        const double pending = it.qty - po->received.value(it.sku, 0.0);
        if (pending > 1e-9)
            rest[it.sku] = pending;
    }
    if (rest.isEmpty())
        return Result<Purchase>::failure(QStringLiteral("Orden %1 ya recibida").arg(folio));
    return receive(folio, rest, user);
}

Result<Purchase> PurchaseService::receive(const QString &folio,
                                          const QMap<QString, double> &delivery,
                                          const QString &user)
{
    const auto po = m_purchases->find(folio);
    if (!po)
        return Result<Purchase>::failure(QStringLiteral("Orden %1 no encontrada").arg(folio));
    if (po->status == QLatin1String("Recibida"))
        return Result<Purchase>::failure(QStringLiteral("Orden %1 ya recibida").arg(folio));
    if (po->status == QLatin1String("Cancelada"))
        return Result<Purchase>::failure(QStringLiteral("Orden %1 cancelada").arg(folio));
    if (delivery.isEmpty())
        return Result<Purchase>::failure(QStringLiteral("Entrega vacía"));

    // Validar contra lo pedido (sin exceder por línea, SKUs de la orden).
    QMap<QString, double> ordered;
    QMap<QString, Money> unitCost;
    for (const PurchaseItem &it : po->items) {
        ordered[it.sku] = ordered.value(it.sku, 0.0) + it.qty;
        if (!unitCost.contains(it.sku))
            unitCost[it.sku] = it.priceBuy;
    }
    for (auto it = delivery.begin(); it != delivery.end(); ++it) {
        if (!ordered.contains(it.key()))
            return Result<Purchase>::failure(
                QStringLiteral("SKU %1 no está en la orden %2").arg(it.key(), folio));
        if (it.value() <= 1e-9)
            return Result<Purchase>::failure(
                QStringLiteral("Cantidad >0 para %1").arg(it.key()));
        const double already = po->received.value(it.key(), 0.0);
        if (already + it.value() > ordered.value(it.key()) + 1e-9)
            return Result<Purchase>::failure(
                QStringLiteral("SKU %1: pedido %2, ya recibido %3, entrega %4 excede")
                    .arg(it.key())
                    .arg(ordered.value(it.key()))
                    .arg(already)
                    .arg(it.value()));
    }

    // Fase 1+5: entrega + movimientos + CxP proporcional en una sola
    // transacción. La CxP (id = folio) crece con cada entrega parcial.
    Transaction tx(m_db);
    if (!tx.isValid())
        return Result<Purchase>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    Money deliveryValue;
    QMap<QString, double> received = po->received;
    for (auto it = delivery.begin(); it != delivery.end(); ++it) {
        const auto prod = m_products->findBySku(it.key());
        if (!prod)
            return Result<Purchase>::failure(
                QStringLiteral("SKU %1 de la orden ya no existe").arg(it.key()));
        const double before = prod->stock;
        if (!m_products->setStockBySku(it.key(), before + it.value()))
            return Result<Purchase>::failure(
                QStringLiteral("No se pudo actualizar el stock de %1").arg(it.key()));
        if (!m_inventory->record(it.key(), prod->name, QStringLiteral("Entrada"), it.value(),
                                 before, before + it.value(),
                                 QStringLiteral("Recepción %1").arg(folio), user)) {
            return Result<Purchase>::failure(
                QStringLiteral("No se pudo registrar el movimiento de %1").arg(it.key()));
        }
        received[it.key()] = received.value(it.key(), 0.0) + it.value();
        deliveryValue += unitCost.value(it.key()) * it.value();
    }
    if (!m_purchases->setReceived(folio, received))
        return Result<Purchase>::failure(
            QStringLiteral("No se pudo registrar la entrega de %1").arg(folio));

    // ¿Completa? Todas las líneas cubiertas → Recibida, si no Parcial.
    bool complete = true;
    for (auto it = ordered.begin(); it != ordered.end(); ++it) {
        if (received.value(it.key(), 0.0) < it.value() - 1e-9) {
            complete = false;
            break;
        }
    }
    if (!m_purchases->setStatus(
            folio, complete ? QStringLiteral("Recibida") : QStringLiteral("Parcial")))
        return Result<Purchase>::failure(
            QStringLiteral("No se pudo marcar la orden %1").arg(folio));

    // CxP a N días de settings (default 30; 2 % pronto pago con
    // TecnoMayorista): se crea en la primera entrega y crece con cada parcial.
    if (const auto existing = m_payables->find(folio)) {
        Payable grow = *existing;
        grow.amount += deliveryValue;
        grow.balance += deliveryValue;
        QSqlQuery up(m_db);
        up.prepare(QStringLiteral("UPDATE payables SET amount=?, balance=? WHERE id=?"));
        up.addBindValue(grow.amount.toCop());
        up.addBindValue(grow.balance.toCop());
        up.addBindValue(folio);
        if (!up.exec())
            return Result<Purchase>::failure(
                QStringLiteral("No se pudo ampliar la cuenta por pagar de %1").arg(folio));
    } else {
        Payable cxp;
        cxp.id = folio;
        cxp.supplier = po->supplier;
        // Fase 3: días de pago externalizados (default 30).
        cxp.due = QDate::currentDate()
                      .addDays(m_settings ? m_settings->payableDays() : 30)
                      .toString(Qt::ISODate);
        cxp.amount = deliveryValue;
        cxp.paid = Money();
        cxp.balance = deliveryValue;
        cxp.discountEarly = po->supplier.contains(QLatin1String("TecnoMayorista"))
                                ? Money::fromCop(2.0)
                                : Money();
        cxp.status = QStringLiteral("Pendiente");
        if (!m_payables->create(cxp).ok())
            return Result<Purchase>::failure(
                QStringLiteral("No se pudo crear la cuenta por pagar de %1").arg(folio));
    }

    if (m_audit)
        m_audit->log(user, QStringLiteral("compra_recibida"),
                     QStringLiteral("%1 %2 $%3 %4")
                         .arg(folio, po->supplier)
                          .arg(deliveryValue.toCop(), 0, 'f', 0)
                         .arg(complete ? QStringLiteral("total")
                                       : QStringLiteral("parcial")));
    if (!tx.commit())
        return Result<Purchase>::failure(QStringLiteral("No se pudo confirmar la recepción"));
    return Result<Purchase>::success(*m_purchases->find(folio));
}

Result<Purchase> PurchaseService::cancel(const QString &folio, const QString &user)
{
    const auto po = m_purchases->find(folio);
    if (!po)
        return Result<Purchase>::failure(QStringLiteral("Orden %1 no encontrada").arg(folio));
    if (po->status == QLatin1String("Recibida") || po->status == QLatin1String("Parcial"))
        return Result<Purchase>::failure(QStringLiteral("No se puede cancelar orden ya recibida"));
    m_purchases->setStatus(folio, QStringLiteral("Cancelada"));
    if (m_audit)
        m_audit->log(user, QStringLiteral("compra_cancelada"), folio);
    return Result<Purchase>::success(*m_purchases->find(folio));
}
