// Fase 5: respaldos VACUUM INTO + retención + restauración (reapertura).
#include <QtTest>

#include "core/DatabaseManager.h"

#include <QDir>
#include <QSqlQuery>
#include <QTemporaryDir>

class TstBackup : public QObject
{
    Q_OBJECT

  private slots:
    void backupRestoreRetain()
    {
        QVERIFY(m_tmp.isValid());
        DatabaseManager dbm;
        QVERIFY(dbm.initialize(m_tmp.filePath(QStringLiteral("shop.db"))));
        // Algo que verificar tras restaurar.
        QSqlQuery q(dbm.database());
        QVERIFY(q.exec(QStringLiteral("INSERT INTO products (sku, name, price, stock, status) "
                                      "VALUES ('BK1','Respaldo',1000,5,'activo')")));
        const QString dir = m_tmp.filePath(QStringLiteral("respaldos"));
        // keep=1: dos respaldos dejan un solo archivo.
        const QVariantMap r1 = dbm.backup(dir, 1);
        QVERIFY2(r1["ok"].toBool(), qPrintable(r1["error"].toString()));
        QVERIFY(QFile::exists(r1["path"].toString()));
        const QVariantMap r2 = dbm.backup(dir, 1);
        QVERIFY(r2["ok"].toBool());
        QCOMPARE(r2["pruned"].toInt(), 1);
        QCOMPARE(QDir(dir).entryList({QStringLiteral("respaldo_*.db")}, QDir::Files).size(), 1);
        // Restauración: la copia reabre y trae los datos.
        QVERIFY(QFile::copy(r2["path"].toString(), m_tmp.filePath(QStringLiteral("rest.db"))));
        DatabaseManager open2;
        QVERIFY(open2.initialize(m_tmp.filePath(QStringLiteral("rest.db"))));
        QCOMPARE(open2.tableRowCount(QStringLiteral("products")), 1);
        QCOMPARE(open2.tableRowCount(QStringLiteral("users")), 1);
        // Dir inválido como archivo: falla controlado.
        QVERIFY(!dbm.backup(m_tmp.filePath(QStringLiteral("shop.db")), 1)["ok"].toBool());
    }

  private:
    QTemporaryDir m_tmp;
};

QTEST_MAIN(TstBackup)
#include "tst_backup.moc"
