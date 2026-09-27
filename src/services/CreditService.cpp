#include "CreditService.h"

#include <QDate>

#include "../core/EventBus.h"
#include "../repositories/SaleRepository.h"
#include "../services/SettingsService.h"

namespace
{
QVariantMap saleToMap(const Sale &s, double moraRate)
{
    return {{"id", s.id},
            {"date", s.date},
            {"client", s.client},
            {"total", s.total.toCop()},
            {"paid", s.paid.toCop()},
            {"balance", s.balance.toCop()},
            {"status", s.status},
            {"due", s.due},
            {"mora", ReceivablesRepository::mora(s, moraRate).toCop()}};
}
QVariantMap payableToMap(const Payable &p)
{
    const QDate due = QDate::fromString(p.due, Qt::ISODate);
    int overdueDays = 0;
    if (due.isValid() && due < QDate::currentDate() && p.balance.isPositive())
        overdueDays = due.daysTo(QDate::currentDate());
    return {{"id", p.id},
            {"supplier", p.supplier},
            {"due", p.due},
            {"amount", p.amount.toCop()},
            {"paid", p.paid.toCop()},
            {"balance", p.balance.toCop()},
            {"discountEarly", p.discountEarly.toCop()},
            {"overdueDays", overdueDays},
            {"status", p.status}};
}
} // namespace

ReceivablesService::ReceivablesService(ReceivablesRepository *repos, EventBus *bus,
                                         QObject *parent, SettingsService *settings)
    : QObject(parent), m_repos(repos), m_bus(bus), m_settings(settings)
{
}

double ReceivablesService::moraMonthlyRate() const
{
    // Fase 3: settings guarda porciento ("2" = 2 %); el repo usa fracción.
    return m_settings ? m_settings->moraRate() / 100.0 : ReceivablesRepository::MoraRate;
}

QVariantList ReceivablesService::pending() const
{
    QVariantList out;
    const double rate = moraMonthlyRate();
    for (const Sale &s : m_repos->pending())
        out << saleToMap(s, rate);
    return out;
}

QVariantList ReceivablesService::statement(const QString &client) const
{
    QVariantList out;
    const double rate = moraMonthlyRate();
    for (const Sale &s : m_repos->statement(client))
        out << saleToMap(s, rate);
    return out;
}

Result<Sale> ReceivablesService::pay(const QString &saleId, Money amount, const QString &method,
                                     const QString &user)
{
    auto r = m_repos->addPayment(saleId, amount, method, user);
    if (r.ok() && m_bus)
        m_bus->publish(EventBus::SaleCreated,
                       {{"sale_id", saleId}, {"payment", amount.toCop()}, {"type", "cxc"}});
    return r;
}

Money ReceivablesService::mora(const Sale &s)
{
    return ReceivablesRepository::mora(s);
}

PayablesService::PayablesService(PayablesRepository *repos, EventBus *bus, QObject *parent,
                                   SettingsService * /*settings*/)
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
                                                               Money amount, const QString &method,
                                                               const QString &user)
{
    return m_repos->addPayment(payableId, amount, method, user);
}
