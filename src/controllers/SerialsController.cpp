#include "SerialsController.h"

SerialsController::SerialsController(SerialRepository *serials, ProductRepository *products,
                                     QObject *parent)
    : QObject(parent), m_repos(serials), m_products(products)
{
    search({});
}

QVariantMap SerialsController::toMap(const SerialInfo &s, const QString &productName)
{
    return {{"id", s.id},
            {"productId", s.productId},
            {"sku", s.sku},
            {"product", productName},
            {"serial", s.serial},
            {"status", s.status},
            {"saleId", s.saleId},
            {"imei2", s.imei2},
            {"notes", s.notes}};
}

QString productNameFor(const ProductRepository *repos, const QString &sku)
{
    if (!repos)
        return {};
    const auto p = repos->findBySku(sku);
    return p ? p->name : QString();
}

void SerialsController::search(const QString &text, const QString &status){
    m_serials.clear();
    if (!m_repos || !m_products) {
        emit serialsChanged();
        return;
    }
    const QString t = text.trimmed().toLower();
    const QString st = status.trimmed();
    if (!t.isEmpty()) {
        // Lookup exacto primero (cubre vendidos/RMA).
        if (const auto exact = m_repos->find(text.trimmed())) {
            if (st.isEmpty() || exact->status == st)
                m_serials << toMap(*exact, productNameFor(m_products, exact->sku));
        }
    }
    if (!st.isEmpty() && st != QLatin1String("in_stock")) {
        // Barrido global por estado (vendidos, RMA...).
        for (const SerialInfo &s : m_repos->byStatus(st)) {
            if (!t.isEmpty() && !s.serial.toLower().contains(t)
                && !s.sku.toLower().contains(t))
                continue;
            // Evitar duplicar el exacto ya agregado.
            bool dup = false;
            for (const QVariant &v : m_serials) {
                if (v.toMap()["serial"].toString() == s.serial) {
                    dup = true;
                    break;
                }
            }
            if (!dup)
                m_serials << toMap(s, productNameFor(m_products, s.sku));
        }
        emit serialsChanged();
        return;
    }
    // in_stock (o todo): barrido por productos tracked.
    for (const Product &p : m_products->list()) {
        if (!Attrs::boolean(p.attrsJson, Attrs::KTrackSerial) && !m_repos->hasSerials(p.sku))
            continue;
        for (const SerialInfo &s : m_repos->inStock(p.sku)) {
            if (!t.isEmpty() && !s.serial.toLower().contains(t)
                && !s.sku.toLower().contains(t))
                continue;
            m_serials << toMap(s, p.name);
        }
    }
    emit serialsChanged();
}

QVariantMap SerialsController::addSerial(const QString &sku, const QString &serial,
                                        const QString &imei2)
{
    if (!m_repos || !m_products)
        return {{"ok", false}, {"error", QStringLiteral("Sin repositorios")}};
    const auto p = m_products->findBySku(sku.trimmed());
    if (!p)
        return {{"ok", false}, {"error", QStringLiteral("SKU no existe")}};
    const auto r = m_repos->add(p->id, p->sku, serial, imei2);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    search({});
    return {{"ok", true}};
}

QVariantMap SerialsController::setStatus(const QString &serial, const QString &status,
                                        const QString &notes, const QString &user)
{
    Q_UNUSED(user);
    if (!m_repos)
        return {{"ok", false}, {"error", QStringLiteral("Sin repositorio")}};
    const auto r = m_repos->setStatus(serial, status, notes);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    search({});
    return {{"ok", true}};
}

QVariantMap SerialsController::warrantyFor(const QString &serial) const
{
    if (!m_repos || !m_products)
        return {{"ok", false}, {"error", QStringLiteral("Sin repositorios")}};
    const auto s = m_repos->find(serial);
    if (!s)
        return {{"ok", false}, {"error", QStringLiteral("Serial no registrado")}};
    int months = 12;
    if (const auto p = m_products->findBySku(s->sku))
        months = Attrs::integer(p->attrsJson, Attrs::KWarrantyMonths, 12);
    QVariantMap r = m_repos->warrantyStatus(serial, months);
    r["ok"] = r.value(QStringLiteral("error"), {}).toString().isEmpty();
    if (r["ok"].toBool())
        r["product"] = productNameFor(m_products, s->sku);
    return r;
}

QVariantList SerialsController::inStock(const QString &sku) const
{
    QVariantList out;
    if (!m_repos)
        return out;
    for (const SerialInfo &s : m_repos->inStock(sku.trimmed()))
        out << toMap(s, productNameFor(m_products, s.sku));
    return out;
}

int SerialsController::inStockCount(const QString &sku) const
{
    return m_repos ? m_repos->inStockCount(sku.trimmed()) : 0;
}
