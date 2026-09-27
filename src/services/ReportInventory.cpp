#include "ReportInventory.h"

#include <QDate>
#include <QMap>
#include <QSqlQuery>

#include <algorithm>

#include "../core/Money.h"
#include "../domain/Attrs.h"
#include "ReportCommon.h"

ReportInventory::ReportInventory(QSqlDatabase db, SettingsService *settings, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_settings(settings)
{
}

QString ReportInventory::effectiveBt(const QString &businessType) const
{
    return ReportCommon::effectiveBt(m_settings, businessType);
}

QVariantList ReportInventory::expiringProducts(int days, const QString &businessType) const
{
    QVariantList out;
    const QString bt = effectiveBt(businessType);
    const QString limit = QDate::currentDate().addDays(days > 0 ? days : 30).toString(Qt::ISODate);
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        q.prepare(QStringLiteral(
            "SELECT sku, name, lote, vencimiento, stock FROM products "
            "WHERE vencimiento IS NOT NULL AND vencimiento != '' AND vencimiento <= ? "
            "ORDER BY vencimiento"));
        q.addBindValue(limit);
    } else {
        q.prepare(QStringLiteral(
            "SELECT sku, name, lote, vencimiento, stock FROM products "
            "WHERE vencimiento IS NOT NULL AND vencimiento != '' AND vencimiento <= ? "
            "AND (business_type IS NULL OR business_type='' OR business_type=?) "
            "ORDER BY vencimiento"));
        q.addBindValue(limit);
        q.addBindValue(bt);
    }
    if (!q.exec())
        return out;
    while (q.next()) {
        out << QVariantMap{{"sku", q.value(0).toString()},
                           {"name", q.value(1).toString()},
                           {"lote", q.value(2).toString()},
                           {"vencimiento", q.value(3).toString()},
                           {"stock", q.value(4).toDouble()}};
    }
    return out;
}

QVariantMap ReportInventory::serialsReport(const QString &businessType) const
{
    // Fase 4: conteo por estado + detalle (para celulares/taller).
    const QString bt = effectiveBt(businessType);
    QVariantMap counts{{"in_stock", 0}, {"sold", 0}, {"rma", 0}, {"repaired", 0}};
    QVariantList items;
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        if (!q.exec(QStringLiteral(
                "SELECT s.serial, s.sku, s.status, s.sale_id, p.name FROM serials s "
                "LEFT JOIN products p ON p.sku = s.sku ORDER BY s.id DESC LIMIT 500")))
            return {{"counts", counts}, {"items", items}};
    } else {
        q.prepare(QStringLiteral(
            "SELECT s.serial, s.sku, s.status, s.sale_id, p.name FROM serials s "
            "JOIN products p ON p.sku = s.sku WHERE (p.business_type IS NULL OR "
            "p.business_type='' OR p.business_type=?) ORDER BY s.id DESC LIMIT 500"));
        q.addBindValue(bt);
        if (!q.exec())
            return {{"counts", counts}, {"items", items}};
    }
    int inStock = 0, sold = 0, rma = 0, repaired = 0;
    while (q.next()) {
        const QString st = q.value(2).toString();
        if (st == QLatin1String("sold"))
            ++sold;
        else if (st == QLatin1String("rma"))
            ++rma;
        else if (st == QLatin1String("repaired"))
            ++repaired;
        else
            ++inStock;
        items << QVariantMap{{"serial", q.value(0).toString()},
                             {"sku", q.value(1).toString()},
                             {"status", st},
                             {"saleId", q.value(3).toString()},
                             {"product", q.value(4).toString()}};
    }
    counts["in_stock"] = inStock;
    counts["sold"] = sold;
    counts["rma"] = rma;
    counts["repaired"] = repaired;
    return {{"counts", counts}, {"items", items}};
}

QVariantList ReportInventory::wasteReport(const QString &businessType) const
{
    // Fase 4: mermas por producto (cantidad + costo).
    const QString bt = effectiveBt(businessType);
    QVariantList out;
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        if (!q.exec(QStringLiteral(
                "SELECT m.sku, m.product, SUM(-m.qty), p.price_buy FROM inventory_movements m "
                "LEFT JOIN products p ON p.sku = m.sku WHERE m.type='Merma' "
                "GROUP BY m.sku ORDER BY SUM(-m.qty) DESC")))
            return out;
    } else {
        q.prepare(QStringLiteral(
            "SELECT m.sku, m.product, SUM(-m.qty), p.price_buy FROM inventory_movements m "
            "JOIN products p ON p.sku = m.sku WHERE m.type='Merma' AND (p.business_type IS NULL OR "
            "p.business_type='' OR p.business_type=?) "
            "GROUP BY m.sku ORDER BY SUM(-m.qty) DESC"));
        q.addBindValue(bt);
        if (!q.exec())
            return out;
    }
    while (q.next()) {
        const double qty = q.value(2).toDouble();
        Money cost = Money::fromCop(q.value(3).toDouble());
        if (!cost.isPositive())
            cost = Money();
        out << QVariantMap{{"sku", q.value(0).toString()},
                           {"product", q.value(1).toString()},
                           {"qty", qty},
                           {"cost", (cost * qty).toCop()}};
    }
    return out;
}

QVariantList ReportInventory::rotationByCategory(const QString &businessType) const
{
    // Fase 5 (genérico): vendidos e ingreso por categoría vs stock (rotación).
    const QString bt = effectiveBt(businessType);
    QMap<QString, QVariantMap> sold;
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        if (q.exec(QStringLiteral("SELECT p.cat, SUM(si.qty), SUM(si.subtotal) FROM sale_items si "
                                  "JOIN sales s ON si.sale_id=s.id "
                                  "JOIN products p ON si.product_id=p.id "
                                  "WHERE s.status!='Cancelada' GROUP BY p.cat"))) {
            while (q.next()) {
                sold[ReportCommon::normCat(q.value(0).toString())]
                    = {{"sold", q.value(1).toDouble()},
                       {"revenue", Money::fromCop(q.value(2).toDouble()).toCop()}};
            }
        }
    } else {
        q.prepare(QStringLiteral("SELECT p.cat, SUM(si.qty), SUM(si.subtotal) FROM sale_items si "
                                 "JOIN sales s ON si.sale_id=s.id "
                                 "JOIN products p ON si.product_id=p.id "
                                 "WHERE s.status!='Cancelada' AND (p.business_type IS NULL OR "
                                 "p.business_type='' OR p.business_type=?) GROUP BY p.cat"));
        q.addBindValue(bt);
        if (q.exec()) {
            while (q.next()) {
                sold[ReportCommon::normCat(q.value(0).toString())]
                    = {{"sold", q.value(1).toDouble()},
                       {"revenue", Money::fromCop(q.value(2).toDouble()).toCop()}};
            }
        }
    }
    QVariantList out;
    QSqlQuery p(m_db);
    if (bt.isEmpty()) {
        if (!p.exec(QStringLiteral(
                "SELECT cat, COUNT(*), COALESCE(SUM(stock),0) FROM products GROUP BY cat")))
            return out;
    } else {
        p.prepare(QStringLiteral("SELECT cat, COUNT(*), COALESCE(SUM(stock),0) FROM products WHERE "
                                 "(business_type IS NULL OR business_type='' OR business_type=?) "
                                 "GROUP BY cat"));
        p.addBindValue(bt);
        if (!p.exec())
            return out;
    }
    while (p.next()) {
        const QString cat = ReportCommon::normCat(p.value(0).toString());
        const double units = sold.value(cat).value(QStringLiteral("sold"), 0.0).toDouble();
        const double stock = p.value(2).toDouble();
        out << QVariantMap{
            {"category", cat}, {"products", p.value(1).toInt()},
            {"sold", units},   {"revenue", sold.value(cat).value(QStringLiteral("revenue"), 0.0)},
            {"stock", stock},  {"rotation", stock > 1e-9 ? units / stock : units}};
    }
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["revenue"].toDouble() > b.toMap()["revenue"].toDouble();
    });
    return out;
}

QVariantMap ReportInventory::inventoryValue(const QString &businessType) const
{
    // Fase 5 (genérico): inventario valorizado a costo y a precio de venta.
    const QString bt = effectiveBt(businessType);
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        if (!q.exec(QStringLiteral("SELECT COALESCE(SUM(price_buy*stock),0), "
                                   "COALESCE(SUM(price*stock),0), COALESCE(SUM(stock),0), "
                                   "COUNT(*) FROM products"))
            || !q.next())
            return {{"cost", 0.0}, {"sale", 0.0}, {"units", 0.0}, {"items", 0}};
    } else {
        q.prepare(QStringLiteral("SELECT COALESCE(SUM(price_buy*stock),0), "
                                 "COALESCE(SUM(price*stock),0), COALESCE(SUM(stock),0), "
                                 "COUNT(*) FROM products WHERE (business_type IS NULL OR "
                                 "business_type='' OR business_type=?)"));
        q.addBindValue(bt);
        if (!q.exec() || !q.next())
            return {{"cost", 0.0}, {"sale", 0.0}, {"units", 0.0}, {"items", 0}};
    }
    return {{"cost", Money::fromCop(q.value(0).toDouble()).toCop()},
            {"sale", Money::fromCop(q.value(1).toDouble()).toCop()},
            {"units", q.value(2).toDouble()},
            {"items", q.value(3).toInt()}};
}

QVariantList ReportInventory::controlledSales(const QString &businessType) const
{
    // Fase 5 (farmacia): líneas vendidas de productos controlados.
    const QString bt = effectiveBt(businessType);
    QVariantList out;
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        if (!q.exec(QStringLiteral(
                "SELECT s.id, s.date, p.sku, p.name, si.qty FROM sale_items si "
                "JOIN sales s ON si.sale_id=s.id "
                "JOIN products p ON si.product_id=p.id "
                "WHERE s.status!='Cancelada' AND p.attrs_json LIKE '%\"controlled\":true%' "
                "ORDER BY s.date DESC LIMIT 500")))
            return out;
    } else {
        q.prepare(QStringLiteral(
            "SELECT s.id, s.date, p.sku, p.name, si.qty FROM sale_items si "
            "JOIN sales s ON si.sale_id=s.id "
            "JOIN products p ON si.product_id=p.id "
            "WHERE s.status!='Cancelada' AND p.attrs_json LIKE '%\"controlled\":true%' "
            "AND (p.business_type IS NULL OR p.business_type='' OR p.business_type=?) "
            "ORDER BY s.date DESC LIMIT 500"));
        q.addBindValue(bt);
        if (!q.exec())
            return out;
    }
    while (q.next()) {
        out << QVariantMap{{"saleId", q.value(0).toString()},
                           {"date", q.value(1).toString()},
                           {"sku", q.value(2).toString()},
                           {"product", q.value(3).toString()},
                           {"qty", q.value(4).toDouble()}};
    }
    return out;
}

QVariantList ReportInventory::warrantyOpen(const QString &businessType) const
{
    // Fase 5 (celulares): seriales vendidos con garantía vigente
    // (fecha venta + warranty_months del producto).
    const QString bt = effectiveBt(businessType);
    QVariantList out;
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        if (!q.exec(
                QStringLiteral("SELECT s.serial, s.sku, p.name, s.sale_id, sa.date, p.attrs_json "
                               "FROM serials s LEFT JOIN products p ON p.sku=s.sku "
                               "LEFT JOIN sales sa ON sa.id=s.sale_id "
                               "WHERE s.status='sold' ORDER BY sa.date DESC LIMIT 500")))
            return out;
    } else {
        q.prepare(QStringLiteral(
            "SELECT s.serial, s.sku, p.name, s.sale_id, sa.date, p.attrs_json "
            "FROM serials s JOIN products p ON p.sku=s.sku "
            "LEFT JOIN sales sa ON sa.id=s.sale_id "
            "WHERE s.status='sold' AND (p.business_type IS NULL OR p.business_type='' OR "
            "p.business_type=?) ORDER BY sa.date DESC LIMIT 500"));
        q.addBindValue(bt);
        if (!q.exec())
            return out;
    }
    const QDate today = QDate::currentDate();
    while (q.next()) {
        const QDate sold = QDate::fromString(q.value(4).toString(), Qt::ISODate);
        if (!sold.isValid())
            continue;
        const int months = Attrs::integer(q.value(5).toString(), Attrs::KWarrantyMonths, 12);
        const QDate expires = sold.addMonths(months > 0 ? months : 12);
        if (today > expires)
            continue;
        out << QVariantMap{
            {"serial", q.value(0).toString()},           {"sku", q.value(1).toString()},
            {"product", q.value(2).toString()},          {"saleId", q.value(3).toString()},
            {"saleDate", q.value(4).toString()},         {"warrantyMonths", months},
            {"expiresAt", expires.toString(Qt::ISODate)}};
    }
    return out;
}

QVariantList ReportInventory::bulkPerformance(const QString &businessType) const
{
    // Fase 5 (abarrotes): cantidad e ingreso agrupados por unidad de medida.
    const QString bt = effectiveBt(businessType);
    QVariantList out;
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        if (!q.exec(
                QStringLiteral("SELECT p.unit, SUM(si.qty), SUM(si.subtotal) FROM sale_items si "
                               "JOIN sales s ON si.sale_id=s.id "
                               "JOIN products p ON si.product_id=p.id "
                               "WHERE s.status!='Cancelada' GROUP BY p.unit "
                               "ORDER BY SUM(si.subtotal) DESC")))
            return out;
    } else {
        q.prepare(QStringLiteral("SELECT p.unit, SUM(si.qty), SUM(si.subtotal) FROM sale_items si "
                                 "JOIN sales s ON si.sale_id=s.id "
                                 "JOIN products p ON si.product_id=p.id "
                                 "WHERE s.status!='Cancelada' AND (p.business_type IS NULL OR "
                                 "p.business_type='' OR p.business_type=?) GROUP BY p.unit "
                                 "ORDER BY SUM(si.subtotal) DESC"));
        q.addBindValue(bt);
        if (!q.exec())
            return out;
    }
    while (q.next()) {
        QString unit = q.value(0).toString().trimmed();
        if (unit.isEmpty())
            unit = QStringLiteral("unidad");
        out << QVariantMap{{"unit", unit},
                           {"qty", q.value(1).toDouble()},
                           {"revenue", Money::fromCop(q.value(2).toDouble()).toCop()}};
    }
    return out;
}
