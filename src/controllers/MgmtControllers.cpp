#include "MgmtControllers.h"

// ── Clients ───────────────────────────────────────────────────────────────

ClientsController::ClientsController(ClientRepository *clients, ReceivablesService *cxc,
                                     QObject *parent)
    : QObject(parent), m_repos(clients), m_cxc(cxc)
{
    search({});
}

QVariantMap ClientsController::toMap(const Client &c)
{
    return {{"id", c.id},
            {"name", c.name},
            {"nit", c.nit},
            {"email", c.email},
            {"phone", c.phone},
            {"city", c.city},
            {"credit", c.credit},
            {"creditLimit", c.creditLimit},
            {"discount", c.discount},
            {"balance", c.balance},
            {"priceList", c.priceList},
            {"status", c.status},
            {"regimen", c.regimen}};
}

Client ClientsController::fromMap(const QVariantMap &m, const Client &base)
{
    Client c = base;
    if (m.contains(QStringLiteral("name")))
        c.name = m[QStringLiteral("name")].toString();
    if (m.contains(QStringLiteral("nit")))
        c.nit = m[QStringLiteral("nit")].toString();
    if (m.contains(QStringLiteral("email")))
        c.email = m[QStringLiteral("email")].toString();
    if (m.contains(QStringLiteral("phone")))
        c.phone = m[QStringLiteral("phone")].toString();
    if (m.contains(QStringLiteral("city")))
        c.city = m[QStringLiteral("city")].toString();
    if (m.contains(QStringLiteral("creditLimit")))
        c.creditLimit = m[QStringLiteral("creditLimit")].toDouble();
    if (m.contains(QStringLiteral("discount")))
        c.discount = m[QStringLiteral("discount")].toInt();
    if (m.contains(QStringLiteral("status")))
        c.status = m[QStringLiteral("status")].toString();
    if (m.contains(QStringLiteral("priceList")))
        c.priceList = m[QStringLiteral("priceList")].toString();
    if (m.contains(QStringLiteral("regimen")))
        c.regimen = m[QStringLiteral("regimen")].toString();
    return c;
}

void ClientsController::search(const QString &text)
{
    m_clients.clear();
    for (const Client &c : m_repos->search(text))
        m_clients << toMap(c);
    emit clientsChanged();
}

QVariantMap ClientsController::add(const QVariantMap &fields)
{
    const auto r = m_repos->add(fromMap(fields));
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    search({});
    return {{"ok", true}};
}

QVariantMap ClientsController::update(int id, const QVariantMap &fields)
{
    const auto cur = m_repos->findById(id);
    if (!cur)
        return {{"ok", false}, {"error", QStringLiteral("No encontrado")}};
    const auto r = m_repos->update(id, fromMap(fields, *cur));
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    search({});
    return {{"ok", true}};
}

QVariantMap ClientsController::remove(int id)
{
    const auto r = m_repos->remove(id);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    search({});
    return {{"ok", true}};
}

QVariantList ClientsController::statement(const QString &name) const
{
    return m_cxc->statement(name);
}

QVariantMap ClientsController::pay(const QString &saleId, double amount,
                                   const QString &method, const QString &user)
{
    const auto r = m_cxc->pay(saleId, amount, method, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    return {{"ok", true}, {"balance", r.value().balance}};
}

// ── Suppliers ─────────────────────────────────────────────────────────────

SuppliersController::SuppliersController(SupplierRepository *suppliers, QObject *parent)
    : QObject(parent), m_repos(suppliers)
{
    search({});
}

QVariantMap SuppliersController::toMap(const Supplier &s)
{
    return {{"id", s.id},
            {"name", s.name},
            {"nit", s.nit},
            {"contact", s.contact},
            {"phone", s.phone},
            {"email", s.email},
            {"city", s.city},
            {"balance", s.balance}};
}

Supplier SuppliersController::fromMap(const QVariantMap &m, const Supplier &base)
{
    Supplier s = base;
    if (m.contains(QStringLiteral("name")))
        s.name = m[QStringLiteral("name")].toString();
    if (m.contains(QStringLiteral("nit")))
        s.nit = m[QStringLiteral("nit")].toString();
    if (m.contains(QStringLiteral("contact")))
        s.contact = m[QStringLiteral("contact")].toString();
    if (m.contains(QStringLiteral("phone")))
        s.phone = m[QStringLiteral("phone")].toString();
    if (m.contains(QStringLiteral("email")))
        s.email = m[QStringLiteral("email")].toString();
    if (m.contains(QStringLiteral("city")))
        s.city = m[QStringLiteral("city")].toString();
    return s;
}

void SuppliersController::search(const QString &text)
{
    m_suppliers.clear();
    for (const Supplier &s : m_repos->search(text))
        m_suppliers << toMap(s);
    emit suppliersChanged();
}

QVariantMap SuppliersController::add(const QVariantMap &fields)
{
    const auto r = m_repos->add(fromMap(fields));
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    search({});
    return {{"ok", true}};
}

QVariantMap SuppliersController::update(int id, const QVariantMap &fields)
{
    const auto cur = m_repos->findById(id);
    if (!cur)
        return {{"ok", false}, {"error", QStringLiteral("No encontrado")}};
    const auto r = m_repos->update(id, fromMap(fields, *cur));
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    search({});
    return {{"ok", true}};
}

QVariantMap SuppliersController::remove(int id)
{
    const auto r = m_repos->remove(id);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    search({});
    return {{"ok", true}};
}
