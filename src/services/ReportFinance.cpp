#include "ReportFinance.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMap>
#include <QSqlQuery>
#include <QSqlRecord>

#include "../core/Money.h"
#include "ReportCommon.h"

ReportFinance::ReportFinance(QSqlDatabase db, SettingsService *settings, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_settings(settings)
{
}

QString ReportFinance::effectiveBt(const QString &businessType) const
{
    return ReportCommon::effectiveBt(m_settings, businessType);
}

double ReportFinance::averageTicket(const QString &businessType) const
{
    // QML necesita double: se calcula en Money y se expone con .toCop().
    const QString bt = effectiveBt(businessType);
    if (bt.isEmpty())
        return ReportCommon::moneyScalar(
                   m_db, QStringLiteral("SELECT AVG(total) FROM sales WHERE status='Pagada'"))
            .toCop();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT AVG(total) FROM sales WHERE status='Pagada' AND "
                             "(business_type IS NULL OR business_type='' OR business_type=?)"));
    q.addBindValue(bt);
    if (!q.exec() || !q.next())
        return 0.0;
    return Money::fromCop(q.value(0).toDouble()).toCop();
}

QVariantMap ReportFinance::salesForPeriod(const QString &range, const QString &businessType) const
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

QVariantList ReportFinance::salesByDay(int days, const QString &businessType) const
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

QVariantMap ReportFinance::salesSummary(const QString &businessType) const
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

QVariantMap ReportFinance::incomeStatement(const QString &businessType) const
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

QVariantMap ReportFinance::cashFlow(const QString &businessType) const
{
    const QString bt = effectiveBt(businessType);
    Money entradas;
    if (bt.isEmpty()) {
        entradas = ReportCommon::moneyScalar(
            m_db, QStringLiteral("SELECT COALESCE(SUM(paid),0) FROM sales WHERE status IN "
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
    const Money salidas = ReportCommon::moneyScalar(
        m_db, QStringLiteral("SELECT COALESCE(SUM(paid),0) FROM payables"));
    return {{"entradas", entradas.toCop()},
            {"salidas", salidas.toCop()},
            {"neto", (entradas - salidas).toCop()}};
}

QVariantMap ReportFinance::taxes(const QString &businessType) const
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
    return {
        {"iva_19", byRate.value(19.0).toCop()}, {"total", total.toCop()}, {"breakdown", breakdown}};
}

QVariantMap ReportFinance::kpis(const QString &businessType) const
{
    const QString bt = effectiveBt(businessType);
    const QVariantMap estado = incomeStatement(bt);
    const Money costo = Money::fromCop(estado["costo"].toDouble());
    const Money invVal = ReportCommon::moneyScalar(
        m_db, QStringLiteral("SELECT COALESCE(SUM(price_buy*stock),0) FROM products"));
    const double costoCop = costo.toCop();
    const double invCop = invVal.toCop();
    const double rotacion = costoCop / (invCop > 0 ? invCop : 1.0);
    const double totalDocs
        = ReportCommon::scalar(m_db, QStringLiteral("SELECT COUNT(*) FROM sales WHERE status IN "
                                                    "('Pagada','Cotización','Pedido')"));
    const double pagadas = ReportCommon::scalar(
        m_db, QStringLiteral("SELECT COUNT(*) FROM sales WHERE status='Pagada'"));
    const double ingresos = estado["ingresos"].toDouble();
    const double brutoPct = ingresos > 0 ? estado["bruto"].toDouble() / ingresos * 100.0 : 0.0;
    const double netoPct = ingresos > 0 ? estado["neto"].toDouble() / ingresos * 100.0 : 0.0;
    // Fase 3: costos fijos externalizados en Settings (fallback al valor
    // histórico si no hay settings).
    const Money costosFijos
        = m_settings ? m_settings->fixedCostsMonthly() : Money::fromCop(5000000.0);
    const double margen = brutoPct / 100.0 > 0 ? brutoPct / 100.0 : 0.01;
    return {{"rotacion", rotacion},
            {"dias_inventario", rotacion > 0 ? 365.0 / rotacion : 0.0},
            {"conversion", totalDocs > 0 ? pagadas / totalDocs * 100.0 : 0.0},
            {"margen_bruto_pct", brutoPct},
            {"margen_neto_pct", netoPct},
            {"punto_equilibrio", costosFijos.toCop() / margen},
            {"costos_fijos", costosFijos.toCop()}};
}
