#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../domain/Entities.h"

// Bitácora (audit_log). Los demás repos reciben un puntero opcional y
// registran las mismas acciones que data/repository.py::log.
class AuditRepository : public QObject
{
    Q_OBJECT

  public:
    explicit AuditRepository(QSqlDatabase db, QObject *parent = nullptr);

    void log(const QString &user, const QString &action, const QString &detail = {});
    QList<AuditEntry> list(int limit = 100) const;
    // Fase 5: cambio estructurado (antes/después JSON) + búsqueda para el
    // explorador de administración.
    void logChange(const QString &user, const QString &action, const QString &entity,
                   const QString &entityId, const QString &beforeJson, const QString &afterJson,
                   const QString &detail = {});
    QList<AuditEntry> search(const QString &text, const QString &user, const QString &action,
                             int limit = 100) const;

    static AuditEntry rowToEntry(const QSqlQuery &q);

  private:
    QSqlDatabase m_db;
};
