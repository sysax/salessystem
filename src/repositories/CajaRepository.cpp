#include "CajaRepository.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

CajaRepository::CajaRepository(QSqlDatabase db, AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_audit(audit)
{
}

static QList<CajaSale> parseSales(const QString &json, double &total)
{
    QList<CajaSale> out;
    total = 0.0;
    const QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();
    for (const auto &v : arr) {
        CajaSale s;
        s.id = v.toObject().value(QStringLiteral("id")).toString();
        s.total = v.toObject().value(QStringLiteral("total")).toDouble();
        total += s.total;
        out << s;
    }
    return out;
}

CajaStatus CajaRepository::status() const
{
    CajaStatus st;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT * FROM caja WHERE id=1")) || !q.next())
        return st;
    st.open = q.value(QStringLiteral("open")).toInt() != 0;
    st.openingAmount = q.value(QStringLiteral("opening_amount")).toDouble();
    st.openingTs = q.value(QStringLiteral("opening_ts")).toString();
    st.openingUser = q.value(QStringLiteral("opening_user")).toString();
    double total = 0.0;
    st.salesToday = parseSales(q.value(QStringLiteral("sales_today_json")).toString(), total);
    st.totalSales = total;
    st.expected = st.openingAmount + total;
    return st;
}

Result<CajaStatus> CajaRepository::open(double amount, const QString &user)
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT open FROM caja WHERE id=1"));
    if (q.next() && q.value(0).toInt() != 0)
        return Result<CajaStatus>::failure(QStringLiteral("Caja ya abierta"));
    QSqlQuery up(m_db);
    up.prepare(QStringLiteral(
        "UPDATE caja SET open=1, opening_amount=?, opening_ts=?, opening_user=?, "
        "sales_today_json='[]', expected=? WHERE id=1"));
    up.addBindValue(amount);
    up.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs).left(19));
    up.addBindValue(user);
    up.addBindValue(amount);
    if (!up.exec())
        return Result<CajaStatus>::failure(up.lastError().text());
    if (m_audit)
        m_audit->log(user, QStringLiteral("caja_apertura"),
                     QStringLiteral("$%1").arg(amount, 0, 'f', 2));
    return Result<CajaStatus>::success(status());
}

Result<CajaCloseResult> CajaRepository::close(double counted, const QString &user)
{
    const CajaStatus st = status();
    if (!st.open)
        return Result<CajaCloseResult>::failure(QStringLiteral("Caja no abierta"));
    CajaCloseResult r;
    r.expected = st.expected;
    r.counted = counted;
    r.diff = counted - st.expected;
    r.salesCount = st.salesToday.size();
    r.totalSales = st.totalSales;
    if (m_audit)
        m_audit->log(user, QStringLiteral("caja_cierre"),
                     QStringLiteral("esperado $%1 contado $%2 diff %3%4 ventas %5")
                         .arg(r.expected, 0, 'f', 2)
                         .arg(counted, 0, 'f', 2)
                         .arg(r.diff >= 0 ? QStringLiteral("+") : QString())
                         .arg(r.diff, 0, 'f', 2)
                         .arg(r.salesCount));
    QSqlQuery up(m_db);
    up.exec(QStringLiteral(
        "UPDATE caja SET open=0, opening_amount=0, opening_ts=NULL, opening_user=NULL, "
        "sales_today_json='[]', expected=0 WHERE id=1"));
    return Result<CajaCloseResult>::success(r);
}

bool CajaRepository::recordSale(const QString &saleId, double total)
{
    const CajaStatus st = status();
    if (!st.open)
        return true; // caja cerrada: la venta igual se guarda
    QJsonArray arr;
    for (const CajaSale &s : st.salesToday) {
        QJsonObject o;
        o[QStringLiteral("id")] = s.id;
        o[QStringLiteral("total")] = s.total;
        arr << o;
    }
    QJsonObject o;
    o[QStringLiteral("id")] = saleId;
    o[QStringLiteral("total")] = total;
    arr << o;
    const double expected = st.openingAmount + st.totalSales + total;
    QSqlQuery up(m_db);
    up.prepare(QStringLiteral(
        "UPDATE caja SET sales_today_json=?, expected=? WHERE id=1"));
    up.addBindValue(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    up.addBindValue(expected);
    return up.exec();
}
