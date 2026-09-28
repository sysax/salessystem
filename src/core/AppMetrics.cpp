#include "AppMetrics.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <algorithm>

AppMetrics::AppMetrics(QSqlDatabase db, QObject *parent) : QObject(parent), m_db(std::move(db))
{
}

bool AppMetrics::ensureTable(QSqlDatabase db)
{
    QSqlQuery q(db);
    return q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS app_metrics (id INTEGER PRIMARY KEY "
                                 "AUTOINCREMENT, ts TEXT "
                                 "NOT NULL, kind TEXT NOT NULL, ms INTEGER DEFAULT 0, ok INTEGER "
                                 "DEFAULT 1, sale_id TEXT "
                                 "DEFAULT '', detail TEXT DEFAULT '')"))
           && q.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_metrics_ts ON app_metrics(ts)"))
           && q.exec(
               QStringLiteral("CREATE INDEX IF NOT EXISTS idx_metrics_kind ON app_metrics(kind)"));
}

bool AppMetrics::record(const QString &kind, qint64 ms, bool ok, const QString &saleId,
                        const QString &detail)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO app_metrics (ts, kind, ms, ok, sale_id, detail) VALUES (?,?,?,?,?,?)"));
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    q.addBindValue(kind.trimmed().toLower());
    q.addBindValue(ms);
    q.addBindValue(ok ? 1 : 0);
    q.addBindValue(saleId.trimmed().left(64));
    q.addBindValue(detail.trimmed().left(512));
    if (!q.exec()) {
        qWarning("AppMetrics::record: %s", qPrintable(q.lastError().text()));
        return false;
    }
    return true;
}

bool AppMetrics::recordSale(qint64 elapsedMs, bool ok, const QString &saleId, const QString &detail)
{
    return record(QStringLiteral("sale"), elapsedMs < 0 ? 0 : elapsedMs, ok, saleId, detail);
}

bool AppMetrics::recordSync(bool ok, const QString &detail)
{
    return record(QStringLiteral("sync"), 0, ok, {}, detail);
}

bool AppMetrics::recordCaja(bool ok, const QString &detail)
{
    return record(QStringLiteral("caja"), 0, ok, {}, detail);
}

QVariantMap AppMetrics::weeklySummary() const
{
    QVariantMap out;
    const QString since = QDate::currentDate().addDays(-6).toString(Qt::ISODate);
    out.insert(QStringLiteral("since"), since);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT kind, ms, ok FROM app_metrics WHERE date(ts) >= date(?)"));
    q.addBindValue(since);
    if (!q.exec()) {
        qWarning("AppMetrics::weeklySummary: %s", qPrintable(q.lastError().text()));
        return out;
    }
    int saleCount = 0, saleFail = 0, syncFail = 0, cajaFail = 0, total = 0;
    QList<qint64> saleMs;
    while (q.next()) {
        ++total;
        const QString kind = q.value(0).toString();
        const qint64 ms = q.value(1).toLongLong();
        const bool ok = q.value(2).toInt() != 0;
        if (kind == QLatin1String("sale")) {
            ++saleCount;
            if (!ok)
                ++saleFail;
            else
                saleMs << ms;
        } else if (kind == QLatin1String("sync")) {
            if (!ok)
                ++syncFail;
        } else if (kind == QLatin1String("caja")) {
            if (!ok)
                ++cajaFail;
        }
    }
    qint64 avg = 0, p95 = 0;
    if (!saleMs.isEmpty()) {
        qint64 sum = 0;
        for (qint64 v : saleMs)
            sum += v;
        avg = sum / saleMs.size();
        std::sort(saleMs.begin(), saleMs.end());
        p95 = saleMs.at(static_cast<int>(saleMs.size() * 0.95) < saleMs.size()
                            ? static_cast<int>(saleMs.size() * 0.95)
                            : saleMs.size() - 1);
    }
    out.insert(QStringLiteral("saleCount"), saleCount);
    out.insert(QStringLiteral("saleAvgMs"), avg);
    out.insert(QStringLiteral("saleP95Ms"), p95);
    out.insert(QStringLiteral("saleFail"), saleFail);
    out.insert(QStringLiteral("syncFail"), syncFail);
    out.insert(QStringLiteral("cajaFail"), cajaFail);
    out.insert(QStringLiteral("totalEvents"), total);
    return out;
}

QString AppMetrics::exportCsv(const QString &dir) const
{
    const QString target = dir.trimmed();
    if (target.isEmpty() || !QDir().mkpath(target))
        return {};
    const auto sum = weeklySummary();
    const QString path = target + QStringLiteral("/metricas_semanal_")
                         + QDate::currentDate().toString(QStringLiteral("yyyyMMdd"))
                         + QStringLiteral(".csv");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    QTextStream out(&f);
    out << "metrica,valor\n";
    for (auto it = sum.constBegin(); it != sum.constEnd(); ++it)
        out << it.key() << ',' << it.value().toString() << '\n';
    return path;
}
