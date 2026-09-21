#include "ReportService.h"

#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QPdfWriter>
#include <QPainter>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

#include <algorithm>

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
        out << QVariantMap{{"id", pid}, {"name", name}, {"sold", q.value(1).toInt()}};
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
    QMap<int, int> cnt;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT product_id, SUM(qty) FROM sale_items JOIN sales ON sale_items.sale_id=sales.id "
        "WHERE sales.status!='Cancelada' GROUP BY product_id"));
    while (q.next())
        cnt[q.value(0).toInt()] = q.value(1).toInt();
    QSqlQuery all(m_db);
    all.exec(QStringLiteral("SELECT id, name FROM products ORDER BY id"));
    QList<QPair<int, QString>> prods;
    while (all.next()) {
        prods << qMakePair(all.value(0).toInt(), all.value(1).toString());
        if (!cnt.contains(all.value(0).toInt()))
            cnt[all.value(0).toInt()] = 0;
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
    QMap<int, int> sold;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT product_id, SUM(qty) FROM sale_items JOIN sales ON sale_items.sale_id=sales.id "
        "WHERE sales.status='Pagada' GROUP BY product_id"));
    while (q.next())
        sold[q.value(0).toInt()] = q.value(1).toInt();
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
        const int s = sold.value(pid, 0);
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
        costo += c * q.value(0).toInt();
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
    const double iva =
        scalar(m_db, QStringLiteral("SELECT COALESCE(SUM(tax),0) FROM sales WHERE status='Pagada'"));
    return {{"iva_19", iva}, {"total", iva}};
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

QString ReportService::exportCsv(const QString &type, const QString &dir) const
{
    const QString path = dir + QStringLiteral("/reporte_%1_%2.csv")
                             .arg(type, QDateTime::currentDateTime().toString(
                                               QStringLiteral("yyyyMMdd_HHmmss")));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    QTextStream out(&f);
    if (type == QLatin1String("financiero")) {
        const QVariantMap e = incomeStatement(), fl = cashFlow(), tx = taxes();
        out << "Financiero,Valor\n";
        out << "Ingresos," << e["ingresos"].toDouble() << "\n";
        out << "Costo," << e["costo"].toDouble() << "\n";
        out << "Bruto," << e["bruto"].toDouble() << "\n";
        out << "IVA," << tx["iva_19"].toDouble() << "\n";
        out << "Flujo neto," << fl["neto"].toDouble() << "\n";
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
    painter.drawText(400, y, QStringLiteral("Sistema de Ventas — Reporte %1").arg(type));
    y += 500;
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
