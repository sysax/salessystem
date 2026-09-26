#include "CajaRepository.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

#include "../core/Transaction.h"

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
    if (!q.exec(QStringLiteral("SELECT open FROM caja WHERE id=1")))
        return Result<CajaStatus>::failure(q.lastError().text());
    if (q.next() && q.value(0).toInt() != 0)
        return Result<CajaStatus>::failure(QStringLiteral("Caja ya abierta"));
    // Fase 1: apertura + auditoría en una transacción.
    Transaction tx(m_db);
    if (!tx.isValid())
        return Result<CajaStatus>::failure(QStringLiteral("No se pudo iniciar la transacción"));
    QSqlQuery up(m_db);
    up.prepare(
        QStringLiteral("UPDATE caja SET open=1, opening_amount=?, opening_ts=?, opening_user=?, "
                       "sales_today_json='[]', expected=? WHERE id=1"));
    up.addBindValue(amount);
    // Fase 3: turno con precisión de ms (evita colisiones entre turnos del
    // mismo segundo al filtrar movimientos por turno).
    up.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    up.addBindValue(user);
    up.addBindValue(amount);
    if (!up.exec())
        return Result<CajaStatus>::failure(up.lastError().text());
    // Fase 3: movimiento de apertura (atómico con el UPDATE: misma tx).
    if (!logMovement(QStringLiteral("apertura"), amount, QStringLiteral("efectivo"), QString(),
                     user))
        return Result<CajaStatus>::failure(QStringLiteral("No se pudo registrar la apertura"));
    if (m_audit)
        m_audit->log(user, QStringLiteral("caja_apertura"),
                     QStringLiteral("$%1").arg(amount, 0, 'f', 2));
    if (!tx.commit())
        return Result<CajaStatus>::failure(QStringLiteral("No se pudo confirmar la apertura"));
    return Result<CajaStatus>::success(status());
}

Result<CajaCloseResult> CajaRepository::close(double counted, const QString &user,
                                              const QString &reason)
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
    // Fase 5: sobra/falta sin justificar no cierra.
    if (qAbs(r.diff) > 1e-9 && reason.trimmed().isEmpty())
        return Result<CajaCloseResult>::failure(
            QStringLiteral("Diferencia de %1: indique el motivo").arg(r.diff, 0, 'f', 0));
    // Fase 1: auditoría + reseteo del turno en una transacción (el
    // UPDATE antes se ejecutaba sin verificar: un fallo dejaba la caja
    // abierta con arqueo ya reportado).
    Transaction tx(m_db);
    if (!tx.isValid())
        return Result<CajaCloseResult>::failure(
            QStringLiteral("No se pudo iniciar la transacción"));
    if (m_audit)
        m_audit->log(user, QStringLiteral("caja_cierre"),
                     QStringLiteral("esperado $%1 contado $%2 diff %3%4 ventas %5%6")
                         .arg(r.expected, 0, 'f', 2)
                         .arg(counted, 0, 'f', 2)
                         .arg(r.diff >= 0 ? QStringLiteral("+") : QString())
                         .arg(r.diff, 0, 'f', 2)
                         .arg(r.salesCount)
                         .arg(reason.trimmed().isEmpty()
                                  ? QString()
                                  : QStringLiteral(" motivo: ") + reason.trimmed()));
    QSqlQuery up(m_db);
    // Fase 3: el arqueo queda en movimientos ANTES de resetear el turno
    // (logMovement toma el turno actual).
    if (!logMovement(QStringLiteral("cierre"), counted, QStringLiteral("arqueo"), QString(), user))
        return Result<CajaCloseResult>::failure(QStringLiteral("No se pudo registrar el cierre"));
    up.prepare(QStringLiteral(
        "UPDATE caja SET open=0, opening_amount=0, opening_ts=NULL, opening_user=NULL, "
        "sales_today_json='[]', expected=0 WHERE id=1"));
    if (!up.exec())
        return Result<CajaCloseResult>::failure(up.lastError().text());
    if (!tx.commit())
        return Result<CajaCloseResult>::failure(QStringLiteral("No se pudo confirmar el cierre"));
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
    up.prepare(QStringLiteral("UPDATE caja SET sales_today_json=?, expected=? WHERE id=1"));
    up.addBindValue(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    up.addBindValue(expected);
    if (!up.exec())
        return false;
    // Fase 3: movimiento de venta (método desde payments_json de la venta).
    QString method;
    {
        QSqlQuery pay(m_db);
        pay.prepare(QStringLiteral("SELECT payments_json FROM sales WHERE id=?"));
        pay.addBindValue(saleId);
        if (pay.exec() && pay.next())
            method
                = QStringList(QJsonDocument::fromJson(pay.value(0).toByteArray()).object().keys())
                      .join(u'+');
    }
    return logMovement(QStringLiteral("venta"), total, method, saleId, st.openingUser);
}

bool CajaRepository::reverseSale(const QString &saleId)
{
    const CajaStatus st = status();
    if (!st.open)
        return true; // turno cerrado: nada que retirar del turno actual
    bool found = false;
    double removed = 0.0;
    QJsonArray arr;
    double total = 0.0;
    for (const CajaSale &s : st.salesToday) {
        if (s.id == saleId) {
            found = true;
            removed = s.total;
            continue;
        }
        QJsonObject o;
        o[QStringLiteral("id")] = s.id;
        o[QStringLiteral("total")] = s.total;
        arr << o;
        total += s.total;
    }
    if (!found)
        return true;
    QSqlQuery up(m_db);
    up.prepare(QStringLiteral("UPDATE caja SET sales_today_json=?, expected=? WHERE id=1"));
    up.addBindValue(QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
    up.addBindValue(st.openingAmount + total);
    if (!up.exec())
        return false;
    // Fase 3: la devolución queda en movimientos (monto negativo).
    return logMovement(QStringLiteral("devolucion"), -removed, QString(), saleId, st.openingUser);
}

bool CajaRepository::logMovement(const QString &type, double amount, const QString &method,
                                 const QString &saleId, const QString &user)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO caja_movimientos (ts, turno, tipo, monto, metodo_pago, sale_id, user_id) "
        "VALUES (?,?,?,?,?,?,?)"));
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs).left(19));
    q.addBindValue(currentTurno());
    q.addBindValue(type.trimmed());
    q.addBindValue(amount);
    q.addBindValue(method.trimmed());
    q.addBindValue(saleId.trimmed().isEmpty() ? QVariant() : saleId.trimmed());
    q.addBindValue(user.trimmed().isEmpty() ? QVariant() : user.trimmed());
    if (!q.exec()) {
        qWarning() << "CajaRepository::logMovement:" << q.lastError().text();
        return false;
    }
    return true;
}

QString CajaRepository::currentTurno() const
{
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT opening_ts FROM caja WHERE id=1")) || !q.next())
        return {};
    return q.value(0).toString();
}

QList<CajaRepository::Movement> CajaRepository::movements(const QString &turno) const
{
    QList<Movement> out;
    QSqlQuery q(m_db);
    // Fase 4: columnas explícitas (tabla nueva, sin legacy que tolerar).
    static const QString kCols
        = QStringLiteral("id, ts, turno, tipo, monto, metodo_pago, sale_id, user_id");
    if (turno.trimmed().isEmpty()) {
        if (!q.exec(QStringLiteral("SELECT ") + kCols
                    + QStringLiteral(" FROM caja_movimientos ORDER BY id")))
            return out;
    } else {
        q.prepare(QStringLiteral("SELECT ") + kCols
                  + QStringLiteral(" FROM caja_movimientos WHERE turno=? ORDER BY id"));
        q.addBindValue(turno.trimmed());
        if (!q.exec())
            return out;
    }
    while (q.next()) {
        Movement m;
        m.id = q.value(QStringLiteral("id")).toInt();
        m.ts = q.value(QStringLiteral("ts")).toString();
        m.turno = q.value(QStringLiteral("turno")).toString();
        m.type = q.value(QStringLiteral("tipo")).toString();
        m.amount = q.value(QStringLiteral("monto")).toDouble();
        m.method = q.value(QStringLiteral("metodo_pago")).toString();
        m.saleId = q.value(QStringLiteral("sale_id")).toString();
        m.user = q.value(QStringLiteral("user_id")).toString();
        out << m;
    }
    return out;
}

double CajaRepository::expectedFromMovements() const
{
    // Cuadra por construcción: apertura + ventas − devoluciones del turno
    // actual (el cierre es informativo y no suma). Sin turno abierto → 0.
    const QString turno = currentTurno();
    if (turno.isEmpty())
        return 0.0;
    double expected = 0.0;
    for (const Movement &m : movements(turno)) {
        if (m.type == QLatin1String("cierre"))
            continue;
        expected += m.amount;
    }
    return expected;
}
