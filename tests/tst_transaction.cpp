// Fase 1 (MAP_PRO.md): integridad transaccional.
// - Transaction RAII: commit persiste, destructor revierte, savepoints anidados.
// - PRAGMAs de DatabaseManager (foreign_keys, WAL, busy_timeout).
// - Rollback forzado: venta fallida no deja rastro (ni filas, ni stock, ni folio).
// - Carrera 50 hilos: sin oversell, stock nunca negativo, folios únicos.
#include <QtTest>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <atomic>
#include <thread>
#include <vector>

#include "core/DatabaseManager.h"
#include "core/Transaction.h"
#include "repositories/Counters.h"
#include "repositories/ProductRepository.h"
#include "repositories/SaleRepository.h"

namespace
{
// Conexión propia por hilo sobre el mismo archivo (QSqlDatabase no se
// comparte entre hilos). Replica los PRAGMAs de DatabaseManager: cada
// conexión trae los suyos (busy_timeout es por conexión).
QSqlDatabase openThreadConn(const QString &name, const QString &path)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    db.setDatabaseName(path);
    db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=10000"));
    if (!db.open())
        return db;
    QSqlQuery q(db);
    const char *kPragmas[] = {
        "PRAGMA foreign_keys = ON",
        "PRAGMA journal_mode = WAL",
        "PRAGMA busy_timeout = 10000",
    };
    for (const char *p : kPragmas) {
        if (!q.exec(QString::fromLatin1(p)))
            qWarning() << "openThreadConn:" << p << q.lastError().text();
    }
    return db;
}

int tableCount(QSqlDatabase db, const QString &table)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM \"%1\"").arg(table)) || !q.next())
        return -1;
    return q.value(0).toInt();
}

double productStock(QSqlDatabase db, int productId)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT stock FROM products WHERE id=?"));
    q.addBindValue(productId);
    if (!q.exec() || !q.next())
        return -1.0;
    return q.value(0).toDouble();
}

SaleRepository::NewSale oneUnitSale(int productId)
{
    SaleRepository::NewSale ns;
    ns.clientName = QStringLiteral("Mostrador");
    ns.vendedor = QStringLiteral("race");
    ns.subtotal = 1000.0;
    ns.total = 1000.0;
    ns.paymentMethod = QStringLiteral("Efectivo");
    SaleItem it;
    it.productId = productId;
    it.qty = 1.0;
    it.subtotal = 1000.0;
    ns.items << it;
    return ns;
}
} // namespace

class TstTransaction : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_dbPath = m_tmp.filePath(QStringLiteral("fase1.db"));
        m_dbm = new DatabaseManager(this);
        QVERIFY(m_dbm->initialize(m_dbPath));
        m_db = m_dbm->database();
        // Producto dedicado con stock exacto (sin depender del seed demo).
        QSqlQuery q(m_db);
        QVERIFY(q.exec(QStringLiteral("INSERT INTO products (sku, name, price, price_buy, stock) "
                                      "VALUES ('F1-RACE','Producto carrera',1000,700,1000)")));
        m_productId = q.lastInsertId().toInt();
        QVERIFY(m_productId > 0);
    }

    void cleanupTestCase()
    {
        // La conexión "sales" de DatabaseManager se cierra con el objeto.
    }

    void pragmas()
    {
        QSqlQuery q(m_db);
        QVERIFY(q.exec(QStringLiteral("PRAGMA foreign_keys")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
        QVERIFY(q.exec(QStringLiteral("PRAGMA journal_mode")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString().toLower(), QStringLiteral("wal"));
        QVERIFY(q.exec(QStringLiteral("PRAGMA busy_timeout")));
        QVERIFY(q.next());
        QVERIFY(q.value(0).toInt() >= 5000);
    }

    void commitPersists()
    {
        {
            Transaction tx(m_db);
            QVERIFY(tx.isValid());
            QSqlQuery q(m_db);
            QVERIFY(q.exec(QStringLiteral(
                "INSERT INTO products (sku, name, price, stock) VALUES ('F1-C','Commit',1,1)")));
            QVERIFY(tx.commit());
        }
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM products WHERE sku='F1-C'"));
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 1);
    }

    void autoRollback()
    {
        {
            Transaction tx(m_db);
            QVERIFY(tx.isValid());
            QSqlQuery q(m_db);
            QVERIFY(q.exec(QStringLiteral(
                "INSERT INTO products (sku, name, price, stock) VALUES ('F1-R','Rollback',1,1)")));
            // Sin commit: el destructor revierte.
        }
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM products WHERE sku='F1-R'"));
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }

    void nestedInnerRollbackKeepsOuter()
    {
        {
            Transaction outer(m_db);
            QVERIFY(outer.isValid());
            QSqlQuery q(m_db);
            QVERIFY(q.exec(QStringLiteral(
                "INSERT INTO products (sku, name, price, stock) VALUES ('F1-O','Outer',1,1)")));
            {
                Transaction inner(m_db);
                QVERIFY(inner.isValid());
                QSqlQuery qi(m_db);
                QVERIFY(qi.exec(QStringLiteral(
                    "INSERT INTO products (sku, name, price, stock) VALUES ('F1-I','Inner',1,1)")));
                inner.rollback();
            }
            QVERIFY(outer.commit());
        }
        auto has = [&](const char *sku) {
            QSqlQuery q(m_db);
            q.prepare(QStringLiteral("SELECT COUNT(*) FROM products WHERE sku=?"));
            q.addBindValue(QString::fromLatin1(sku));
            return q.exec() && q.next() && q.value(0).toInt() == 1;
        };
        QVERIFY(has("F1-O"));
        QVERIFY(!has("F1-I"));
    }

    void nestedInnerCommitOuterRollbackLosesAll()
    {
        {
            Transaction outer(m_db);
            QVERIFY(outer.isValid());
            QSqlQuery q(m_db);
            QVERIFY(q.exec(QStringLiteral(
                "INSERT INTO products (sku, name, price, stock) VALUES ('F1-O2','Outer2',1,1)")));
            {
                Transaction inner(m_db);
                QVERIFY(inner.isValid());
                QSqlQuery qi(m_db);
                QVERIFY(qi.exec(QStringLiteral("INSERT INTO products (sku, name, price, stock) "
                                               "VALUES ('F1-I2','Inner2',1,1)")));
                QVERIFY(inner.commit()); // libera savepoint, pero el outer manda
            }
            outer.rollback();
        }
        QSqlQuery q(m_db);
        q.prepare(QStringLiteral("SELECT COUNT(*) FROM products WHERE sku IN ('F1-O2','F1-I2')"));
        QVERIFY(q.exec() && q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }

    void failedSaleLeavesNoTrace()
    {
        SaleRepository sales(m_db);
        const int salesBefore = tableCount(m_db, "sales");
        const int itemsBefore = tableCount(m_db, "sale_items");
        const double stockBefore = productStock(m_db, m_productId);
        const int counterBefore = Counters::get(m_db, QStringLiteral("SALE_COUNTER"));

        // Sobregiro imposible: falla en el decremento atómico.
        SaleRepository::NewSale ns = oneUnitSale(m_productId);
        ns.items[0].qty = stockBefore + 1000.0;
        ns.total = ns.subtotal = 1000.0 * ns.items[0].qty;
        QVERIFY(!sales.create(ns).ok());

        QCOMPARE(tableCount(m_db, "sales"), salesBefore);
        QCOMPARE(tableCount(m_db, "sale_items"), itemsBefore);
        QCOMPARE(productStock(m_db, m_productId), stockBefore);
        // El folio también se revierte (el contador vive en la misma tx).
        QCOMPARE(Counters::get(m_db, QStringLiteral("SALE_COUNTER")), counterBefore);
    }

    void legacyCategoriesMigration()
    {
        // BD creada con el schema histórico (REFERENCES autorreferencial
        // + una raíz parent_id=0 que con FK ON sería imposible de insertar
        // hoy). Al abrirla, DatabaseManager migra la tabla sin perder datos.
        const QString legacyPath = m_tmp.filePath(QStringLiteral("legacy.db"));
        {
            QSqlDatabase setup
                = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), "legacy_setup");
            setup.setDatabaseName(legacyPath);
            QVERIFY(setup.open());
            QSqlQuery q(setup);
            QVERIFY(q.exec(QStringLiteral("CREATE TABLE users (username TEXT PRIMARY KEY)")));
            // Un usuario para que ensureSeeded() no intente sembrar el seed
            // demo (esta tabla mínima no tiene sus columnas).
            QVERIFY(q.exec(QStringLiteral("INSERT INTO users (username) VALUES ('admin')")));
            QVERIFY(q.exec(QStringLiteral(
                "CREATE TABLE categories (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT "
                "NULL, parent_id INTEGER REFERENCES categories(id), business_type TEXT DEFAULT "
                "'', sort_order INTEGER DEFAULT 0, UNIQUE(name, parent_id))")));
            QVERIFY(q.exec(
                QStringLiteral("INSERT INTO categories (name, parent_id) VALUES ('Vieja', 0)")));
            setup.close();
        }
        QSqlDatabase::removeDatabase(QStringLiteral("legacy_setup"));

        DatabaseManager legacy;
        const bool okInit = legacy.initialize(legacyPath);
        QVERIFY(okInit);
        QSqlDatabase db = legacy.database();
        // Datos preservados.
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM categories WHERE name='Vieja'")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
        // Sin cláusula FK y con raíces insertables bajo foreign_keys=ON.
        QVERIFY(q.exec(QStringLiteral("PRAGMA foreign_key_list(categories)")));
        QVERIFY(!q.next());
        QVERIFY(q.exec(
            QStringLiteral("INSERT INTO categories (name, parent_id) VALUES ('RaizNueva', 0)")));
        legacy.close();
        // Restaurar la conexión "sales" del fixture para los slots siguientes.
        m_dbm->close();
        QVERIFY(m_dbm->initialize(m_dbPath));
        m_db = m_dbm->database();
    }

    void oversellRace()
    {
        // Stock 10, 50 hilos venden 1 ud.: exactamente 10 éxitos,
        // stock final 0, nunca negativo, sin ventas a medias.
        QSqlQuery set(m_db);
        set.prepare(QStringLiteral("UPDATE products SET stock=10 WHERE id=?"));
        set.addBindValue(m_productId);
        QVERIFY(set.exec());
        const int salesBefore = tableCount(m_db, "sales");

        std::atomic<int> ok{0};
        std::vector<std::thread> threads;
        threads.reserve(50);
        for (int i = 0; i < 50; ++i) {
            threads.emplace_back([this, i, &ok] {
                const QString conn = QStringLiteral("race_over_%1").arg(i);
                {
                    QSqlDatabase db = openThreadConn(conn, m_dbPath);
                    if (!db.isOpen())
                        return;
                    SaleRepository sales(db);
                    if (sales.create(oneUnitSale(m_productId)).ok())
                        ++ok;
                    db.close();
                }
                QSqlDatabase::removeDatabase(conn);
            });
        }
        for (auto &t : threads)
            t.join();

        QCOMPARE(ok.load(), 10);
        QCOMPARE(productStock(m_db, m_productId), 0.0);
        QCOMPARE(tableCount(m_db, "sales"), salesBefore + 10);
        // Sin negativos en ningún producto.
        QSqlQuery neg(m_db);
        QVERIFY(neg.exec(QStringLiteral("SELECT COUNT(*) FROM products WHERE stock < -1e-9")));
        QVERIFY(neg.next());
        QCOMPARE(neg.value(0).toInt(), 0);
    }

    void fullDrainRace()
    {
        // Stock 50, 50 hilos venden 1 ud.: los 50 tienen éxito, stock 0.
        QSqlQuery set(m_db);
        set.prepare(QStringLiteral("UPDATE products SET stock=50 WHERE id=?"));
        set.addBindValue(m_productId);
        QVERIFY(set.exec());
        const int salesBefore = tableCount(m_db, "sales");

        std::atomic<int> ok{0};
        std::vector<std::thread> threads;
        threads.reserve(50);
        for (int i = 0; i < 50; ++i) {
            threads.emplace_back([this, i, &ok] {
                const QString conn = QStringLiteral("race_drain_%1").arg(i);
                {
                    QSqlDatabase db = openThreadConn(conn, m_dbPath);
                    if (!db.isOpen())
                        return;
                    SaleRepository sales(db);
                    if (sales.create(oneUnitSale(m_productId)).ok())
                        ++ok;
                    db.close();
                }
                QSqlDatabase::removeDatabase(conn);
            });
        }
        for (auto &t : threads)
            t.join();

        QCOMPARE(ok.load(), 50);
        QCOMPARE(productStock(m_db, m_productId), 0.0);
        QCOMPARE(tableCount(m_db, "sales"), salesBefore + 50);
    }

  private:
    QTemporaryDir m_tmp;
    QString m_dbPath;
    DatabaseManager *m_dbm = nullptr;
    QSqlDatabase m_db;
    int m_productId = 0;
};

QTEST_MAIN(TstTransaction)
#include "tst_transaction.moc"
