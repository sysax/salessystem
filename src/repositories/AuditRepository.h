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

private:
    QSqlDatabase m_db;
};
