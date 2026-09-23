#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>

// Capa Repositories (arquitectura.txt): único acceso SQL a la tabla
// `settings` (key/value). Sin lógica de negocio: eso vive en SettingsService.
class SettingsRepository : public QObject
{
    Q_OBJECT

public:
    explicit SettingsRepository(QSqlDatabase db, QObject *parent = nullptr);

    QString get(const QString &key, const QString &fallback = {}) const;
    QVariantMap getAll() const;
    bool set(const QString &key, const QString &value);
    bool setAll(const QVariantMap &m);

private:
    QSqlDatabase m_db;
};
