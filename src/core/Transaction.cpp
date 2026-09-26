#include "Transaction.h"

#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSqlQuery>

namespace
{
// Profundidad de anidamiento por nombre de conexión (todas las copias
// de QSqlDatabase con el mismo nombre comparten la transacción real).
QMutex &depthMutex()
{
    static QMutex m;
    return m;
}
QHash<QString, int> &depths()
{
    static QHash<QString, int> d;
    return d;
}
bool execDirect(QSqlDatabase &db, const QString &sql)
{
    QSqlQuery q(db);
    return q.exec(sql);
}
void releaseDepth(const QString &conn)
{
    QMutexLocker lock(&depthMutex());
    depths()[conn] = qMax(0, depths().value(conn, 1) - 1);
}
} // namespace

Transaction::Transaction(QSqlDatabase &db) : m_db(&db)
{
    const QString conn = db.connectionName();
    int depth = 0;
    {
        QMutexLocker lock(&depthMutex());
        depth = depths().value(conn, 0);
    }
    if (depth == 0) {
        // Nivel externo: BEGIN IMMEDIATE real (no db.transaction(),
        // que es DEFERRED). IMMEDIATE toma el lock de escritura al
        // empezar: dos cajas concurrentes se serializan en vez de
        // entrelazar su leer-luego-escribir (caja JSON, crédito).
        // busy_timeout=5000 (DatabaseManager) hace que el segundo
        // espere en vez de fallar con SQLITE_BUSY. Sin esto, cada
        // INSERT haría autocommit y un fallo a mitad de venta dejaría
        // datos a medias.
        m_active = execDirect(db, QStringLiteral("BEGIN IMMEDIATE"));
        if (m_active) {
            QMutexLocker lock(&depthMutex());
            depths()[conn] = 1;
        }
        return;
    }
    m_savepoint = QStringLiteral("fase1_sp_%1").arg(depth);
    m_active = execDirect(db, QStringLiteral("SAVEPOINT %1").arg(m_savepoint));
    if (m_active) {
        QMutexLocker lock(&depthMutex());
        depths()[conn] = depth + 1;
    }
}

Transaction::~Transaction()
{
    if (m_active && !m_committed)
        rollback();
}

bool Transaction::commit()
{
    if (!m_active || m_committed)
        return m_committed;
    bool ok = false;
    if (m_savepoint.isEmpty()) {
        ok = m_db->commit();
        if (!ok)
            m_db->rollback();
    } else {
        ok = execDirect(*m_db, QStringLiteral("RELEASE SAVEPOINT %1").arg(m_savepoint));
        if (!ok)
            execDirect(*m_db, QStringLiteral("ROLLBACK TO SAVEPOINT %1").arg(m_savepoint));
    }
    m_active = false;
    m_committed = ok;
    releaseDepth(m_db->connectionName());
    return ok;
}

void Transaction::rollback()
{
    if (!m_active)
        return;
    if (m_savepoint.isEmpty()) {
        m_db->rollback();
    } else {
        execDirect(*m_db, QStringLiteral("ROLLBACK TO SAVEPOINT %1").arg(m_savepoint));
        execDirect(*m_db, QStringLiteral("RELEASE SAVEPOINT %1").arg(m_savepoint));
    }
    m_active = false;
    releaseDepth(m_db->connectionName());
}
