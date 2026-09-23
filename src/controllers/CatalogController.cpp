#include "CatalogController.h"

CatalogController::CatalogController(ProductRepository *products, CategoryRepository *categories,
                                       SettingsService *settings, QObject *parent)
    : QObject(parent), m_repos(products), m_cats(categories), m_settings(settings)
{
    search({});
    reloadCategories();
}

namespace
{
// Fase 3: con require_expiry activo, lote+vencimiento son obligatorios.
QString checkExpiry(const SettingsService *settings, const Product &p)
{
    if (settings && settings->requireExpiry()) {
        if (p.lote.trimmed().isEmpty() || p.vencimiento.trimmed().isEmpty())
            return QStringLiteral("Lote y vencimiento obligatorios (configuración farmacia)");
    }
    return {};
}
} // namespace

QVariantMap CatalogController::toMap(const Product &p)
{
    return {{"id", p.id},
            {"sku", p.sku},
            {"barcode", p.barcode},
            {"name", p.name},
            {"description", p.description},
            {"cat", p.cat},
            {"brand", p.brand},
            {"supplier", p.supplier},
            {"price", p.price},
            {"priceBuy", p.priceBuy},
            {"priceWholesale", p.priceWholesale},
            {"tax", p.tax},
            {"unit", p.unit},
            {"subcat", p.subcat},
            {"lote", p.lote},
            {"vencimiento", p.vencimiento},
            {"attrsJson", p.attrsJson},
            {"lote", p.lote},
            {"vencimiento", p.vencimiento},
            {"attrsJson", p.attrsJson},
            {"stock", p.stock},
            {"stockMin", p.stockMin},
            {"stockMax", p.stockMax},
            {"location", p.location},
            {"status", p.status}};
}

Product CatalogController::fromMap(const QVariantMap &m, const Product &base)
{
    Product p = base;
    if (m.contains(QStringLiteral("sku")))
        p.sku = m[QStringLiteral("sku")].toString();
    if (m.contains(QStringLiteral("name")))
        p.name = m[QStringLiteral("name")].toString();
    if (m.contains(QStringLiteral("description")))
        p.description = m[QStringLiteral("description")].toString();
    if (m.contains(QStringLiteral("cat")))
        p.cat = m[QStringLiteral("cat")].toString();
    if (m.contains(QStringLiteral("subcat")))
        p.subcat = m[QStringLiteral("subcat")].toString();
    if (m.contains(QStringLiteral("unit")))
        p.unit = m[QStringLiteral("unit")].toString();
    if (m.contains(QStringLiteral("lote")))
        p.lote = m[QStringLiteral("lote")].toString();
    if (m.contains(QStringLiteral("vencimiento")))
        p.vencimiento = m[QStringLiteral("vencimiento")].toString();
    if (m.contains(QStringLiteral("attrsJson")))
        p.attrsJson = m[QStringLiteral("attrsJson")].toString();
    if (m.contains(QStringLiteral("brand")))
        p.brand = m[QStringLiteral("brand")].toString();
    if (m.contains(QStringLiteral("supplier")))
        p.supplier = m[QStringLiteral("supplier")].toString();
    if (m.contains(QStringLiteral("price")))
        p.price = m[QStringLiteral("price")].toDouble();
    if (m.contains(QStringLiteral("priceBuy")))
        p.priceBuy = m[QStringLiteral("priceBuy")].toDouble();
    if (m.contains(QStringLiteral("tax")))
        p.tax = m[QStringLiteral("tax")].toString();
    if (m.contains(QStringLiteral("stock")))
        p.stock = m[QStringLiteral("stock")].toDouble();
    if (m.contains(QStringLiteral("stockMin")))
        p.stockMin = m[QStringLiteral("stockMin")].toDouble();
    if (m.contains(QStringLiteral("stockMax")))
        p.stockMax = m[QStringLiteral("stockMax")].toDouble();
    if (m.contains(QStringLiteral("location")))
        p.location = m[QStringLiteral("location")].toString();
    if (m.contains(QStringLiteral("status")))
        p.status = m[QStringLiteral("status")].toString();
    if (m.contains(QStringLiteral("barcode")))
        p.barcode = m[QStringLiteral("barcode")].toString();
    return p;
}

void CatalogController::search(const QString &text)
{
    m_products.clear();
    for (const Product &p : m_repos->search(text))
        m_products << toMap(p);
    emit productsChanged();
}

QVariantMap CatalogController::add(const QVariantMap &fields)
{
    const Product p = fromMap(fields);
    if (const QString err = checkExpiry(m_settings, p); !err.isEmpty())
        return {{"ok", false}, {"error", err}};
    const auto r = m_repos->add(p);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    search({});
    return {{"ok", true}};
}

QVariantMap CatalogController::update(const QString &sku, const QVariantMap &fields)
{
    const auto cur = m_repos->findBySku(sku);
    if (!cur)
        return {{"ok", false}, {"error", QStringLiteral("No encontrado")}};
    const Product p = fromMap(fields, *cur);
    if (const QString err = checkExpiry(m_settings, p); !err.isEmpty())
        return {{"ok", false}, {"error", err}};
    const auto r = m_repos->update(sku, p);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    search({});
    return {{"ok", true}};
}

QVariantMap CatalogController::remove(const QString &sku)
{
    const auto r = m_repos->remove(sku);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    search({});
    return {{"ok", true}};
}

QVariantMap CatalogController::categoryToMap(const Category &c)
{
    return {{"id", c.id},
            {"name", c.name},
            {"parentId", c.parentId},
            {"businessType", c.businessType},
            {"sortOrder", c.sortOrder}};
}

void CatalogController::reloadCategories(const QString &businessType)
{
    m_catFilter = businessType;
    m_categories.clear();
    if (m_cats) {
        for (const Category &c : m_cats->list(businessType))
            m_categories << categoryToMap(c);
    }
    emit categoriesChanged();
}

QVariantMap CatalogController::addCategory(const QString &name, int parentId,
                                          const QString &businessType)
{
    if (!m_cats)
        return {{"ok", false}, {"error", QStringLiteral("Sin repositorio de categorías")}};
    const auto r = m_cats->add(name, parentId, businessType);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    reloadCategories(m_catFilter);
    return {{"ok", true}, {"id", r.value().id}};
}

QVariantMap CatalogController::removeCategory(int id)
{
    if (!m_cats)
        return {{"ok", false}, {"error", QStringLiteral("Sin repositorio de categorías")}};
    const auto r = m_cats->remove(id);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    reloadCategories(m_catFilter);
    return {{"ok", true}};
}
