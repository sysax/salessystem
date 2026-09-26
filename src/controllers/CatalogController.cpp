#include "CatalogController.h"

#include <QSet>

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
// Multitienda: la categoría debe pertenecer al rubro activo (o ser global '';
// 'miscelanea' permite todo). Evita guardar "Equipos" en modo abarrotes.
QString checkVertical(CategoryRepository *cats, const SettingsService *settings, Product &p)
{
    if (!settings)
        return {};
    QString bt = settings->businessType().trimmed();
    if (bt.isEmpty() || bt == QLatin1String("miscelanea"))
        return {};
    // Etiqueta explícita de otro rubro → rechazo directo (filtrar sin borrar).
    if (!p.businessType.trimmed().isEmpty() && p.businessType.trimmed() != bt)
        return QStringLiteral("Producto de '%1': el rubro activo es '%2'")
            .arg(p.businessType.trimmed(), bt);
    p.businessType = bt;
    if (!cats || p.cat.trimmed().isEmpty())
        return {};
    bool hasBtCats = false;
    QSet<QString> valid;
    for (const Category &c : cats->list(bt)) {
        hasBtCats = true;
        valid.insert(c.name.trimmed().toLower());
    }
    if (!hasBtCats)
        return {};
    if (!valid.contains(p.cat.trimmed().toLower()))
        return QStringLiteral("Categoría '%1' no es del rubro '%2' (use las de la lista)")
            .arg(p.cat.trimmed(), bt);
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
            {"businessType", p.businessType},
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
            {"reserved", p.reserved},
            {"available", p.available()},
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
    if (m.contains(QStringLiteral("businessType")))
        p.businessType = m[QStringLiteral("businessType")].toString().trimmed();
    if (m.contains(QStringLiteral("business_type")))
        p.businessType = m[QStringLiteral("business_type")].toString().trimmed();
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

void CatalogController::search(const QString &text, const QString &businessType)
{
    // Multitienda: si se pasa bt explícito manda; si no, usa el rubro activo
    // (salvo 'miscelanea'/vacío = ver todo, modo mixto intencional).
    QString bt = businessType.trimmed();
    if (bt.isEmpty() && m_settings)
        bt = m_settings->businessType().trimmed();
    if (bt == QLatin1String("miscelanea"))
        bt.clear();
    m_products.clear();
    for (const Product &p : m_repos->search(text, bt))
        m_products << toMap(p);
    m_totalCount = m_products.size();
    emit productsChanged();
}

void CatalogController::searchPaged(const QString &text, const QString &businessType, int page,
                                    int pageSize)
{
    // Fase 4: página servidor; pageSize <= 0 equivale a search().
    QString bt = businessType.trimmed();
    if (bt.isEmpty() && m_settings)
        bt = m_settings->businessType().trimmed();
    if (bt == QLatin1String("miscelanea"))
        bt.clear();
    m_totalCount = m_repos->countSearch(text, bt);
    if (pageSize <= 0) {
        search(text, businessType);
        return;
    }
    m_products.clear();
    for (const Product &p : m_repos->searchPaged(text, bt, pageSize, qMax(0, page) * pageSize))
        m_products << toMap(p);
    emit productsChanged();
}

QVariantMap CatalogController::add(const QVariantMap &fields)
{
    Product p = fromMap(fields);
    // Multitienda: auto-etiquetar con el rubro activo si no se indicó.
    if (p.businessType.trimmed().isEmpty() && m_settings) {
        const QString bt = m_settings->businessType().trimmed();
        if (!bt.isEmpty() && bt != QLatin1String("miscelanea"))
            p.businessType = bt;
    }
    if (const QString err = checkExpiry(m_settings, p); !err.isEmpty())
        return {{"ok", false}, {"error", err}};
    if (const QString verr = checkVertical(m_cats, m_settings, p); !verr.isEmpty())
        return {{"ok", false}, {"error", verr}};
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
    Product p = fromMap(fields, *cur);
    if (const QString err = checkExpiry(m_settings, p); !err.isEmpty())
        return {{"ok", false}, {"error", err}};
    if (const QString verr = checkVertical(m_cats, m_settings, p); !verr.isEmpty())
        return {{"ok", false}, {"error", verr}};
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

QVariantMap CatalogController::visibilityPreview(const QString &businessType) const
{
    // Multitienda: contar sin modificar nada (filtrar sin borrar).
    const QString bt = businessType.trimmed();
    const bool showAll = bt.isEmpty() || bt == QLatin1String("miscelanea");
    const int totalProducts = m_repos ? m_repos->search({}).size() : 0;
    const int visibleProducts = m_repos ? m_repos->search({}, showAll ? QString() : bt).size() : 0;
    int visibleCategories = 0, totalCategories = 0;
    if (m_cats) {
        totalCategories = m_cats->list().size();
        visibleCategories = m_cats->list(showAll ? QString() : bt).size();
    }
    QStringList hiddenSamples;
    if (m_repos && !showAll) {
        for (const Product &p : m_repos->search({})) {
            const QString pb = p.businessType.trimmed();
            if (!pb.isEmpty() && pb != QLatin1String("miscelanea") && pb != bt) {
                hiddenSamples << p.name;
                if (hiddenSamples.size() >= 5)
                    break;
            }
        }
    }
    return {{"visibleProducts", visibleProducts},
            {"hiddenProducts", totalProducts - visibleProducts},
            {"visibleCategories", visibleCategories},
            {"hiddenCategories", totalCategories - visibleCategories},
            {"hiddenSamples", hiddenSamples}};
}
