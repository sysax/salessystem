#include "CatalogController.h"

CatalogController::CatalogController(ProductRepository *products, QObject *parent)
    : QObject(parent), m_repos(products)
{
    search({});
}

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
    if (m.contains(QStringLiteral("brand")))
        p.brand = m[QStringLiteral("brand")].toString();
    if (m.contains(QStringLiteral("supplier")))
        p.supplier = m[QStringLiteral("supplier")].toString();
    if (m.contains(QStringLiteral("price")))
        p.price = m[QStringLiteral("price")].toDouble();
    if (m.contains(QStringLiteral("priceBuy")))
        p.priceBuy = m[QStringLiteral("priceBuy")].toDouble();
    if (m.contains(QStringLiteral("stock")))
        p.stock = m[QStringLiteral("stock")].toInt();
    if (m.contains(QStringLiteral("stockMin")))
        p.stockMin = m[QStringLiteral("stockMin")].toInt();
    if (m.contains(QStringLiteral("stockMax")))
        p.stockMax = m[QStringLiteral("stockMax")].toInt();
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
    const auto r = m_repos->add(fromMap(fields));
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
    const auto r = m_repos->update(sku, fromMap(fields, *cur));
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
