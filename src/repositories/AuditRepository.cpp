#include "AuditRepository.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

AuditRepository::AuditRepository(QSqlDatabase db, QObject *parent)
    : QObject(parent), m_db(std::move(db))
{
}

void AuditRepository::log(const QString &user, const QString &action, const QString &detail)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO audit_log (ts, user, action, detail) VALUES (?,?,?,?)"));
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs).left(19));
    q.addBindValue(user);
    q.addBindValue(action);
    q.addBindValue(detail);
    q.exec();
}

QList<AuditEntry> AuditRepository::list(int limit) const
{
    return search({}, {}, {}, limit);
}

AuditEntry AuditRepository::rowToEntry(const QSqlQuery &q)
{
    AuditEntry e;
    e.id = q.value(QStringLiteral("id")).toInt();
    e.ts = q.value(QStringLiteral("ts")).toString();
    e.user = q.value(QStringLiteral("user")).toString();
    e.action = q.value(QStringLiteral("action")).toString();
    e.detail = q.value(QStringLiteral("detail")).toString();
    // Columnas aditivas Fase 5 (legacy → vacío).
    e.entity = q.value(QStringLiteral("entity")).toString();
    e.entityId = q.value(QStringLiteral("entity_id")).toString();
    e.beforeJson = q.value(QStringLiteral("before_json")).toString();
    e.afterJson = q.value(QStringLiteral("after_json")).toString();
    return e;
}

void AuditRepository::logChange(const QString &user, const QString &action, const QString &entity,
                                const QString &entityId, const QString &beforeJson,
                                const QString &afterJson, const QString &detail)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO audit_log (ts, user, action, detail, entity, entity_id, "
                             "before_json, after_json) VALUES (?,?,?,?,?,?,?,?)"));
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs).left(19));
    q.addBindValue(user);
    q.addBindValue(action);
    q.addBindValue(detail);
    q.addBindValue(entity.trimmed());
    q.addBindValue(entityId.trimmed());
    q.addBindValue(beforeJson.trimmed().isEmpty() ? QStringLiteral("{}") : beforeJson);
    q.addBindValue(afterJson.trimmed().isEmpty() ? QStringLiteral("{}") : afterJson);
    if (!q.exec())
        qWarning() << "AuditRepository::logChange:" << action << q.lastError().text();
}

QList<AuditEntry> AuditRepository::search(const QString &text, const QString &user,
                                          const QString &action, int limit) const
{
    QList<AuditEntry> out;
    const QString t = text.trimmed().toLower();
    const QString u = user.trimmed();
    const QString a = action.trimmed();
    QSqlQuery q(m_db);
    QString sql = QStringLiteral("SELECT * FROM audit_log WHERE 1=1");
    if (!t.isEmpty())
        sql += QStringLiteral(" AND (lower(user) LIKE ? OR lower(action) LIKE ? OR "
                              "lower(detail) LIKE ? OR lower(entity_id) LIKE ?)");
    if (!u.isEmpty())
        sql += QStringLiteral(" AND user=?");
    if (!a.isEmpty())
        sql += QStringLiteral(" AND action=?");
    sql += QStringLiteral(" ORDER BY id DESC LIMIT ") + QString::number(qMax(1, limit));
    q.prepare(sql);
    if (!t.isEmpty()) {
        const QString like = u'%' + t + u'%';
        q.addBindValue(like);
        q.addBindValue(like);
        q.addBindValue(like);
        q.addBindValue(like);
    }
    if (!u.isEmpty())
        q.addBindValue(u);
    if (!a.isEmpty())
        q.addBindValue(a);
    if (!q.exec())
        return out;
    while (q.next())
        out << rowToEntry(q);
    return out;
}
