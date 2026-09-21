#include "AuditRepository.h"

#include <QDateTime>
#include <QSqlQuery>

AuditRepository::AuditRepository(QSqlDatabase db, QObject *parent)
    : QObject(parent), m_db(std::move(db))
{
}

void AuditRepository::log(const QString &user, const QString &action, const QString &detail)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO audit_log (ts, user, action, detail) VALUES (?,?,?,?)"));
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs).left(19));
    q.addBindValue(user);
    q.addBindValue(action);
    q.addBindValue(detail);
    q.exec();
}

QList<AuditEntry> AuditRepository::list(int limit) const
{
    QList<AuditEntry> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM audit_log ORDER BY id DESC LIMIT ?"));
    q.addBindValue(limit);
    if (!q.exec())
        return out;
    while (q.next()) {
        AuditEntry e;
        e.id = q.value(QStringLiteral("id")).toInt();
        e.ts = q.value(QStringLiteral("ts")).toString();
        e.user = q.value(QStringLiteral("user")).toString();
        e.action = q.value(QStringLiteral("action")).toString();
        e.detail = q.value(QStringLiteral("detail")).toString();
        out << e;
    }
    return out;
}
