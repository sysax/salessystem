#include "SettingsRepository.h"

#include <QSqlError>
#include <QSqlQuery>

SettingsRepository::SettingsRepository(QSqlDatabase db, QObject *parent)
    : QObject(parent), m_db(std::move(db))
{
}

QString SettingsRepository::get(const QString &key, const QString &fallback) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT value FROM settings WHERE key=?"));
    q.addBindValue(key);
    if (!q.exec() || !q.next())
        return fallback;
    return q.value(0).toString();
}

QVariantMap SettingsRepository::getAll() const
{
    QVariantMap out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT key, value FROM settings")))
        return out;
    while (q.next())
        out[q.value(0).toString()] = q.value(1).toString();
    return out;
}

bool SettingsRepository::set(const QString &key, const QString &value)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO settings (key, value) VALUES (?,?)"));
    q.addBindValue(key);
    q.addBindValue(value);
    return q.exec();
}

bool SettingsRepository::setAll(const QVariantMap &m)
{
    if (!m_db.transaction())
        return false;
    for (auto it = m.begin(); it != m.end(); ++it) {
        if (!set(it.key(), it.value().toString())) {
            m_db.rollback();
            return false;
        }
    }
    return m_db.commit();
}
