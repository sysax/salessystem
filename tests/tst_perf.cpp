// Fase 4: rendimiento — dataset sintético (5k productos, 500 clientes,
// 500 ventas), EXPLAIN QUERY PLAN con índices y tiempos acotados.
#include <QtTest>

#include "core/DatabaseManager.h"
#include "repositories/AuditRepository.h"
#include "repositories/ClientRepository.h"
#include "repositories/ProductRepository.h"
#include "repositories/SaleRepository.h"

#include <QElapsedTimer>
#include <QSqlQuery>
#include <QTemporaryDir>

class TstPerf : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_dbm = new DatabaseManager(this);
        QVERIFY(m_dbm->initialize(m_tmp.filePath(QStringLiteral("perf.db"))));
        QSqlDatabase db = m_dbm->database();
        auto *audit = new AuditRepository(db, this);
        m_products = new ProductRepository(db, audit, this);
        m_clients = new ClientRepository(db, audit, this);
        m_sales = new SaleRepository(db, nullptr, nullptr, audit, this);
        seed();
    }

    void explainUsesIndexes()
    {
        // Estas consultas deben resolver con SEARCH (índice, no SCAN). Para
        // los índices nuevos se exige el nombre; sku/barcode ya los cubre el
        // UNIQUE (autoindex).
        const QList<QPair<QString, QString>> cases = {
            {QStringLiteral("SELECT * FROM products WHERE sku='P000001'"), QStringLiteral("")},
            {QStringLiteral("SELECT * FROM products WHERE barcode='B000001'"),
             QStringLiteral("idx_products_barcode")},
            {QStringLiteral("SELECT * FROM sale_items WHERE product_id=1"),
             QStringLiteral("idx_sale_items_product")},
            {QStringLiteral("SELECT * FROM sale_items WHERE sale_id='V000001'"),
             QStringLiteral("idx_sale_items_sale")},
            {QStringLiteral("SELECT * FROM sales WHERE date >= '2026-01-01'"),
             QStringLiteral("idx_sales_date")},
            {QStringLiteral("SELECT * FROM clients WHERE name='Cliente 1'"),
             QStringLiteral("")}, // UNIQUE ya indexa name
            {QStringLiteral("SELECT * FROM audit_log WHERE user='tester'"),
             QStringLiteral("idx_audit_user_ts")},
            {QStringLiteral("SELECT * FROM serials WHERE sku='P000001'"),
             QStringLiteral("idx_serials_sku")},
        };
        QSqlQuery q(m_dbm->database());
        for (const auto &[sql, idx] : cases) {
            QVERIFY(q.exec(QStringLiteral("EXPLAIN QUERY PLAN ") + sql));
            QString plan;
            while (q.next())
                plan += q.value(3).toString() + u' ';
            QVERIFY2(plan.contains(QStringLiteral("SEARCH"))
                         && (idx.isEmpty() || plan.contains(idx)),
                     qPrintable(sql + QStringLiteral(" -> ") + plan));
        }
    }

    void boundedTimings()
    {
        QElapsedTimer t;
        // Búsqueda de productos sobre 5k filas ("Producto 0001" → 100 filas).
        t.start();
        const auto page = m_products->searchPaged(QStringLiteral("Producto 0001"), {}, 50, 0);
        const qint64 searchMs = t.elapsed();
        QVERIFY(!page.isEmpty());
        QVERIFY2(searchMs < 2000, qPrintable(QStringLiteral("search %1 ms").arg(searchMs)));
        // Conteo + página de clientes (500).
        t.start();
        QCOMPARE(m_clients->countSearch(QStringLiteral("Cliente")), 500);
        const auto cp = m_clients->searchPaged(QStringLiteral("Cliente"), 50, 0);
        QCOMPARE(cp.size(), 50);
        QVERIFY2(t.elapsed() < 2000, "client search/count");
        // Página de ventas (500).
        t.start();
        QCOMPARE(m_sales->count(), 500);
        QCOMPARE(m_sales->listPaged(50, 0).size(), 50);
        QCOMPARE(m_sales->listPaged(50, 450).size(), 50);
        QCOMPARE(m_sales->listPaged(50, 500).size(), 0);
        QVERIFY2(t.elapsed() < 2000, "sales paging");
    }

  private:
    void seed()
    {
        // Inserción directa por lotes (rápida): una transacción por tabla.
        QSqlDatabase db = m_dbm->database();
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral("BEGIN IMMEDIATE")));
        q.prepare(QStringLiteral("INSERT INTO products (sku, barcode, name, price, stock, status) "
                                 "VALUES (?,?,?,?,?,?)"));
        for (int i = 1; i <= 5000; ++i) {
            const QString n = QString::number(i).rightJustified(6, u'0');
            q.addBindValue(QStringLiteral("P") + n);
            q.addBindValue(QStringLiteral("B") + n);
            q.addBindValue(QStringLiteral("Producto ") + n);
            q.addBindValue(1000.0 + i);
            q.addBindValue(10.0);
            q.addBindValue(QStringLiteral("activo"));
            QVERIFY(q.exec());
        }
        q.prepare(QStringLiteral("INSERT INTO clients (name, nit, credit_limit, status) "
                                 "VALUES (?,?,?,?)"));
        for (int i = 1; i <= 500; ++i) {
            q.addBindValue(QStringLiteral("Cliente ") + QString::number(i));
            q.addBindValue(QStringLiteral("900%1").arg(i));
            q.addBindValue(1000000.0);
            q.addBindValue(QStringLiteral("activo"));
            QVERIFY(q.exec());
        }
        q.prepare(QStringLiteral("INSERT INTO sales (id, date, client, total, status) "
                                 "VALUES (?,?,?,?,?)"));
        for (int i = 1; i <= 500; ++i) {
            const QString n = QString::number(i).rightJustified(6, u'0');
            q.addBindValue(QStringLiteral("V") + n);
            q.addBindValue(QStringLiteral("2026-06-15"));
            q.addBindValue(QStringLiteral("Cliente 1"));
            q.addBindValue(5000.0 + i);
            q.addBindValue(QStringLiteral("Pagada"));
            QVERIFY(q.exec());
        }
        q.prepare(QStringLiteral(
            "INSERT INTO sale_items (sale_id, product_id, qty, subtotal) VALUES (?,?,?,?)"));
        for (int i = 1; i <= 500; ++i) {
            const QString n = QString::number(i).rightJustified(6, u'0');
            q.addBindValue(QStringLiteral("V") + n);
            q.addBindValue(i);
            q.addBindValue(1.0);
            q.addBindValue(5000.0 + i);
            QVERIFY(q.exec());
        }
        // Filas para que el planificador use los índices de auditoría y seriales.
        q.prepare(QStringLiteral("INSERT INTO audit_log (ts, user, action, detail) "
                                 "VALUES (?,?,?,?)"));
        for (int i = 0; i < 50; ++i) {
            q.addBindValue(QStringLiteral("2026-06-15T10:00:00"));
            q.addBindValue(QStringLiteral("tester"));
            q.addBindValue(QStringLiteral("venta"));
            q.addBindValue(QStringLiteral("V000001"));
            QVERIFY(q.exec());
        }
        q.prepare(QStringLiteral("INSERT INTO serials (product_id, sku, serial, status) "
                                 "VALUES (?,?,?,?)"));
        for (int i = 1; i <= 50; ++i) {
            q.addBindValue(1);
            q.addBindValue(QStringLiteral("P000001"));
            q.addBindValue(QStringLiteral("SN") + QString::number(1000 + i));
            q.addBindValue(QStringLiteral("in_stock"));
            QVERIFY(q.exec());
        }
        QVERIFY(q.exec(QStringLiteral("COMMIT")));
        QCOMPARE(m_dbm->tableRowCount(QStringLiteral("products")), 5000);
    }

    QTemporaryDir m_tmp;
    DatabaseManager *m_dbm = nullptr;
    ProductRepository *m_products = nullptr;
    ClientRepository *m_clients = nullptr;
    SaleRepository *m_sales = nullptr;
};

QTEST_MAIN(TstPerf)
#include "tst_perf.moc"
