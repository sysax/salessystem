#include "SalesController.h"

#include "../domain/Attrs.h"

SalesController::SalesController(SaleRepository *sales, SalesService *service,
                                 SerialRepository *serials, ProductRepository *products,
                                 QObject *parent)
    : QObject(parent), m_repos(sales), m_service(service), m_serials(serials),
      m_products(products)
{
    refresh();
}

QVariantMap SalesController::toMap(const Sale &s)
{
    QVariantMap pay;
    for (auto it = s.payments.begin(); it != s.payments.end(); ++it)
        pay[it.key()] = it.value();
    return {{"id", s.id},
            {"date", s.date},
            {"client", s.client},
            {"total", s.total},
            {"subtotal", s.subtotal},
            {"tax", s.tax},
            {"discount", s.discount},
            {"status", s.status},
            {"docType", s.docType},
            {"payment", s.payment},
            {"payments", pay},
            {"paid", s.paid},
            {"balance", s.balance},
            {"cufe", s.dianCufe}};
}

void SalesController::refresh()
{
    m_sales.clear();
    for (const Sale &s : m_repos->list())
        m_sales << toMap(s);
    emit salesChanged();
}

QVariantMap SalesController::detail(const QString &id) const
{
    const auto s = m_repos->find(id);
    if (!s)
        return {{"ok", false}, {"error", QStringLiteral("No encontrada")}};
    QVariantMap d = toMap(*s);
    QVariantList items;
    for (const SaleItem &it : m_repos->itemsFor(id)) {
        QVariantMap m{{"productId", it.productId},
                      {"qty", it.qty},
                      {"subtotal", it.subtotal},
                      {"serial", it.serial}};
        if (it.attrsJson.trimmed() != QStringLiteral("{}") && !it.attrsJson.trimmed().isEmpty())
            m["attrs"] = it.attrsJson;
        items << m;
    }
    d["items"] = items;
    d["ok"] = true;
    return d;
}

QVariantMap SalesController::advance(const QString &id, const QString &status,
                                     const QString &user)
{
    const auto r = m_repos->advanceStatus(id, status, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}};
}

QVariantMap SalesController::cancel(const QString &id, const QString &reason,
                                    const QString &user)
{
    const auto r = m_service->cancel(id, reason, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}};
}

QVariantMap SalesController::createDoc(const QString &type, const QString &client,
                                       double total, const QString &user)
{
    const auto r = m_repos->createDocument(type, client, total, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}, {"id", r.value().id}};
}

QVariantMap SalesController::creditNote(const QString &id, double amount,
                                        const QString &reason, const QString &user)
{
    const auto r = m_repos->createCreditNote(id, amount, reason, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}, {"id", r.value().id}};
}

QVariantMap SalesController::debitNote(const QString &id, double amount,
                                       const QString &reason, const QString &user)
{
    const auto r = m_repos->createDebitNote(id, amount, reason, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}, {"id", r.value().id}};
}

QVariantMap SalesController::warrantyFor(const QString &serial) const
{
    if (!m_serials || !m_products)
        return {{"ok", false}, {"error", QStringLiteral("Sin repositorio de seriales")}};
    const auto s = m_serials->find(serial);
    if (!s)
        return {{"ok", false}, {"error", QStringLiteral("Serial no registrado")}};
    int months = 12;
    if (const auto p = m_products->findBySku(s->sku))
        months = Attrs::integer(p->attrsJson, Attrs::KWarrantyMonths, 12);
    QVariantMap r = m_serials->warrantyStatus(serial, months);
    r["ok"] = r.value(QStringLiteral("error"), {}).toString().isEmpty();
    return r;
}

QVariantMap SalesController::markRma(const QString &serial, const QString &notes,
                                     const QString &user)
{
    Q_UNUSED(user);
    if (!m_serials)
        return {{"ok", false}, {"error", QStringLiteral("Sin repositorio de seriales")}};
    const auto r = m_serials->setStatus(serial, QStringLiteral("rma"), notes);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    return {{"ok", true}};
}
