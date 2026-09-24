#include "ReportService.h"

#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPdfWriter>
#include <QPainter>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTextStream>

#include <algorithm>

#include "../domain/Attrs.h"

ReportService::ReportService(QSqlDatabase db, QObject *parent)
    : QObject(parent), m_db(std::move(db))
{
}

static double scalar(QSqlDatabase db, const QString &sql)
{
    QSqlQuery q(db);
    q.exec(sql);
    return (q.next() ? q.value(0).toDouble() : 0.0);
}

QVariantMap ReportService::stats() const
{
    return {
        {"totalSales", scalar(m_db, QStringLiteral("SELECT COALESCE(SUM(total),0) FROM sales WHERE status='Pagada'"))},
        {"totalProducts", scalar(m_db, QStringLiteral("SELECT COUNT(*) FROM products"))},
        {"totalClients", scalar(m_db, QStringLiteral("SELECT COUNT(*) FROM clients"))},
        {"pendingOrders", scalar(m_db, QStringLiteral("SELECT COUNT(*) FROM sales WHERE status='Pendiente'"))},
        {"lowStockAlerts", scalar(m_db, QStringLiteral("SELECT COUNT(*) FROM products WHERE stock < stock_min"))},
    };
}

QVariantList ReportService::topProducts(int n) const
{
    QVariantList out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT product_id, SUM(qty) FROM sale_items JOIN sales ON sale_items.sale_id=sales.id "
        "WHERE sales.status!='Cancelada' GROUP BY product_id ORDER BY SUM(qty) DESC LIMIT ?"));
    q.addBindValue(n);
    if (!q.exec())
        return out;
    QSet<int> seen;
    while (q.next()) {
        const int pid = q.value(0).toInt();
        seen.insert(pid);
        QSqlQuery p(m_db);
        p.prepare(QStringLiteral("SELECT name FROM products WHERE id=?"));
        p.addBindValue(pid);
        QString name;
        if (p.exec() && p.next())
            name = p.value(0).toString();
        out << QVariantMap{{"id", pid}, {"name", name}, {"sold", q.value(1).toDouble()}};
    }
    // Rellenar con cero vendidos hasta n (como en Python)
    if (out.size() < n) {
        QSqlQuery all(m_db);
        all.exec(QStringLiteral("SELECT id, name FROM products ORDER BY id"));
        while (all.next() && out.size() < n) {
            if (!seen.contains(all.value(0).toInt()))
                out << QVariantMap{{"id", all.value(0).toInt()},
                                   {"name", all.value(1).toString()},
                                   {"sold", 0}};
        }
    }
    return out;
}

QVariantList ReportService::leastSold(int n) const
{
    QMap<int, double> cnt;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT product_id, SUM(qty) FROM sale_items JOIN sales ON sale_items.sale_id=sales.id "
        "WHERE sales.status!='Cancelada' GROUP BY product_id"));
    while (q.next())
        cnt[q.value(0).toInt()] = q.value(1).toDouble();
    QSqlQuery all(m_db);
    all.exec(QStringLiteral("SELECT id, name FROM products ORDER BY id"));
    QList<QPair<int, QString>> prods;
    while (all.next()) {
        prods << qMakePair(all.value(0).toInt(), all.value(1).toString());
        if (!cnt.contains(all.value(0).toInt()))
            cnt[all.value(0).toInt()] = 0.0;
    }
    std::sort(prods.begin(), prods.end(),
              [&](const auto &a, const auto &b) { return cnt[a.first] < cnt[b.first]; });
    QVariantList out;
    for (int i = 0; i < std::min<qsizetype>(n, prods.size()); ++i)
        out << QVariantMap{{"id", prods[i].first}, {"name", prods[i].second}, {"sold", cnt[prods[i].first]}};
    return out;
}

QVariantList ReportService::topClients(int n) const
{
    QVariantList out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT client, COUNT(*), SUM(total) FROM sales WHERE status='Pagada' "
        "GROUP BY client ORDER BY COUNT(*) DESC LIMIT ?"));
    q.addBindValue(n);
    if (!q.exec())
        return out;
    while (q.next())
        out << QVariantMap{{"client", q.value(0).toString()},
                           {"orders", q.value(1).toInt()},
                           {"total", q.value(2).toDouble()}};
    return out;
}

QVariantList ReportService::topSellers(int n) const
{
    QVariantList out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT vendedor, COUNT(*), SUM(total) FROM sales WHERE status='Pagada' "
        "GROUP BY vendedor ORDER BY COUNT(*) DESC LIMIT ?"));
    q.addBindValue(n);
    if (!q.exec())
        return out;
    while (q.next())
        out << QVariantMap{{"seller", q.value(0).toString()},
                           {"sales", q.value(1).toInt()},
                           {"total", q.value(2).toDouble()}};
    return out;
}

QVariantList ReportService::marginPerProduct() const
{
    QMap<int, double> sold;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT product_id, SUM(qty) FROM sale_items JOIN sales ON sale_items.sale_id=sales.id "
        "WHERE sales.status='Pagada' GROUP BY product_id"));
    while (q.next())
        sold[q.value(0).toInt()] = q.value(1).toDouble();
    QVariantList out;
    QSqlQuery p(m_db);
    p.exec(QStringLiteral("SELECT id, sku, name, price, price_buy FROM products"));
    while (p.next()) {
        const int pid = p.value(0).toInt();
        const double price = p.value(3).toDouble();
        double cost = p.value(4).toDouble();
        if (cost <= 0)
            cost = price * 0.7;
        const double mu = price - cost;
        const double s = sold.value(pid, 0.0);
        out << QVariantMap{{"id", pid},
                           {"sku", p.value(1).toString()},
                           {"name", p.value(2).toString()},
                           {"sold", s},
                           {"price", price},
                           {"cost", cost},
                           {"marginUnit", mu},
                           {"marginTotal", mu * s},
                           {"marginPct", price > 0 ? mu / price * 100.0 : 0.0}};
    }
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["marginTotal"].toDouble() > b.toMap()["marginTotal"].toDouble();
    });
    return out;
}

double ReportService::averageTicket() const
{
    return scalar(m_db, QStringLiteral("SELECT AVG(total) FROM sales WHERE status='Pagada'"));
}

QVariantMap ReportService::salesForPeriod(const QString &range) const
{
    static const QMap<QString, int> days = {
        {QStringLiteral("dia"), 1}, {QStringLiteral("semana"), 7},
        {QStringLiteral("mes"), 30}, {QStringLiteral("año"), 365},
    };
    const int d = days.value(range, 1);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT SUM(total), COUNT(*) FROM sales WHERE status!='Cancelada' AND date >= "
        "date('now', ?)"));
    q.addBindValue(QStringLiteral("-%1 days").arg(d - 1));
    double total = 0;
    int count = 0;
    if (q.exec() && q.next()) {
        total = q.value(0).toDouble();
        count = q.value(1).toInt();
    }
    return {{"total", total}, {"count", count}, {"rango", range}};
}

QVariantList ReportService::salesByDay(int days) const
{
    QMap<QString, double> byDate;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT date, SUM(total) FROM sales WHERE status!='Cancelada' GROUP BY date"));
    while (q.next())
        byDate[q.value(0).toString()] = q.value(1).toDouble();
    QVariantList out;
    const QDate today = QDate::currentDate();
    for (int i = days - 1; i >= 0; --i) {
        const QDate d = today.addDays(-i);
        const QString key = d.toString(Qt::ISODate);
        out << QVariantMap{{"day", d.toString(QStringLiteral("MM/dd"))},
                           {"date", key},
                           {"total", byDate.value(key, 0.0)}};
    }
    return out;
}

QVariantMap ReportService::salesSummary() const
{
    QVariantMap cnt = {{"Pagada", 0}, {"Pendiente", 0}, {"Cancelada", 0}};
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT status, COUNT(*) FROM sales GROUP BY status"));
    while (q.next())
        cnt[q.value(0).toString()] = q.value(1).toInt();
    return cnt;
}

QVariantMap ReportService::incomeStatement() const
{
    const double ingresos =
        scalar(m_db, QStringLiteral("SELECT COALESCE(SUM(total),0) FROM sales WHERE status='Pagada'"));
    double costo = 0.0;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT sale_items.qty, products.price_buy, products.price FROM sale_items "
        "JOIN sales ON sale_items.sale_id=sales.id "
        "JOIN products ON sale_items.product_id=products.id "
        "WHERE sales.status='Pagada'"));
    while (q.next()) {
        double c = q.value(1).toDouble();
        if (c <= 0)
            c = q.value(2).toDouble() * 0.7;
        costo += c * q.value(0).toDouble();
    }
    const double impuestos =
        scalar(m_db, QStringLiteral("SELECT COALESCE(SUM(tax),0) FROM sales WHERE status='Pagada'"));
    const double bruto = ingresos - costo;
    return {{"ingresos", ingresos},
            {"costo", costo},
            {"bruto", bruto},
            {"impuestos", impuestos},
            {"neto", bruto - impuestos * 0.1}};
}

QVariantMap ReportService::cashFlow() const
{
    const double entradas = scalar(
        m_db, QStringLiteral("SELECT COALESCE(SUM(paid),0) FROM sales WHERE status IN "
                             "('Pagada','Entregada','Facturada')"));
    const double salidas =
        scalar(m_db, QStringLiteral("SELECT COALESCE(SUM(paid),0) FROM payables"));
    return {{"entradas", entradas}, {"salidas", salidas}, {"neto", entradas - salidas}};
}

QVariantMap ReportService::taxes() const
{
    // Fase 1: desglose por tasa desde sales.tax_breakdown (JSON por venta).
    // Ventas históricas sin breakdown caen al bucket legacy iva_19 (compat).
    QMap<double, double> byRate;
    QMap<double, QString> names;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT tax, tax_breakdown FROM sales WHERE status='Pagada'"));
    bool hasColumn = true;
    while (q.next()) {
        const double legacy = q.value(0).toDouble();
        const int col = q.record().indexOf(QStringLiteral("tax_breakdown"));
        if (col < 0) {
            hasColumn = false;
            byRate[19.0] += legacy;
            continue;
        }
        const QString js = q.value(col).toString().trimmed();
        if (js.isEmpty()) {
            byRate[19.0] += legacy;
            continue;
        }
        QJsonParseError err{};
        const auto doc = QJsonDocument::fromJson(js.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isArray()) {
            byRate[19.0] += legacy;
            continue;
        }
        for (const QJsonValue &v : doc.array()) {
            if (!v.isObject())
                continue;
            const auto o = v.toObject();
            const double rate = o.value(QStringLiteral("rate")).toDouble();
            const double tax = o.value(QStringLiteral("tax")).toDouble();
            byRate[rate] += tax;
            const QString nm = o.value(QStringLiteral("name")).toString();
            if (!nm.isEmpty() && !names.contains(rate))
                names[rate] = nm;
        }
    }
    Q_UNUSED(hasColumn);
    QVariantList breakdown;
    double total = 0.0;
    for (auto it = byRate.begin(); it != byRate.end(); ++it) {
        total += it.value();
        breakdown << QVariantMap{{"name", names.value(it.key(), QStringLiteral("%1%").arg(it.key()))},
                                 {"rate", it.key()},
                                 {"tax", it.value()}};
    }
    return {{"iva_19", byRate.value(19.0, 0.0)}, {"total", total}, {"breakdown", breakdown}};
}

QVariantMap ReportService::kpis() const
{
    const QVariantMap estado = incomeStatement();
    const double costo = estado["costo"].toDouble();
    const double invVal =
        scalar(m_db, QStringLiteral("SELECT COALESCE(SUM(price_buy*stock),0) FROM products"));
    const double rotacion = costo / (invVal > 0 ? invVal : 1.0);
    const double totalDocs =
        scalar(m_db, QStringLiteral("SELECT COUNT(*) FROM sales WHERE status IN "
                                    "('Pagada','Cotización','Pedido')"));
    const double pagadas =
        scalar(m_db, QStringLiteral("SELECT COUNT(*) FROM sales WHERE status='Pagada'"));
    const double ingresos = estado["ingresos"].toDouble();
    const double brutoPct = ingresos > 0 ? estado["bruto"].toDouble() / ingresos * 100.0 : 0.0;
    const double netoPct = ingresos > 0 ? estado["neto"].toDouble() / ingresos * 100.0 : 0.0;
    constexpr double costosFijos = 5000000.0;
    const double margen = brutoPct / 100.0 > 0 ? brutoPct / 100.0 : 0.01;
    return {{"rotacion", rotacion},
            {"dias_inventario", rotacion > 0 ? 365.0 / rotacion : 0.0},
            {"conversion", totalDocs > 0 ? pagadas / totalDocs * 100.0 : 0.0},
            {"margen_bruto_pct", brutoPct},
            {"margen_neto_pct", netoPct},
            {"punto_equilibrio", costosFijos / margen},
            {"costos_fijos", costosFijos}};
}

QVariantList ReportService::expiringProducts(int days) const
{
    QVariantList out;
    const QString limit =
        QDate::currentDate().addDays(days > 0 ? days : 30).toString(Qt::ISODate);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT sku, name, lote, vencimiento, stock FROM products "
        "WHERE vencimiento IS NOT NULL AND vencimiento != '' AND vencimiento <= ? "
        "ORDER BY vencimiento"));
    q.addBindValue(limit);
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

QVariantMap ReportService::serialsReport() const
{
    // Fase 4: conteo por estado + detalle (para celulares/taller).
    QVariantMap counts{{"in_stock", 0}, {"sold", 0}, {"rma", 0}, {"repaired", 0}};
    QVariantList items;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT s.serial, s.sku, s.status, s.sale_id, p.name FROM serials s "
            "LEFT JOIN products p ON p.sku = s.sku ORDER BY s.id DESC LIMIT 500")))
        return {{"counts", counts}, {"items", items}};
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

QVariantList ReportService::wasteReport() const
{
    // Fase 4: mermas por producto (cantidad + costo).
    QVariantList out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT m.sku, m.product, SUM(-m.qty), p.price_buy FROM inventory_movements m "
            "LEFT JOIN products p ON p.sku = m.sku WHERE m.type='Merma' "
            "GROUP BY m.sku ORDER BY SUM(-m.qty) DESC")))
        return out;
    while (q.next()) {
        const double qty = q.value(2).toDouble();
        double cost = q.value(3).toDouble();
        if (cost <= 0)
            cost = 0.0;
        out << QVariantMap{{"sku", q.value(0).toString()},
                           {"product", q.value(1).toString()},
                           {"qty", qty},
                           {"cost", qty * cost}};
    }
    return out;
}

namespace
{
QString normCat(const QString &cat)
{
    const QString c = cat.trimmed();
    return c.isEmpty() ? QStringLiteral("General") : c;
}
} // namespace

QVariantList ReportService::rotationByCategory() const
{
    // Fase 5 (genérico): vendidos e ingreso por categoría vs stock (rotación).
    QMap<QString, QVariantMap> sold;
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral(
            "SELECT p.cat, SUM(si.qty), SUM(si.subtotal) FROM sale_items si "
            "JOIN sales s ON si.sale_id=s.id "
            "JOIN products p ON si.product_id=p.id "
            "WHERE s.status!='Cancelada' GROUP BY p.cat"))) {
        while (q.next()) {
            sold[normCat(q.value(0).toString())] = {{"sold", q.value(1).toDouble()},
                                                    {"revenue", q.value(2).toDouble()}};
        }
    }
    QVariantList out;
    QSqlQuery p(m_db);
    if (!p.exec(QStringLiteral(
            "SELECT cat, COUNT(*), COALESCE(SUM(stock),0) FROM products GROUP BY cat")))
        return out;
    while (p.next()) {
        const QString cat = normCat(p.value(0).toString());
        const double units = sold.value(cat).value(QStringLiteral("sold"), 0.0).toDouble();
        const double stock = p.value(2).toDouble();
        out << QVariantMap{{"category", cat},
                           {"products", p.value(1).toInt()},
                           {"sold", units},
                           {"revenue", sold.value(cat).value(QStringLiteral("revenue"), 0.0)},
                           {"stock", stock},
                           {"rotation", stock > 1e-9 ? units / stock : units}};
    }
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["revenue"].toDouble() > b.toMap()["revenue"].toDouble();
    });
    return out;
}

QVariantMap ReportService::inventoryValue() const
{
    // Fase 5 (genérico): inventario valorizado a costo y a precio de venta.
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT COALESCE(SUM(price_buy*stock),0), "
                          "COALESCE(SUM(price*stock),0), COALESCE(SUM(stock),0), "
                          "COUNT(*) FROM products"));
    if (!q.next())
        return {{"cost", 0.0}, {"sale", 0.0}, {"units", 0.0}, {"items", 0}};
    return {{"cost", q.value(0).toDouble()},
            {"sale", q.value(1).toDouble()},
            {"units", q.value(2).toDouble()},
            {"items", q.value(3).toInt()}};
}

QVariantList ReportService::controlledSales() const
{
    // Fase 5 (farmacia): líneas vendidas de productos controlados.
    QVariantList out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT s.id, s.date, p.sku, p.name, si.qty FROM sale_items si "
            "JOIN sales s ON si.sale_id=s.id "
            "JOIN products p ON si.product_id=p.id "
            "WHERE s.status!='Cancelada' AND p.attrs_json LIKE '%\"controlled\":true%' "
            "ORDER BY s.date DESC LIMIT 500")))
        return out;
    while (q.next()) {
        out << QVariantMap{{"saleId", q.value(0).toString()},
                           {"date", q.value(1).toString()},
                           {"sku", q.value(2).toString()},
                           {"product", q.value(3).toString()},
                           {"qty", q.value(4).toDouble()}};
    }
    return out;
}

QVariantList ReportService::warrantyOpen() const
{
    // Fase 5 (celulares): seriales vendidos con garantía vigente
    // (fecha venta + warranty_months del producto).
    QVariantList out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT s.serial, s.sku, p.name, s.sale_id, sa.date, p.attrs_json "
            "FROM serials s LEFT JOIN products p ON p.sku=s.sku "
            "LEFT JOIN sales sa ON sa.id=s.sale_id "
            "WHERE s.status='sold' ORDER BY sa.date DESC LIMIT 500")))
        return out;
    const QDate today = QDate::currentDate();
    while (q.next()) {
        const QDate sold = QDate::fromString(q.value(4).toString(), Qt::ISODate);
        if (!sold.isValid())
            continue;
        const int months = Attrs::integer(q.value(5).toString(), Attrs::KWarrantyMonths, 12);
        const QDate expires = sold.addMonths(months > 0 ? months : 12);
        if (today > expires)
            continue;
        out << QVariantMap{{"serial", q.value(0).toString()},
                           {"sku", q.value(1).toString()},
                           {"product", q.value(2).toString()},
                           {"saleId", q.value(3).toString()},
                           {"saleDate", q.value(4).toString()},
                           {"warrantyMonths", months},
                           {"expiresAt", expires.toString(Qt::ISODate)}};
    }
    return out;
}

QVariantList ReportService::bulkPerformance() const
{
    // Fase 5 (abarrotes): cantidad e ingreso agrupados por unidad de medida.
    QVariantList out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT p.unit, SUM(si.qty), SUM(si.subtotal) FROM sale_items si "
            "JOIN sales s ON si.sale_id=s.id "
            "JOIN products p ON si.product_id=p.id "
            "WHERE s.status!='Cancelada' GROUP BY p.unit "
            "ORDER BY SUM(si.subtotal) DESC")))
        return out;
    while (q.next()) {
        QString unit = q.value(0).toString().trimmed();
        if (unit.isEmpty())
            unit = QStringLiteral("unidad");
        out << QVariantMap{{"unit", unit},
                           {"qty", q.value(1).toDouble()},
                           {"revenue", q.value(2).toDouble()}};
    }
    return out;
}

QString ReportService::exportCsv(const QString &type, const QString &dir) const
{
    const QString path = dir + QStringLiteral("/reporte_%1_%2.csv")
                             .arg(type, QDateTime::currentDateTime().toString(
                                               QStringLiteral("yyyyMMdd_HHmmss")));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    QTextStream out(&f);
    out << "Negocio," << setting(QStringLiteral("business_name"), QStringLiteral("Mi Negocio"))
        << "\n";
    const QString nit = setting(QStringLiteral("business_nit"));
    if (!nit.isEmpty())
        out << "NIT," << nit << "\n";
    if (type == QLatin1String("financiero")) {
        const QVariantMap e = incomeStatement(), fl = cashFlow(), tx = taxes();
        out << "Financiero,Valor\n";
        out << "Ingresos," << e["ingresos"].toDouble() << "\n";
        out << "Costo," << e["costo"].toDouble() << "\n";
        out << "Bruto," << e["bruto"].toDouble() << "\n";
        out << "IVA," << tx["iva_19"].toDouble() << "\n";
        // Fase 1: desglose por tasa.
        for (const QVariant &v : tx["breakdown"].toList()) {
            const QVariantMap m = v.toMap();
            out << "Impuesto " << m["name"].toString() << "," << m["tax"].toDouble() << "\n";
        }
        out << "Flujo neto," << fl["neto"].toDouble() << "\n";
    } else if (type == QLatin1String("vencimientos")) {
        // Fase 3: próximos a vencer (90 días).
        out << "SKU,Nombre,Lote,Vencimiento,Stock\n";
        for (const QVariant &v : expiringProducts(90)) {
            const QVariantMap m = v.toMap();
            out << m["sku"].toString() << "," << m["name"].toString() << ","
                << m["lote"].toString() << "," << m["vencimiento"].toString() << ","
                << m["stock"].toDouble() << "\n";
        }
    } else if (type == QLatin1String("seriales")) {
        // Fase 4: estado de seriales.
        const QVariantMap rep = serialsReport();
        out << "Serial,SKU,Producto,Estado,Venta\n";
        for (const QVariant &v : rep["items"].toList()) {
            const QVariantMap m = v.toMap();
            out << m["serial"].toString() << "," << m["sku"].toString() << ","
                << m["product"].toString() << "," << m["status"].toString() << ","
                << m["saleId"].toString() << "\n";
        }
    } else if (type == QLatin1String("mermas")) {
        // Fase 4: desperdicio valorizado.
        out << "SKU,Producto,Cantidad,Costo\n";
        for (const QVariant &v : wasteReport()) {
            const QVariantMap m = v.toMap();
            out << m["sku"].toString() << "," << m["product"].toString() << ","
                << m["qty"].toDouble() << "," << m["cost"].toDouble() << "\n";
        }
    } else if (type == QLatin1String("rotacion")) {
        // Fase 5: rotación por categoría.
        out << "Categoria,Productos,Vendidos,Ingreso,Stock,Rotacion\n";
        for (const QVariant &v : rotationByCategory()) {
            const QVariantMap m = v.toMap();
            out << m["category"].toString() << "," << m["products"].toInt() << ","
                << m["sold"].toDouble() << "," << m["revenue"].toDouble() << ","
                << m["stock"].toDouble() << "," << m["rotation"].toDouble() << "\n";
        }
    } else if (type == QLatin1String("inventario")) {
        // Fase 5: valorizado del inventario.
        const QVariantMap iv = inventoryValue();
        out << "Concepto,Valor\n";
        out << "Costo," << iv["cost"].toDouble() << "\n";
        out << "Venta," << iv["sale"].toDouble() << "\n";
        out << "Unidades," << iv["units"].toDouble() << "\n";
        out << "Items," << iv["items"].toInt() << "\n";
    } else if (type == QLatin1String("controlados")) {
        // Fase 5 (farmacia): controlados vendidos.
        out << "Venta,Fecha,SKU,Producto,Cantidad\n";
        for (const QVariant &v : controlledSales()) {
            const QVariantMap m = v.toMap();
            out << m["saleId"].toString() << "," << m["date"].toString() << ","
                << m["sku"].toString() << "," << m["product"].toString() << ","
                << m["qty"].toDouble() << "\n";
        }
    } else if (type == QLatin1String("garantias")) {
        // Fase 5 (celulares): seriales con garantía vigente.
        out << "Serial,SKU,Producto,Venta,FechaVenta,Vence\n";
        for (const QVariant &v : warrantyOpen()) {
            const QVariantMap m = v.toMap();
            out << m["serial"].toString() << "," << m["sku"].toString() << ","
                << m["product"].toString() << "," << m["saleId"].toString() << ","
                << m["saleDate"].toString() << "," << m["expiresAt"].toString() << "\n";
        }
    } else if (type == QLatin1String("granel")) {
        // Fase 5 (abarrotes): rendimiento por unidad de medida.
        out << "Unidad,Cantidad,Ingreso\n";
        for (const QVariant &v : bulkPerformance()) {
            const QVariantMap m = v.toMap();
            out << m["unit"].toString() << "," << m["qty"].toDouble() << ","
                << m["revenue"].toDouble() << "\n";
        }
    } else {
        const QVariantMap d = salesForPeriod(QStringLiteral("dia")),
                          w = salesForPeriod(QStringLiteral("semana"));
        out << "Operativo,Valor,Conteo\n";
        out << "Ventas dia," << d["total"].toDouble() << "," << d["count"].toInt() << "\n";
        out << "Ventas semana," << w["total"].toDouble() << "," << w["count"].toInt() << "\n";
        for (const QVariant &v : topProducts(5)) {
            const QVariantMap m = v.toMap();
            out << "Top," << m["name"].toString() << "," << m["sold"].toInt() << "\n";
        }
    }
    return path;
}

QString ReportService::exportPdf(const QString &type, const QString &dir) const
{
    const QString path = dir + QStringLiteral("/reporte_%1_%2.pdf")
                             .arg(type, QDateTime::currentDateTime().toString(
                                               QStringLiteral("yyyyMMdd_HHmmss")));
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setTitle(QStringLiteral("Reporte %1 — Sistema de Ventas").arg(type));
    QPainter painter(&writer);
    if (!painter.isActive())
        return {};
    QFont titleFont = painter.font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    int y = 400;
    painter.drawText(400, y, QStringLiteral("%1 — Reporte %2")
                                 .arg(setting(QStringLiteral("business_name"),
                                              QStringLiteral("Sistema de Ventas")),
                                      type));
    y += 350;
    const QString nit = setting(QStringLiteral("business_nit"));
    if (!nit.isEmpty()) {
        painter.drawText(400, y, QStringLiteral("NIT: %1").arg(nit));
        y += 350;
    }
    y += 150;
    QFont bodyFont = painter.font();
    bodyFont.setPointSize(10);
    bodyFont.setBold(false);
    painter.setFont(bodyFont);
    QStringList rows;
    if (type == QLatin1String("financiero")) {
        const QVariantMap e = incomeStatement(), fl = cashFlow(), tx = taxes();
        rows << QStringLiteral("Ingresos: $%1").arg(e["ingresos"].toDouble(), 0, 'f', 0)
             << QStringLiteral("Costo: $%1").arg(e["costo"].toDouble(), 0, 'f', 0)
             << QStringLiteral("Bruto: $%1").arg(e["bruto"].toDouble(), 0, 'f', 0)
             << QStringLiteral("IVA: $%1").arg(tx["iva_19"].toDouble(), 0, 'f', 0)
              << QStringLiteral("Flujo neto: $%1").arg(fl["neto"].toDouble(), 0, 'f', 0);
    } else if (type == QLatin1String("vencimientos")) {
        // Fase 3/5: próximos a vencer (90 días).
        for (const QVariant &v : expiringProducts(90)) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("%1 · lote %2 · vence %3 · stock %4")
                        .arg(m["name"].toString(), m["lote"].toString(),
                             m["vencimiento"].toString())
                        .arg(m["stock"].toDouble());
        }
    } else if (type == QLatin1String("seriales")) {
        // Fase 4/5: estado de seriales.
        for (const QVariant &v : serialsReport()["items"].toList()) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("%1 · %2 · %3")
                        .arg(m["serial"].toString(), m["product"].toString(),
                             m["status"].toString());
        }
    } else if (type == QLatin1String("mermas")) {
        // Fase 4/5: desperdicio valorizado.
        for (const QVariant &v : wasteReport()) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("%1: %2 uds · costo $%3")
                        .arg(m["product"].toString())
                        .arg(m["qty"].toDouble())
                        .arg(m["cost"].toDouble(), 0, 'f', 0);
        }
    } else if (type == QLatin1String("rotacion")) {
        // Fase 5: rotación por categoría.
        for (const QVariant &v : rotationByCategory()) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("%1: %2 uds · rotación %3")
                        .arg(m["category"].toString())
                        .arg(m["sold"].toDouble())
                        .arg(m["rotation"].toDouble(), 0, 'f', 2);
        }
    } else if (type == QLatin1String("inventario")) {
        // Fase 5: valorizado del inventario.
        const QVariantMap iv = inventoryValue();
        rows << QStringLiteral("Costo: $%1").arg(iv["cost"].toDouble(), 0, 'f', 0)
             << QStringLiteral("Venta: $%1").arg(iv["sale"].toDouble(), 0, 'f', 0)
             << QStringLiteral("Unidades: %1").arg(iv["units"].toDouble());
    } else if (type == QLatin1String("controlados")) {
        // Fase 5 (farmacia): controlados vendidos.
        for (const QVariant &v : controlledSales()) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("%1 · %2 x %3 (%4)")
                        .arg(m["saleId"].toString(), m["product"].toString())
                        .arg(m["qty"].toDouble())
                        .arg(m["date"].toString());
        }
    } else if (type == QLatin1String("garantias")) {
        // Fase 5 (celulares): garantías vigentes.
        for (const QVariant &v : warrantyOpen()) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("%1 · vence %2")
                        .arg(m["serial"].toString(), m["expiresAt"].toString());
        }
    } else if (type == QLatin1String("granel")) {
        // Fase 5 (abarrotes): rendimiento por unidad.
        for (const QVariant &v : bulkPerformance()) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("%1: %2 · $%3")
                        .arg(m["unit"].toString())
                        .arg(m["qty"].toDouble())
                        .arg(m["revenue"].toDouble(), 0, 'f', 0);
        }
    } else {
        const QVariantMap d = salesForPeriod(QStringLiteral("dia")),
                          w = salesForPeriod(QStringLiteral("semana"));
        rows << QStringLiteral("Ventas día: $%1 (%2 docs)")
                    .arg(d["total"].toDouble(), 0, 'f', 0)
                    .arg(d["count"].toInt())
             << QStringLiteral("Ventas semana: $%1 (%2 docs)")
                    .arg(w["total"].toDouble(), 0, 'f', 0)
                    .arg(w["count"].toInt());
        for (const QVariant &v : topProducts(5)) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("Top: %1 (%2 uds)").arg(m["name"].toString()).arg(m["sold"].toInt());
        }
    }
    for (const QString &row : rows) {
        y += 350;
        if (y > writer.height() - 400) {
            writer.newPage();
            y = 400;
        }
        painter.drawText(400, y, row);
    }
    painter.end();
    return QFile::exists(path) ? path : QString();
}

QString ReportService::setting(const QString &key, const QString &fallback) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT value FROM settings WHERE key=?"));
    q.addBindValue(key);
    if (!q.exec() || !q.next())
        return fallback;
    return q.value(0).toString();
}
