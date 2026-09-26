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

    void deviceSeqAndStrategy()
    {
        // Fase 6: device estable + seq monótona + estrategia por entidad.
        const QString d1 = m_sync->deviceId();
        QVERIFY(!d1.isEmpty());
        QCOMPARE(m_sync->deviceId(), d1); // estable entre llamadas
        m_sync->queueOperation(QStringLiteral("sale"), {{"folio", "VD1"}}, QStringLiteral("kd1"));
        m_sync->queueOperation(QStringLiteral("sale"), {{"folio", "VD2"}}, QStringLiteral("kd2"));
        const QVariantList pend = m_sync->pendingList(10);
        QCOMPARE(pend.size(), 2);
        QCOMPARE(pend[0].toMap()["device"].toString(), d1);
        QVERIFY(pend[1].toMap()["seq"].toLongLong() > pend[0].toMap()["seq"].toLongLong());
        QCOMPARE(SyncService::conflictStrategy(QStringLiteral("sale")), QStringLiteral("additive"));
        QCOMPARE(SyncService::conflictStrategy(QStringLiteral("payment")),
                 QStringLiteral("additive"));
        QCOMPARE(SyncService::conflictStrategy(QStringLiteral("product")), QStringLiteral("lww"));
        QCOMPARE(SyncService::conflictStrategy(QStringLiteral("settings")), QStringLiteral("lww"));
        // Limpieza: sincronizar lo encolado aquí.
        QCOMPARE(m_sync->syncNow(true)["synced"].toInt(), 2);
    }

    void lastSyncVisible()
    {
        // Fase 6: el último sync y su estado quedan visibles en métricas.
        const QVariantMap m = m_sync->metrics();
        QVERIFY(!m["lastSync"].toString().isEmpty());
        QVERIFY(m.contains("lastError") && m.contains("deviceId"));
    }

  private:
    QTemporaryDir m_tmp;
    DatabaseManager *m_dbm = nullptr;
    SyncService *m_sync = nullptr;
};

QTEST_MAIN(TstSync)
#include "tst_sync.moc"
