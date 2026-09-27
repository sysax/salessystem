#include "ReportOps.h"

#include <QMap>
#include <QSet>
#include <QSqlQuery>

#include <algorithm>

#include "../core/Money.h"
#include "ReportCommon.h"

ReportOps::ReportOps(QSqlDatabase db, SettingsService *settings, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_settings(settings)
{
}

QString ReportOps::effectiveBt(const QString &businessType) const
{
    return ReportCommon::effectiveBt(m_settings, businessType);
}

QVariantMap ReportOps::stats(const QString &businessType) const
{
    // Multitienda: ventas por rubro ('' = mixtas/legacy, visibles en todos);
    // catálogo y clientes quedan globales.
    const QString bt = effectiveBt(businessType);
    const QString sf = ReportCommon::btPred(QStringLiteral("sales"), bt);
    const QString pf = ReportCommon::btPred(QStringLiteral("products"), bt);
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
        {"totalClients",
         ReportCommon::scalar(m_db, QStringLiteral("SELECT COUNT(*) FROM clients"))},
        {"pendingOrders",
         salesScalar(QStringLiteral("SELECT COUNT(*) FROM sales WHERE status='Pendiente'"), true)},
        {"lowStockAlerts",
         salesScalar(QStringLiteral("SELECT COUNT(*) FROM products WHERE stock < stock_min"),
                     false)},
    };
}

QVariantList ReportOps::topProducts(int n, const QString &businessType) const
{
    QVariantList out;
    const QString bt = effectiveBt(businessType);
    QSqlQuery q(m_db);
    q.prepare(
        QStringLiteral(
            "SELECT si.product_id, SUM(si.qty) FROM sale_items si JOIN sales s ON si.sale_id=s.id "
            "JOIN products p ON si.product_id=p.id "
            "WHERE s.status!='Cancelada'")
        + ReportCommon::btPred(QStringLiteral("p"), bt)
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

QVariantList ReportOps::leastSold(int n, const QString &businessType) const
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
                  + ReportCommon::btPred(QStringLiteral("p"), bt)
                  + QStringLiteral(" GROUP BY si.product_id"));
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

QVariantList ReportOps::topClients(int n, const QString &businessType) const
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

QVariantList ReportOps::topSellers(int n, const QString &businessType) const
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

QVariantList ReportOps::marginPerProduct(const QString &businessType) const
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
                  + ReportCommon::btPred(QStringLiteral("p"), bt)
                  + QStringLiteral(" GROUP BY si.product_id"));
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
