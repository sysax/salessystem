// Fase 8: calidad, observabilidad y release — logging, métricas,
// versionado de esquema y crash reports.
#include <QtTest>

#include "core/AppMetrics.h"
#include "core/CrashHandler.h"
#include "core/DatabaseManager.h"
#include "core/Logger.h"
#include "core/Version.h"

#include <QFile>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <csignal>

class TstFase8 : public QObject
{
    Q_OBJECT

  private slots:
    void versionCentral()
    {
        QVERIFY(!AppVersion::versionString().trimmed().isEmpty());
        QCOMPARE(AppVersion::kSchemaVersion, 8);
    }

    void loggerWritesWithSaleContext()
    {
        QVERIFY(m_logTmp.isValid());
        // Pre-crear un log grande para ejercitar la rotación al arrancar.
        const QString pre = m_logTmp.path() + QStringLiteral("/qtsales.log");
        QFile f(pre);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write(QByteArray(2048, 'x'));
        f.close();
        Logger::init(m_logTmp.path(), 1024, 3);
        QVERIFY(Logger::isActive());
        QVERIFY(QFile::exists(m_logTmp.path() + QStringLiteral("/qtsales.1.log")));
        Logger::setSaleContext(QStringLiteral("V-8"));
        qInfo("venta de prueba fase8");
        Logger::clearSaleContext();
        qInfo("mensaje sin venta");
        QFile cur(Logger::logFile());
        QVERIFY(cur.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(cur.readAll());
        QVERIFY2(content.contains(QStringLiteral("[sale:V-8]")), qPrintable(content.left(500)));
        QVERIFY(content.contains(QStringLiteral("venta de prueba fase8")));
    }

    void metricsWeeklyAndCsv()
    {
        QVERIFY(m_dbTmp.isValid());
        DatabaseManager dbm;
        QVERIFY(dbm.initialize(m_dbTmp.filePath(QStringLiteral("m.db"))));
        QCOMPARE(dbm.schemaVersion(), AppVersion::kSchemaVersion);
        AppMetrics m(dbm.database());
        QVERIFY(m.recordSale(120, true, QStringLiteral("V-1")));
        QVERIFY(m.recordSale(300, true, QStringLiteral("V-2")));
        QVERIFY(m.recordSale(50, false, QStringLiteral("V-3"), QStringLiteral("caja cerrada")));
        QVERIFY(m.recordSync(false, QStringLiteral("offline")));
        QVERIFY(m.recordCaja(false, QStringLiteral("diferencia sin justificar")));
        const QVariantMap s = m.weeklySummary();
        QCOMPARE(s.value(QStringLiteral("saleCount")).toInt(), 3);
        QCOMPARE(s.value(QStringLiteral("saleFail")).toInt(), 1);
        QCOMPARE(s.value(QStringLiteral("syncFail")).toInt(), 1);
        QCOMPARE(s.value(QStringLiteral("cajaFail")).toInt(), 1);
        QVERIFY(s.value(QStringLiteral("saleAvgMs")).toLongLong() > 0);
        const QString csv = m.exportCsv(m_dbTmp.path());
        QVERIFY2(!csv.isEmpty(), "exportCsv vacío");
        QVERIFY(QFile::exists(csv));
    }

    void schemaVersionedAndFutureRejected()
    {
        QVERIFY(m_dbTmp.isValid());
        const QString path = m_dbTmp.filePath(QStringLiteral("v.db"));
        {
            DatabaseManager dbm;
            QVERIFY(dbm.initialize(path));
            QCOMPARE(dbm.schemaVersion(), 8);
            QVERIFY(dbm.tableRowCount(QStringLiteral("app_metrics")) >= 0);
            dbm.close();
        }
        // Migración sobre copia de producción: copiar, abrir, datos intactos.
        QVERIFY(QFile::copy(path, m_dbTmp.filePath(QStringLiteral("copia.db"))));
        {
            DatabaseManager copia;
            QVERIFY(copia.initialize(m_dbTmp.filePath(QStringLiteral("copia.db"))));
            QCOMPARE(copia.schemaVersion(), 8);
            // BD de app futura: user_version alto => initialize falla.
            QSqlQuery q(copia.database());
            QVERIFY(q.exec(QStringLiteral("PRAGMA user_version = 999")));
            copia.close();
        }
        DatabaseManager futura;
        QVERIFY(!futura.initialize(m_dbTmp.filePath(QStringLiteral("copia.db"))));
        QVERIFY(futura.statusMessage().contains(QStringLiteral("futura")));
    }

    void crashReportIdentifiesModuleAndVersion()
    {
        QVERIFY(m_logTmp.isValid());
        const QString path = CrashHandler::writeCrashReport(
            SIGSEGV, m_logTmp.path(), QStringLiteral("1.0.0-test"), QStringLiteral("fake-stack"));
        QVERIFY(!path.isEmpty());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString c = QString::fromUtf8(f.readAll());
        QVERIFY(c.contains(QStringLiteral("QtSalesSystem")));
        QVERIFY(c.contains(QStringLiteral("1.0.0-test")));
        QVERIFY(c.contains(QStringLiteral("SIGSEGV")));
        QVERIFY(c.contains(QStringLiteral("fake-stack")));
        QVERIFY(c.contains(QStringLiteral("outbox")));
    }

    void cleanupTestCase()
    {
        Logger::shutdown();
    }

  private:
    QTemporaryDir m_logTmp;
    QTemporaryDir m_dbTmp;
};

QTEST_MAIN(TstFase8)
#include "tst_fase8.moc"
