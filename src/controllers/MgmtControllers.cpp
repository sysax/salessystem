#include "MgmtControllers.h"

#include <cmath>

// ── Clients ───────────────────────────────────────────────────────────────

namespace
{
// Frontera QML: QML solo maneja double; el dominio usa Money (céntimos).
// Entradas double → Money::fromCop + validación fail-closed (finito,
// no negativo; >2 decimales por redondeo half-up en fromCop).
bool checkCopInput(double cop, QString &error)
{
    if (!std::isfinite(cop)) {
        error = QStringLiteral("Importe inválido");
        return false;
    }
    if (cop < 0.0) {
        error = QStringLiteral("El importe no puede ser negativo");
        return false;
    }
    return true;
}
} // namespace

ClientsController::ClientsController(ClientRepository *clients, ReceivablesService *cxc,
                                     QObject *parent)
    : QObject(parent), m_repos(clients), m_cxc(cxc), m_clientModel(this)
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
            {"credit", c.credit.toCop()},
            {"creditLimit", c.creditLimit.toCop()},
            {"discount", c.discount},
            {"balance", c.balance.toCop()},
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
        c.creditLimit = Money::fromCop(m[QStringLiteral("creditLimit")].toDouble());
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
    m_lastText = text;
    m_pagedActive = false;
    m_modelActive = false;
    m_clients.clear();
    for (const Client &c : m_repos->search(text))
        m_clients << toMap(c);
    m_totalCount = m_clients.size();
    emit clientsChanged();
}

void ClientsController::searchPaged(const QString &text, int page, int pageSize)
{
    // Fase 4: página servidor; pageSize <= 0 equivale a search().
    m_lastText = text;
    m_lastPage = page;
    m_lastSize = pageSize;
    m_pagedActive = true;
    m_modelActive = false;
    m_totalCount = m_repos->countSearch(text);
    if (pageSize <= 0) {
        search(text);
        return;
    }
    m_clients.clear();
    for (const Client &c : m_repos->searchPaged(text, pageSize, qMax(0, page) * pageSize))
        m_clients << toMap(c);
    emit clientsChanged();
}

void ClientsController::reloadClients()
{
    if (m_modelActive) {
        searchClients(m_modelText);
        return;
    }
    if (m_pagedActive)
        searchPaged(m_lastText, m_lastPage, m_lastSize);
    else
        search(m_lastText);
}

void ClientsController::searchClients(const QString &text)
{
    // Fase 4: reinicia el scroll infinito (página 0 al modelo).
    m_modelActive = true;
    m_modelPage = 0;
    m_totalCount = m_repos->countSearch(text);
    m_clientModel.setTotalCount(m_totalCount);
    QVariantList rows;
    for (const Client &c : m_repos->searchPaged(text, ModelPageSize, 0))
        rows << toMap(c);
    m_clientModel.setRows(rows);
    m_modelPage = 1;
    emit clientsChanged();
}

void ClientsController::fetchMoreClients()
{
    // Fase 4: anexa el siguiente lote si el servidor tiene más.
    if (!m_clientModel.canFetchMore())
        return;
    QVariantList rows;
    for (const Client &c :
         m_repos->searchPaged(m_modelText, ModelPageSize, m_modelPage * ModelPageSize))
        rows << toMap(c);
    if (rows.isEmpty()) {
        m_clientModel.setTotalCount(m_clientModel.rowCount());
        return;
    }
    m_clientModel.appendRows(rows);
    ++m_modelPage;
}

QVariantMap ClientsController::add(const QVariantMap &fields)
{
    const auto r = m_repos->add(fromMap(fields));
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    reloadClients();
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
    reloadClients();
    return {{"ok", true}};
}

QVariantMap ClientsController::remove(int id)
{
    const auto r = m_repos->remove(id);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    reloadClients();
    return {{"ok", true}};
}

QVariantList ClientsController::statement(const QString &name) const
{
    return m_cxc->statement(name);
}

QVariantMap ClientsController::pay(const QString &saleId, double amount, const QString &method,
                                   const QString &user)
{
    QString err;
    if (!checkCopInput(amount, err))
        return {{"ok", false}, {"error", QStringLiteral("Abono: ") + err}};
    const auto r = m_cxc->pay(saleId, Money::fromCop(amount), method, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    return {{"ok", true}, {"balance", r.value().balance.toCop()}};
}

// ── Suppliers ─────────────────────────────────────────────────────────────

SuppliersController::SuppliersController(SupplierRepository *suppliers, QObject *parent)
    : QObject(parent), m_repos(suppliers)
{
    search({});
}

QVariantMap SuppliersController::toMap(const Supplier &s)
{
    return {{"id", s.id},       {"name", s.name},   {"nit", s.nit},   {"contact", s.contact},
            {"phone", s.phone}, {"email", s.email}, {"city", s.city}, {"balance", s.balance.toCop()}};
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
