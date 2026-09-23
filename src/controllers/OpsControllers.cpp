#include "OpsControllers.h"

// ── Inventory ─────────────────────────────────────────────────────────────

InventoryController::InventoryController(InventoryService *service,
                                         InventoryRepository *inventory,
                                         ProductRepository *products, QObject *parent)
    : QObject(parent), m_service(service), m_inventory(inventory), m_products(products)
{
    refresh();
}

void InventoryController::refresh()
{
    m_movements.clear();
    for (const InventoryMovement &m : m_inventory->movements(50)) {
        m_movements << QVariantMap{{"ts", m.ts},
                                   {"sku", m.sku},
                                   {"type", m.type},
                                   {"qty", m.qty},
                                   {"before", m.before},
                                   {"after", m.after},
                                   {"reason", m.reason}};
    }
    QVariantList low, excess, out;
    for (const Product &p : m_inventory->belowMin())
        low << QVariantMap{{"sku", p.sku}, {"name", p.name}, {"stock", p.stock}};
    for (const Product &p : m_inventory->aboveMax())
        excess << QVariantMap{{"sku", p.sku}, {"name", p.name}, {"stock", p.stock}};
    for (const Product &p : m_inventory->outOfStock())
        out << QVariantMap{{"sku", p.sku}, {"name", p.name}};
    m_alerts = {{"low", low}, {"excess", excess}, {"out", out}};
    emit movementsChanged();
}

QVariantMap InventoryController::adjust(const QString &sku, double delta, const QString &reason,
                                        const QString &user)
{
    const auto r = m_service->registerAdjustment(sku, delta, reason, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}, {"newStock", r.value().newStock}};
}

QVariantMap InventoryController::transfer(const QString &sku, double qty, const QString &to,
                                          const QString &reason, const QString &user)
{
    const auto r = m_service->transfer(sku, qty, to, reason, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}};
}

QVariantMap InventoryController::valuation() const
{
    const auto v = m_service->valuation();
    return {{"totalValue", v.totalValue}, {"productsCount", v.productsCount}};
}

// ── Purchases ─────────────────────────────────────────────────────────────

PurchasesController::PurchasesController(PurchaseService *service, PurchaseRepository *repos,
                                         QObject *parent)
    : QObject(parent), m_service(service), m_repos(repos)
{
    refresh();
}

QVariantMap PurchasesController::toMap(const Purchase &p)
{
    return {{"id", p.id},
            {"date", p.date},
            {"supplier", p.supplier},
            {"total", p.total},
            {"status", p.status},
            {"notes", p.notes}};
}

void PurchasesController::refresh()
{
    m_orders.clear();
    for (const Purchase &p : m_repos->list())
        m_orders << toMap(p);
    emit ordersChanged();
}

QVariantMap PurchasesController::create(const QString &supplier, const QString &sku, double qty,
                                        const QString &user)
{
    const auto r = m_service->create(supplier, sku, qty, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}, {"id", r.value().id}};
}

QVariantMap PurchasesController::receive(const QString &folio, const QString &user)
{
    const auto r = m_service->receive(folio, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}};
}

QVariantMap PurchasesController::cancel(const QString &folio, const QString &user)
{
    const auto r = m_service->cancel(folio, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}};
}

// ── Receivables / Payables ────────────────────────────────────────────────

ReceivablesController::ReceivablesController(ReceivablesService *service, QObject *parent)
    : QObject(parent), m_service(service)
{
    refresh();
}

void ReceivablesController::refresh()
{
    m_pending = m_service->pending();
    emit pendingChanged();
}

QVariantList ReceivablesController::statement(const QString &client) const
{
    return m_service->statement(client);
}

QVariantMap ReceivablesController::pay(const QString &saleId, double amount,
                                       const QString &method, const QString &user)
{
    const auto r = m_service->pay(saleId, amount, method, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}, {"balance", r.value().balance}};
}

PayablesController::PayablesController(PayablesService *service, QObject *parent)
    : QObject(parent), m_service(service)
{
    refresh();
}

void PayablesController::refresh()
{
    m_pending = m_service->pending();
    emit pendingChanged();
}

QVariantMap PayablesController::pay(const QString &id, double amount, const QString &method,
                                    const QString &user)
{
    const auto r = m_service->pay(id, amount, method, user);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true},
            {"balance", r.value().payable.balance},
            {"earlyDiscount", r.value().earlyDiscount}};
}

// ── Promos ────────────────────────────────────────────────────────────────

PromosController::PromosController(PromoRepository *promos, QObject *parent)
    : QObject(parent), m_repos(promos)
{
    refresh();
}

QVariantMap PromosController::toMap(const Promo &p)
{
    return {{"id", p.id},
            {"name", p.name},
            {"type", p.type},
            {"value", p.value},
            {"condition", p.condition},
            {"code", p.code},
            {"active", p.active},
            {"desc", p.desc}};
}

void PromosController::refresh()
{
    m_promos.clear();
    for (const Promo &p : m_repos->list())
        m_promos << toMap(p);
    emit promosChanged();
}

QVariantMap PromosController::add(const QVariantMap &fields)
{
    Promo p;
    p.name = fields.value(QStringLiteral("name")).toString();
    p.type = fields.value(QStringLiteral("type"), QStringLiteral("porcentaje")).toString();
    p.value = fields.value(QStringLiteral("value")).toDouble();
    p.condition = fields.value(QStringLiteral("condition")).toString();
    p.code = fields.value(QStringLiteral("code")).toString();
    p.active = fields.value(QStringLiteral("active"), true).toBool();
    p.desc = fields.value(QStringLiteral("desc")).toString();
    const auto r = m_repos->add(p);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}};
}

QVariantMap PromosController::setActive(int id, bool active)
{
    const auto cur = m_repos->findById(id);
    if (!cur)
        return {{"ok", false}, {"error", QStringLiteral("No encontrada")}};
    Promo p = *cur;
    p.active = active;
    const auto r = m_repos->update(id, p);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}};
}

QVariantMap PromosController::remove(int id)
{
    const auto r = m_repos->remove(id);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}};
}

// ── Users ─────────────────────────────────────────────────────────────────

UsersController::UsersController(AuthService *auth, QObject *parent)
    : QObject(parent), m_auth(auth)
{
    refresh();
}

void UsersController::refresh()
{
    m_users.clear();
    for (const auto &u : m_auth->listUsers()) {
        m_users << QVariantMap{{"username", u.username},
                               {"role", u.role},
                               {"active", u.active},
                               {"totpEnabled", u.totpEnabled},
                               {"lastLogin", u.lastLogin}};
    }
    emit usersChanged();
}

QVariantMap UsersController::add(const QString &username, const QString &password,
                                 const QString &role)
{
    const auto r = m_auth->addUser(username, password, role);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}};
}

QVariantMap UsersController::setActive(const QString &username, bool active)
{
    const auto r = m_auth->setUserActive(username, active);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}};
}

QVariantMap UsersController::resetPassword(const QString &username, const QString &password)
{
    const auto r = m_auth->resetPassword(username, password);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    return {{"ok", true}};
}

QVariantMap UsersController::remove(const QString &username)
{
    const auto r = m_auth->deleteUser(username);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}};
}

QVariantMap UsersController::enable2fa(const QString &username)
{
    const auto r = m_auth->enable2fa(username);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}, {"secret", r.value()}};
}

QVariantMap UsersController::confirm2fa(const QString &username, const QString &code)
{
    const auto r = m_auth->confirm2fa(username, code);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}, {"recoveryCodes", r.value()}};
}

QVariantMap UsersController::disable2fa(const QString &username)
{
    const auto r = m_auth->disable2fa(username);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    refresh();
    return {{"ok", true}};
}
