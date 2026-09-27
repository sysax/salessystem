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

#include "../core/Money.h"
#include "../domain/Attrs.h"

ReportService::ReportService(QSqlDatabase db, SettingsService *settings, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_settings(settings)
{
}

QString ReportService::effectiveBt(const QString &businessType) const
{
    QString bt = businessType.trimmed();
    if (bt.isEmpty() && m_settings)
        bt = m_settings->businessType().trimmed();
    if (bt == QLatin1String("miscelanea"))
        return {};
    return bt;
}

namespace
{
// Multitienda: predicado sobre el alias de products dado ('' = legacy
// visible en todos). Retorna "" si no hay filtro; el llamador enlaza bt.
QString btPred(const QString &alias, const QString &bt)
{
    if (bt.isEmpty())
        return {};
    return QStringLiteral(" AND (%1.business_type IS NULL OR %1.business_type='' OR "
                          "%1.business_type=?)")
        .arg(alias);
}
} // namespace

static double scalar(QSqlDatabase db, const QString &sql)
{
    QSqlQuery q(db);
    if (!q.exec(sql) || !q.next())
        return 0.0;
    return q.value(0).toDouble();
}

// Frontera SQL: la BD guarda REAL; el dominio calcula en Money (céntimos).
// Solo importes de dinero usan moneyScalar; qty/conteos/tasas % siguen en double.
static Money moneyScalar(QSqlDatabase db, const QString &sql)
{
    QSqlQuery q(db);
    if (!q.exec(sql) || !q.next())
        return Money();
    return Money::fromCop(q.value(0).toDouble());
}

QVariantMap ReportService::stats(const QString &businessType) const
{
    // Multitienda: ventas por rubro ('' = mixtas/legacy, visibles en todos);
    // catálogo y clientes quedan globales.
    const QString bt = effectiveBt(businessType);
    const QString sf = btPred(QStringLiteral("sales"), bt);
    const QString pf = btPred(QStringLiteral("products"), bt);
    auto salesScalar = [&](const QString &sql, bool useSales) -> double {
        QSqlQuery q(m_db);
        if ((useSales ? sf : pf).isEmpty()) {
            if (!q.exec(sql) || !q.next())
                return 0.0;
        } else {
            // Inserta el predicado antes de un eventual GROUP/ORDER (aquí no hay).
            q.prepare(sql + (useSales ? sf : pf));
            q.addBindValue(bt);
            if (!q.exec() || !q.next())
                return 0.0;
        }
        return q.value(0).toDouble();
    };
    auto salesMoney = [&](const QString &sql) -> Money {
        QSqlQuery q(m_db);
        if (sf.isEmpty()) {
            if (!q.exec(sql) || !q.next())
                return Money();
        } else {
            q.prepare(sql + sf);
            q.addBindValue(bt);
            if (!q.exec() || !q.next())
                return Money();
        }
        return Money::fromCop(q.value(0).toDouble());
    };
    const Money totalSales = salesMoney(
        QStringLiteral("SELECT COALESCE(SUM(total),0) FROM sales WHERE status='Pagada'"));
    return {
        {"totalSales", totalSales.toCop()},
        {"totalProducts", salesScalar(QStringLiteral("SELECT COUNT(*) FROM products"), false)},
        {"totalClients", scalar(m_db, QStringLiteral("SELECT COUNT(*) FROM clients"))},
        {"pendingOrders",
         salesScalar(QStringLiteral("SELECT COUNT(*) FROM sales WHERE status='Pendiente'"), true)},
        {"lowStockAlerts",
         salesScalar(QStringLiteral("SELECT COUNT(*) FROM products WHERE stock < stock_min"),
                     false)},
    };
}

QVariantList ReportService::topProducts(int n, const QString &businessType) const
{
    QVariantList out;
    const QString bt = effectiveBt(businessType);
    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral(
            "SELECT si.product_id, SUM(si.qty) FROM sale_items si JOIN sales s ON si.sale_id=s.id "
            "JOIN products p ON si.product_id=p.id "
            "WHERE s.status!='Cancelada'")
        + btPred(QStringLiteral("p"), bt)
        + QStringLiteral(" GROUP BY si.product_id ORDER BY SUM(si.qty) DESC LIMIT ?"));
    if (!bt.isEmpty())
        q.addBindValue(bt);
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
        if (bt.isEmpty()) {
            if (!all.exec(QStringLiteral("SELECT id, name FROM products ORDER BY id")))
                return out;
        } else {
            all.prepare(QStringLiteral("SELECT id, name FROM products WHERE (business_type IS NULL "
                                       "OR business_type='' OR business_type=?) ORDER BY id"));
            all.addBindValue(bt);
            if (!all.exec())
                return out;
        }
        while (all.next() && out.size() < n) {
            if (!seen.contains(all.value(0).toInt()))
                out << QVariantMap{
                    {"id", all.value(0).toInt()}, {"name", all.value(1).toString()}, {"sold", 0}};
        }
    }
    return out;
}

QVariantList ReportService::leastSold(int n, const QString &businessType) const
{
    QVariantList out;
    const QString bt = effectiveBt(businessType);
    QMap<int, double> cnt;
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        if (!q.exec(QStringLiteral("SELECT product_id, SUM(qty) FROM sale_items JOIN sales ON "
                                   "sale_items.sale_id=sales.id "
                                   "WHERE sales.status!='Cancelada' GROUP BY product_id")))
            return out;
    } else {
        q.prepare(QStringLiteral("SELECT si.product_id, SUM(si.qty) FROM sale_items si JOIN sales "
                                 "s ON si.sale_id=s.id "
                                 "JOIN products p ON si.product_id=p.id "
                                 "WHERE s.status!='Cancelada'")
                  + btPred(QStringLiteral("p"), bt) + QStringLiteral(" GROUP BY si.product_id"));
        q.addBindValue(bt);
        if (!q.exec())
            return out;
    }
    while (q.next())
        cnt[q.value(0).toInt()] = q.value(1).toDouble();
    QSqlQuery all(m_db);
    if (bt.isEmpty()) {
        if (!all.exec(QStringLiteral("SELECT id, name FROM products ORDER BY id")))
            return out;
    } else {
        all.prepare(QStringLiteral("SELECT id, name FROM products WHERE (business_type IS NULL OR "
                                   "business_type='' OR business_type=?) ORDER BY id"));
        all.addBindValue(bt);
        if (!all.exec())
            return out;
    }
    QList<QPair<int, QString>> prods;
    while (all.next()) {
        prods << qMakePair(all.value(0).toInt(), all.value(1).toString());
        if (!cnt.contains(all.value(0).toInt()))
            cnt[all.value(0).toInt()] = 0.0;
    }
    std::sort(prods.begin(), prods.end(),
              [&](const auto &a, const auto &b) { return cnt[a.first] < cnt[b.first]; });
    for (int i = 0; i < std::min<qsizetype>(n, prods.size()); ++i)
        out << QVariantMap{
            {"id", prods[i].first}, {"name", prods[i].second}, {"sold", cnt[prods[i].first]}};
    return out;
}

QVariantList ReportService::topClients(int n, const QString &businessType) const
{
    QVariantList out;
    const QString bt = effectiveBt(businessType);
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        q.prepare(
            QStringLiteral("SELECT client, COUNT(*), SUM(total) FROM sales WHERE status='Pagada' "
                           "GROUP BY client ORDER BY COUNT(*) DESC LIMIT ?"));
        q.addBindValue(n);
    } else {
        q.prepare(
            QStringLiteral("SELECT client, COUNT(*), SUM(total) FROM sales WHERE status='Pagada' "
                           "AND (business_type IS NULL OR business_type='' OR business_type=?) "
                           "GROUP BY client ORDER BY COUNT(*) DESC LIMIT ?"));
        q.addBindValue(bt);
        q.addBindValue(n);
    }
    if (!q.exec())
        return out;
    while (q.next())
        out << QVariantMap{{"client", q.value(0).toString()},
                           {"orders", q.value(1).toInt()},
                           {"total", Money::fromCop(q.value(2).toDouble()).toCop()}};
    return out;
}

QVariantList ReportService::topSellers(int n, const QString &businessType) const
{
    QVariantList out;
    const QString bt = effectiveBt(businessType);
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        q.prepare(
            QStringLiteral("SELECT vendedor, COUNT(*), SUM(total) FROM sales WHERE status='Pagada' "
                           "GROUP BY vendedor ORDER BY COUNT(*) DESC LIMIT ?"));
        q.addBindValue(n);
    } else {
        q.prepare(
            QStringLiteral("SELECT vendedor, COUNT(*), SUM(total) FROM sales WHERE status='Pagada' "
                           "AND (business_type IS NULL OR business_type='' OR business_type=?) "
                           "GROUP BY vendedor ORDER BY COUNT(*) DESC LIMIT ?"));
        q.addBindValue(bt);
        q.addBindValue(n);
    }
    if (!q.exec())
        return out;
    while (q.next())
        out << QVariantMap{{"seller", q.value(0).toString()},
                           {"sales", q.value(1).toInt()},
                           {"total", Money::fromCop(q.value(2).toDouble()).toCop()}};
    return out;
}

QVariantList ReportService::marginPerProduct(const QString &businessType) const
{
    QVariantList out;
    const QString bt = effectiveBt(businessType);
    QMap<int, double> sold;
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        if (!q.exec(QStringLiteral("SELECT product_id, SUM(qty) FROM sale_items JOIN sales ON "
                                   "sale_items.sale_id=sales.id "
                                   "WHERE sales.status='Pagada' GROUP BY product_id")))
            return out;
    } else {
        q.prepare(QStringLiteral("SELECT si.product_id, SUM(si.qty) FROM sale_items si JOIN sales "
                                 "s ON si.sale_id=s.id "
                                 "JOIN products p ON si.product_id=p.id "
                                 "WHERE s.status='Pagada'")
                  + btPred(QStringLiteral("p"), bt) + QStringLiteral(" GROUP BY si.product_id"));
        q.addBindValue(bt);
        if (!q.exec())
            return out;
    }
    while (q.next())
        sold[q.value(0).toInt()] = q.value(1).toDouble();
    QSqlQuery p(m_db);
    if (bt.isEmpty()) {
        if (!p.exec(QStringLiteral("SELECT id, sku, name, price, price_buy FROM products")))
            return out;
    } else {
        p.prepare(QStringLiteral("SELECT id, sku, name, price, price_buy FROM products WHERE "
                                 "(business_type IS NULL OR business_type='' OR business_type=?)"));
        p.addBindValue(bt);
        if (!p.exec())
            return out;
    }
    while (p.next()) {
        const int pid = p.value(0).toInt();
        const Money price = Money::fromCop(p.value(3).toDouble());
        Money cost = Money::fromCop(p.value(4).toDouble());
        if (!cost.isPositive())
            cost = price * 0.7;
        const Money mu = price - cost;
        const double s = sold.value(pid, 0.0);
        const Money marginTotal = mu * s;
        const double priceCop = price.toCop();
        out << QVariantMap{{"id", pid},
                           {"sku", p.value(1).toString()},
                           {"name", p.value(2).toString()},
                           {"sold", s},
                           {"price", price.toCop()},
                           {"cost", cost.toCop()},
                           {"marginUnit", mu.toCop()},
                           {"marginTotal", marginTotal.toCop()},
                           {"marginPct", priceCop > 0 ? mu.toCop() / priceCop * 100.0 : 0.0}};
    }
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap()["marginTotal"].toDouble() > b.toMap()["marginTotal"].toDouble();
    });
    return out;
}

double ReportService::averageTicket(const QString &businessType) const
{
    // QML necesita double: se calcula en Money y se expone con .toCop().
    const QString bt = effectiveBt(businessType);
    if (bt.isEmpty())
        return moneyScalar(m_db, QStringLiteral("SELECT AVG(total) FROM sales WHERE status='Pagada'"))
            .toCop();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT AVG(total) FROM sales WHERE status='Pagada' AND "
                             "(business_type IS NULL OR business_type='' OR business_type=?)"));
    q.addBindValue(bt);
    if (!q.exec() || !q.next())
        return 0.0;
    return Money::fromCop(q.value(0).toDouble()).toCop();
}

QVariantMap ReportService::salesForPeriod(const QString &range, const QString &businessType) const
{
    static const QMap<QString, int> days = {
        {QStringLiteral("dia"), 1},
        {QStringLiteral("semana"), 7},
        {QStringLiteral("mes"), 30},
        {QStringLiteral("año"), 365},
    };
    const int d = days.value(range, 1);
    const QString bt = effectiveBt(businessType);
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        q.prepare(QStringLiteral(
            "SELECT SUM(total), COUNT(*) FROM sales WHERE status!='Cancelada' AND date >= "
            "date('now', ?)"));
        q.addBindValue(QStringLiteral("-%1 days").arg(d - 1));
    } else {
        q.prepare(QStringLiteral(
            "SELECT SUM(total), COUNT(*) FROM sales WHERE status!='Cancelada' AND date >= "
            "date('now', ?) AND (business_type IS NULL OR business_type='' OR business_type=?)"));
        q.addBindValue(QStringLiteral("-%1 days").arg(d - 1));
        q.addBindValue(bt);
    }
    Money total;
    int count = 0;
    if (q.exec() && q.next()) {
        total = Money::fromCop(q.value(0).toDouble());
        count = q.value(1).toInt();
    }
    return {{"total", total.toCop()}, {"count", count}, {"rango", range}};
}

QVariantList ReportService::salesByDay(int days, const QString &businessType) const
{
    QVariantList out;
    const QString bt = effectiveBt(businessType);
    QMap<QString, Money> byDate;
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        if (!q.exec(QStringLiteral(
                "SELECT date, SUM(total) FROM sales WHERE status!='Cancelada' GROUP BY date")))
            return out;
    } else {
        q.prepare(QStringLiteral("SELECT date, SUM(total) FROM sales WHERE status!='Cancelada' AND "
                                 "(business_type IS NULL "
                                 "OR business_type='' OR business_type=?) GROUP BY date"));
        q.addBindValue(bt);
        if (!q.exec())
            return out;
    }
    while (q.next())
        byDate[q.value(0).toString()] = Money::fromCop(q.value(1).toDouble());
    const QDate today = QDate::currentDate();
    for (int i = days - 1; i >= 0; --i) {
        const QDate d = today.addDays(-i);
        const QString key = d.toString(Qt::ISODate);
        out << QVariantMap{{"day", d.toString(QStringLiteral("MM/dd"))},
                           {"date", key},
                           {"total", byDate.value(key).toCop()}};
    }
    return out;
}

QVariantMap ReportService::salesSummary(const QString &businessType) const
{
    QVariantMap cnt = {{"Pagada", 0}, {"Pendiente", 0}, {"Cancelada", 0}};
    const QString bt = effectiveBt(businessType);
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        if (!q.exec(QStringLiteral("SELECT status, COUNT(*) FROM sales GROUP BY status")))
            return cnt;
    } else {
        q.prepare(
            QStringLiteral("SELECT status, COUNT(*) FROM sales WHERE (business_type IS NULL OR "
                           "business_type='' OR business_type=?) GROUP BY status"));
        q.addBindValue(bt);
        if (!q.exec())
            return cnt;
    }
    while (q.next())
        cnt[q.value(0).toString()] = q.value(1).toInt();
    return cnt;
}

QVariantMap ReportService::incomeStatement(const QString &businessType) const
{
    const QString bt = effectiveBt(businessType);
    auto salesMoney = [&](const QString &sql) -> Money {
        QSqlQuery q(m_db);
        if (bt.isEmpty()) {
            if (!q.exec(sql) || !q.next())
                return Money();
        } else {
            q.prepare(sql
                      + QStringLiteral(" AND (business_type IS NULL OR business_type='' OR "
                                       "business_type=?)"));
            q.addBindValue(bt);
            if (!q.exec() || !q.next())
                return Money();
        }
        return Money::fromCop(q.value(0).toDouble());
    };
    const Money ingresos = salesMoney(
        QStringLiteral("SELECT COALESCE(SUM(total),0) FROM sales WHERE status='Pagada'"));
    Money costo;
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        if (!q.exec(QStringLiteral(
                "SELECT sale_items.qty, products.price_buy, products.price FROM sale_items "
                "JOIN sales ON sale_items.sale_id=sales.id "
                "JOIN products ON sale_items.product_id=products.id "
                "WHERE sales.status='Pagada'")))
            return {{"ingresos", ingresos.toCop()},
                    {"costo", 0.0},
                    {"bruto", ingresos.toCop()},
                    {"impuestos", 0.0},
                    {"neto", ingresos.toCop()}};
    } else {
        q.prepare(QStringLiteral(
            "SELECT sale_items.qty, products.price_buy, products.price FROM sale_items "
            "JOIN sales ON sale_items.sale_id=sales.id "
            "JOIN products ON sale_items.product_id=products.id "
            "WHERE sales.status='Pagada' AND (sales.business_type IS NULL OR "
            "sales.business_type='' OR sales.business_type=?)"));
        q.addBindValue(bt);
        if (!q.exec())
            return {{"ingresos", ingresos.toCop()},
                    {"costo", 0.0},
                    {"bruto", ingresos.toCop()},
                    {"impuestos", 0.0},
                    {"neto", ingresos.toCop()}};
    }
    while (q.next()) {
        Money c = Money::fromCop(q.value(1).toDouble());
        if (!c.isPositive())
            c = Money::fromCop(q.value(2).toDouble()) * 0.7;
        costo += c * q.value(0).toDouble();
    }
    const Money impuestos = salesMoney(
        QStringLiteral("SELECT COALESCE(SUM(tax),0) FROM sales WHERE status='Pagada'"));
    const Money bruto = ingresos - costo;
    const Money neto = bruto - impuestos * 0.1;
    return {{"ingresos", ingresos.toCop()},
            {"costo", costo.toCop()},
            {"bruto", bruto.toCop()},
            {"impuestos", impuestos.toCop()},
            {"neto", neto.toCop()}};
}

QVariantMap ReportService::cashFlow(const QString &businessType) const
{
    const QString bt = effectiveBt(businessType);
    Money entradas;
    if (bt.isEmpty()) {
        entradas = moneyScalar(m_db, QStringLiteral("SELECT COALESCE(SUM(paid),0) FROM sales WHERE status IN "
                                          "('Pagada','Entregada','Facturada')"));
    } else {
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("SELECT COALESCE(SUM(paid),0) FROM sales WHERE status IN "
                                 "('Pagada','Entregada','Facturada') AND (business_type IS NULL OR "
                                 "business_type='' OR business_type=?)"));
        q.addBindValue(bt);
        if (q.exec() && q.next())
            entradas = Money::fromCop(q.value(0).toDouble());
    }
    // Multitienda: las CxP (proveedores) no llevan rubro → salidas globales.
    const Money salidas
        = moneyScalar(m_db, QStringLiteral("SELECT COALESCE(SUM(paid),0) FROM payables"));
    return {{"entradas", entradas.toCop()}, {"salidas", salidas.toCop()}, {"neto", (entradas - salidas).toCop()}};
}

QVariantMap ReportService::taxes(const QString &businessType) const
{
    // Fase 1: desglose por tasa desde sales.tax_breakdown (JSON por venta).
    // Ventas históricas sin breakdown caen al bucket legacy iva_19 (compat).
    // Multitienda: '' = mixtas/legacy, visibles en todos los rubros.
    const QString bt = effectiveBt(businessType);
    QMap<double, Money> byRate;
    QMap<double, QString> names;
    QSqlQuery q(m_db);
    if (bt.isEmpty()) {
        if (!q.exec(QStringLiteral("SELECT tax, tax_breakdown FROM sales WHERE status='Pagada'")))
            return {{"iva_19", 0.0}, {"total", 0.0}, {"breakdown", QVariantList{}}};
    } else {
        q.prepare(QStringLiteral("SELECT tax, tax_breakdown FROM sales WHERE status='Pagada' AND "
                                 "(business_type IS NULL OR business_type='' OR business_type=?)"));
        q.addBindValue(bt);
        if (!q.exec())
            return {{"iva_19", 0.0}, {"total", 0.0}, {"breakdown", QVariantList{}}};
    }
    bool hasColumn = true;
    while (q.next()) {
        const Money legacy = Money::fromCop(q.value(0).toDouble());
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
            const Money tax = Money::fromCop(o.value(QStringLiteral("tax")).toDouble());
            byRate[rate] += tax;
            const QString nm = o.value(QStringLiteral("name")).toString();
            if (!nm.isEmpty() && !names.contains(rate))
                names[rate] = nm;
        }
    }
    Q_UNUSED(hasColumn);
    QVariantList breakdown;
    Money total;
    for (auto it = byRate.begin(); it != byRate.end(); ++it) {
        total += it.value();
        breakdown << QVariantMap{
            {"name", names.value(it.key(), QStringLiteral("%1%").arg(it.key()))},
            {"rate", it.key()},
            {"tax", it.value().toCop()}};
    }
    return {{"iva_19", byRate.value(19.0).toCop()}, {"total", total.toCop()}, {"breakdown", breakdown}};
}

QVariantMap ReportService::kpis(const QString &businessType) const
{
    const QString bt = effectiveBt(businessType);
    const QVariantMap estado = incomeStatement(bt);
    const Money costo = Money::fromCop(estado["costo"].toDouble());
    const Money invVal = moneyScalar(
        m_db, QStringLiteral("SELECT COALESCE(SUM(price_buy*stock),0) FROM products"));
    const double costoCop = costo.toCop();
    const double invCop = invVal.toCop();
    const double rotacion = costoCop / (invCop > 0 ? invCop : 1.0);
    const double totalDocs
        = scalar(m_db, QStringLiteral("SELECT COUNT(*) FROM sales WHERE status IN "
                                      "('Pagada','Cotización','Pedido')"));
    const double pagadas
        = scalar(m_db, QStringLiteral("SELECT COUNT(*) FROM sales WHERE status='Pagada'"));
    const double ingresos = estado["ingresos"].toDouble();
    const double brutoPct = ingresos > 0 ? estado["bruto"].toDouble() / ingresos * 100.0 : 0.0;
    const double netoPct = ingresos > 0 ? estado["neto"].toDouble() / ingresos * 100.0 : 0.0;
    const Money costosFijos = Money::fromCop(5000000.0);
    const double margen = brutoPct / 100.0 > 0 ? brutoPct / 100.0 : 0.01;
    return {{"rotacion", rotacion},
            {"dias_inventario", rotacion > 0 ? 365.0 / rotacion : 0.0},
            {"conversion", totalDocs > 0 ? pagadas / totalDocs * 100.0 : 0.0},
            {"margen_bruto_pct", brutoPct},
            {"margen_neto_pct", netoPct},
            {"punto_equilibrio", costosFijos.toCop() / margen},
            {"costos_fijos", costosFijos.toCop()}};
}

QVariantList ReportService::expiringProducts(int days, const QString &businessType) const
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

QVariantMap ReportService::serialsReport(const QString &businessType) const
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

QVariantList ReportService::wasteReport(const QString &businessType) const
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

namespace
{
QString normCat(const QString &cat)
{
    const QString c = cat.trimmed();
    return c.isEmpty() ? QStringLiteral("General") : c;
}
} // namespace

QVariantList ReportService::rotationByCategory(const QString &businessType) const
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
                sold[normCat(q.value(0).toString())]
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
                sold[normCat(q.value(0).toString())]
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
        const QString cat = normCat(p.value(0).toString());
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

QVariantMap ReportService::inventoryValue(const QString &businessType) const
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

QVariantList ReportService::controlledSales(const QString &businessType) const
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

QVariantList ReportService::warrantyOpen(const QString &businessType) const
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

QVariantList ReportService::bulkPerformance(const QString &businessType) const
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

QString ReportService::exportCsv(const QString &type, const QString &dir) const
{
    const QString path
        = dir
          + QStringLiteral("/reporte_%1_%2.csv")
                .arg(type,
                     QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
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
            out << m["sku"].toString() << "," << m["name"].toString() << "," << m["lote"].toString()
                << "," << m["vencimiento"].toString() << "," << m["stock"].toDouble() << "\n";
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
    const QString path
        = dir
          + QStringLiteral("/reporte_%1_%2.pdf")
                .arg(type,
                     QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
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
    painter.drawText(
        400, y,
        QStringLiteral("%1 — Reporte %2")
            .arg(setting(QStringLiteral("business_name"), QStringLiteral("Sistema de Ventas")),
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
            rows << QStringLiteral("Top: %1 (%2 uds)")
                        .arg(m["name"].toString())
                        .arg(m["sold"].toInt());
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
