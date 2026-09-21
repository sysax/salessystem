#include "CreditService.h"

#include "../core/EventBus.h"
#include "../repositories/SaleRepository.h"

namespace
{
QVariantMap saleToMap(const Sale &s)
{
    return {{"id", s.id},
            {"date", s.date},
            {"client", s.client},
            {"total", s.total},
            {"paid", s.paid},
            {"balance", s.balance},
            {"status", s.status},
            {"due", s.due},
            {"mora", ReceivablesRepository::mora(s)}};
}
} // namespace

ReceivablesService::ReceivablesService(ReceivablesRepository *repos, EventBus *bus,
                                       QObject *parent)
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

Result<Sale> ReceivablesService::pay(const QString &saleId, double amount,
                                     const QString &method, const QString &user)
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
    for (const Payable &p : m_repos->pending()) {
        out << QVariantMap{{"id", p.id},
                           {"supplier", p.supplier},
                           {"due", p.due},
                           {"amount", p.amount},
                           {"paid", p.paid},
                           {"balance", p.balance},
                           {"status", p.status}};
    }
    return out;
}

Result<PayablesRepository::PaymentResult> PayablesService::pay(const QString &payableId,
                                                               double amount,
                                                               const QString &method,
                                                               const QString &user)
{
    return m_repos->addPayment(payableId, amount, method, user);
}
