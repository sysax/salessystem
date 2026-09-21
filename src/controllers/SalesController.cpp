#include "SalesController.h"

SalesController::SalesController(SaleRepository *sales, SalesService *service, QObject *parent)
    : QObject(parent), m_repos(sales), m_service(service)
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
    for (const SaleItem &it : m_repos->itemsFor(id))
        items << QVariantMap{{"productId", it.productId}, {"qty", it.qty}, {"subtotal", it.subtotal}};
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
