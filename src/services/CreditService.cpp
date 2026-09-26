#include "CreditService.h"

#include <QDate>

#include "../core/EventBus.h"
#include "../repositories/SaleRepository.h"

namespace
{
QVariantMap saleToMap(const Sale &s)
{
    return {{"id", s.id},         {"date", s.date}, {"client", s.client},
            {"total", s.total},   {"paid", s.paid}, {"balance", s.balance},
            {"status", s.status}, {"due", s.due},   {"mora", ReceivablesRepository::mora(s)}};
}
QVariantMap payableToMap(const Payable &p)
{
    const QDate due = QDate::fromString(p.due, Qt::ISODate);
    int overdueDays = 0;
    if (due.isValid() && due < QDate::currentDate() && p.balance > 0)
        overdueDays = due.daysTo(QDate::currentDate());
    return {{"id", p.id},
            {"supplier", p.supplier},
            {"due", p.due},
            {"amount", p.amount},
            {"paid", p.paid},
            {"balance", p.balance},
            {"discountEarly", p.discountEarly},
            {"overdueDays", overdueDays},
            {"status", p.status}};
}
} // namespace

ReceivablesService::ReceivablesService(ReceivablesRepository *repos, EventBus *bus, QObject *parent)
    : QObject(parent), m_repos(repos), m_bus(bus)
{
}

QVariantList ReceivablesService::pending() const
{
    QVariantList out;
    for (const Sale &s : m_repos->pending())
        out << saleToMap(s);
    return out;
}

QVariantList ReceivablesService::statement(const QString &client) const
{
    QVariantList out;
    for (const Sale &s : m_repos->statement(client))
        out << saleToMap(s);
    return out;
}

Result<Sale> ReceivablesService::pay(const QString &saleId, double amount, const QString &method,
                                     const QString &user)
{
    auto r = m_repos->addPayment(saleId, amount, method, user);
    if (r.ok() && m_bus)
        m_bus->publish(EventBus::SaleCreated,
                       {{"sale_id", saleId}, {"payment", amount}, {"type", "cxc"}});
    return r;
}

double ReceivablesService::mora(const Sale &s)
{
    return ReceivablesRepository::mora(s);
}

PayablesService::PayablesService(PayablesRepository *repos, EventBus *bus, QObject *parent)
    : QObject(parent), m_repos(repos), m_bus(bus)
{
}

QVariantList PayablesService::pending() const
{
    QVariantList out;
    for (const Payable &p : m_repos->pending())
        out << payableToMap(p);
    return out;
}

QVariantList PayablesService::overdue() const
{
    QVariantList out;
    for (const Payable &p : m_repos->overdue())
        out << payableToMap(p);
    return out;
}

QVariantList PayablesService::statement(const QString &supplier) const
{
    QVariantList out;
    for (const Payable &p : m_repos->statement(supplier))
        out << payableToMap(p);
    return out;
}

Result<PayablesRepository::PaymentResult> PayablesService::pay(const QString &payableId,
                                                               double amount, const QString &method,
                                                               const QString &user)
{
    return m_repos->addPayment(payableId, amount, method, user);
}
