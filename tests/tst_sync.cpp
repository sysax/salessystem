// SyncService: cola idempotente, sync simulado, purga y métricas.
#include <QtTest>

#include "core/DatabaseManager.h"
#include "core/EventBus.h"
#include "services/SyncService.h"

#include <QTemporaryDir>

class TstSync : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_dbm = new DatabaseManager(this);
        QVERIFY(m_dbm->initialize(m_tmp.filePath(QStringLiteral("sync.db"))));
        m_sync = new SyncService(m_dbm->database(), nullptr, this);
    }

    void queueIsIdempotent()
    {
        QCOMPARE(m_sync->pendingCount(), 0);
        m_sync->queueOperation(QStringLiteral("sale"), {{"folio", "V999"}}, QStringLiteral("k1"));
        m_sync->queueOperation(QStringLiteral("sale"), {{"folio", "V999"}}, QStringLiteral("k1"));
        QCOMPARE(m_sync->pendingCount(), 1); // INSERT OR IGNORE
        m_sync->queueOperation(QStringLiteral("sale"), {{"folio", "V998"}}, QStringLiteral("k2"));
        QCOMPARE(m_sync->pendingCount(), 2);
        QCOMPARE(m_sync->pendingList().size(), 2);
    }

    void syncForceWorksOffline()
    {
        // force=true no requiere red: sincroniza el lote pendiente
        const QVariantMap r = m_sync->syncNow(true);
        QCOMPARE(r["synced"].toInt(), 2);
        QCOMPARE(m_sync->pendingCount(), 0);
        const QVariantMap m = m_sync->metrics();
        QCOMPARE(m["syncedTotal"].toInt(), 2);
        QCOMPARE(m["circuit"].toString(), QStringLiteral("closed"));
    }

    void purgeAndReset()
    {
        m_sync->purgeSynced(0); // purga todo lo sincronizado
        const QVariantMap m = m_sync->metrics();
        QCOMPARE(m["pending"].toInt(), 0);
        m_sync->resetCircuit();
        QCOMPARE(m_sync->metrics()["circuit"].toString(), QStringLiteral("closed"));
    }

private:
    QTemporaryDir m_tmp;
    DatabaseManager *m_dbm = nullptr;
    SyncService *m_sync = nullptr;
};

QTEST_MAIN(TstSync)
#include "tst_sync.moc"
