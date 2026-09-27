// Fase 6 (MAP_PRO.md): multi-almacén/sucursal con traspasos y stock por
// ubicación. Ventas y recepciones operan en un almacén explícito (default
// Principal, id 1) sin romper la API (params defaulted).
//
// - ensureLocation crea el almacén B2.
// - transfer mueve unidades (origen −qty, destino +qty, agregado invariante).
// - venta con locationId=B2 descuenta B2 y el agregado; persiste location_id.
// - venta falla si B2 está corto aunque el global alcance.
// - receive con locationId=B2 suma a B2.
// - carrera concurrente: N hilos venden el mismo SKU en la misma ubicación
//   con stock limitado → stock nunca negativo y éxitos == stock inicial.
#include <QtTest>

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <atomic>
#include <thread>
#include <vector>

#include "TestDb.h"
#include "core/DatabaseManager.h"
#include "core/EventBus.h"
#include "core/Transaction.h"
#include "repositories/AuditRepository.h"
#include "repositories/CajaRepository.h"
#include "repositories/ClientRepository.h"
#include "repositories/CreditRepository.h"
#include "repositories/InventoryRepository.h"
#include "repositories/LocationRepository.h"
#include "repositories/ProductRepository.h"
#include "repositories/PromoRepository.h"
#include "repositories/PurchaseRepository.h"
#include "repositories/SaleRepository.h"
#include "services/InventoryService.h"
#include "services/PurchaseService.h"
#include "services/SalesService.h"

using SI = SalesService::ServiceItem;

namespace
{
// Conexión propia por hilo sobre el mismo archivo (QSqlDatabase no se
// comparte entre hilos). Replica los PRAGMAs de DatabaseManager.
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

SaleRepository::NewSale oneUnitSale(int productId, int locationId)
{
    SaleRepository::NewSale ns;
    ns.clientName = QStringLiteral("Mostrador");
    ns.vendedor = QStringLiteral("race6");
    ns.subtotal = Money::fromCop(1000.0);
    ns.total = Money::fromCop(1000.0);
    ns.paymentMethod = QStringLiteral("Efectivo");
    ns.locationId = locationId;
    SaleItem it;
    it.productId = productId;
    it.qty = 1.0;
    it.subtotal = Money::fromCop(1000.0);
    ns.items << it;
    return ns;
}
} // namespace

class TstFase6 : public QObject
{
    Q_OBJECT

  private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_dbPath = m_tmp.filePath(QStringLiteral("fase6.db"));
        m_dbm = new DatabaseManager(this);
        QVERIFY(m_dbm->initialize(m_dbPath));
        QVERIFY(TestDb::loadDemo(m_dbm->database()));
        QSqlDatabase db = m_dbm->database();
        m_audit = new AuditRepository(db, this);
        m_products = new ProductRepository(db, m_audit, this);
        m_clients = new ClientRepository(db, m_audit, this);
        m_caja = new CajaRepository(db, m_audit, this);
        m_sales = new SaleRepository(db, m_clients, m_caja, m_audit, this);
        m_inventory = new InventoryRepository(db, m_audit, this);
        m_promos = new PromoRepository(db, m_products, m_audit, this);
        m_purchases = new PurchaseRepository(db, m_audit, this);
        m_suppliers = new SupplierRepository(db, m_audit, this);
        m_payables = new PayablesRepository(db, m_audit, this);
        m_bus = new EventBus(this);
        m_invSvc = new InventoryService(db, m_products, m_inventory, m_bus, this);
        m_purSvc = new PurchaseService(db, m_purchases, m_products, m_suppliers, m_inventory,
                                       m_payables, m_audit, this);
        m_salesSvc = new SalesService(db, m_products, m_sales, m_inventory, m_clients, m_caja,
                                      m_promos, m_bus, nullptr, m_audit, nullptr, this);
        m_ledger = m_products->locations();
        QVERIFY(m_ledger != nullptr);
        // Sucursal de pruebas (id > 1; Principal = 1).
        auto b2 = m_ledger->ensureLocation(QStringLiteral("B2"));
        QVERIFY(b2.ok());
        m_b2 = b2.value().id;
        QVERIFY(m_b2 != LocationRepository::kPrincipalId);
    }

    void ensureLocationCreates()
    {
        auto r = m_ledger->ensureLocation(QStringLiteral("B2-Norte"));
        QVERIFY(r.ok());
        QVERIFY(r.value().id > 0);
        // Idempotente: segunda llamada devuelve la misma fila.
        auto again = m_ledger->ensureLocation(QStringLiteral("B2-Norte"));
        QVERIFY(again.ok());
        QCOMPARE(again.value().id, r.value().id);
        bool seen = false;
        for (const auto &l : m_ledger->locations()) {
            if (l.name == QLatin1String("B2-Norte"))
                seen = true;
        }
        QVERIFY(seen);
    }

    void transferMovesUnitsKeepsAggregate()
    {
        int pid = -1;
        mkProductChecked(QStringLiteral("F6-T"), 20.0, pid);
        QCOMPARE(m_ledger->stockAt(QStringLiteral("F6-T"), LocationRepository::kPrincipalId), 20.0);
        auto r = m_invSvc->transfer(QStringLiteral("F6-T"), 8.0, QStringLiteral("Principal"),
                                    QStringLiteral("B2"), QStringLiteral("surtir B2"),
                                    QStringLiteral("tester"));
        QVERIFY(r.ok());
        QCOMPARE(m_ledger->stockAt(QStringLiteral("F6-T"), LocationRepository::kPrincipalId), 12.0);
        QCOMPARE(m_ledger->stockAt(QStringLiteral("F6-T"), m_b2), 8.0);
        // Agregado products.stock invariante.
        QCOMPARE(m_products->findById(pid)->stock, 20.0);
    }

    void saleDiscountsChosenLocation()
    {
        int pid = -1;
        mkProductChecked(QStringLiteral("F6-S"), 10.0, pid);
        QVERIFY(m_invSvc
                    ->transfer(QStringLiteral("F6-S"), 6.0, QStringLiteral("Principal"),
                               QStringLiteral("B2"), QStringLiteral("surtir B2"),
                               QStringLiteral("tester"))
                    .ok());
        auto r = m_salesSvc->create({SI{.productId = pid, .qty = 2.0}},
                                    QStringLiteral("Mostrador"), {},
                                    QStringLiteral("Efectivo"), QString(), QStringLiteral("tester"),
                                    false, QString(), m_b2);
        QVERIFY(r.ok());
        QCOMPARE(m_ledger->stockAt(QStringLiteral("F6-S"), m_b2), 4.0);
        QCOMPARE(m_ledger->stockAt(QStringLiteral("F6-S"), LocationRepository::kPrincipalId), 4.0);
        QCOMPARE(m_products->findById(pid)->stock, 8.0);
        // La ubicación persiste en la venta.
        const auto s = m_sales->find(r.value().id);
        QVERIFY(s.has_value());
        QCOMPARE(s->locationId, m_b2);
    }

    void saleFailsWhenLocationShortButGlobalEnough()
    {
        int pid = -1;
        mkProductChecked(QStringLiteral("F6-SHORT"), 10.0, pid);
        QVERIFY(m_invSvc
                    ->transfer(QStringLiteral("F6-SHORT"), 9.0, QStringLiteral("Principal"),
                               QStringLiteral("B2"), QStringLiteral("surtir B2"),
                               QStringLiteral("tester"))
                    .ok());
        // Principal tiene 1 aunque el global sea 10: vender 5 en Principal falla.
        auto bad = m_salesSvc->create({SI{.productId = pid, .qty = 5.0}},
                                      QStringLiteral("Mostrador"), {},
                                      QStringLiteral("Efectivo"), QString(),
                                      QStringLiteral("tester"), false, QString(),
                                      LocationRepository::kPrincipalId);
        QVERIFY(!bad.ok());
        QVERIFY(bad.error().contains(QStringLiteral("Sin existencias en")));
        // Sin efectos parciales.
        QCOMPARE(m_ledger->stockAt(QStringLiteral("F6-SHORT"), LocationRepository::kPrincipalId),
                 1.0);
        QCOMPARE(m_ledger->stockAt(QStringLiteral("F6-SHORT"), m_b2), 9.0);
        QCOMPARE(m_products->findById(pid)->stock, 10.0);
        // En B2 sí alcanza.
        auto good = m_salesSvc->create({SI{.productId = pid, .qty = 5.0}},
                                       QStringLiteral("Mostrador"), {},
                                       QStringLiteral("Efectivo"), QString(),
                                       QStringLiteral("tester"), false, QString(), m_b2);
        QVERIFY(good.ok());
    }

    void receiveAddsToChosenLocation()
    {
        int pid = -1;
        mkProductChecked(QStringLiteral("F6-R"), 0.0, pid, 28000.0);
        Q_UNUSED(pid);
        auto oc = m_purSvc->create(QStringLiteral("TecnoMayorista SAS"), QStringLiteral("F6-R"),
                                   10.0, QStringLiteral("tester"));
        QVERIFY(oc.ok());
        auto rc = m_purSvc->receive(oc.value().id, QStringLiteral("tester"), m_b2);
        QVERIFY(rc.ok());
        QCOMPARE(m_ledger->stockAt(QStringLiteral("F6-R"), m_b2), 10.0);
        QCOMPARE(m_ledger->stockAt(QStringLiteral("F6-R"), LocationRepository::kPrincipalId), 0.0);
        QCOMPARE(m_products->findBySku(QStringLiteral("F6-R"))->stock, 10.0);
    }

    void locationRaceNoOversell()
    {
        // Stock 10 solo en B2, 20 hilos venden 1 ud. en B2: exactamente 10
        // éxitos, B2 en 0, agregado en 0, sin negativos.
        int pid = -1;
        mkProductChecked(QStringLiteral("F6-RACE"), 0.0, pid);
        QVERIFY(m_ledger->setStock(QStringLiteral("F6-RACE"), m_b2, 10.0));
        QCOMPARE(m_products->findById(pid)->stock, 10.0);
        const int salesBefore = tableCount(QStringLiteral("sales"));

        std::atomic<int> ok{0};
        std::vector<std::thread> threads;
        threads.reserve(20);
        for (int i = 0; i < 20; ++i) {
            threads.emplace_back([this, pid, i, &ok] {
                const QString conn = QStringLiteral("race6_%1").arg(i);
                {
                    QSqlDatabase db = openThreadConn(conn, m_dbPath);
                    if (!db.isOpen())
                        return;
                    SaleRepository sales(db);
                    if (sales.create(oneUnitSale(pid, m_b2)).ok())
                        ++ok;
                    db.close();
                }
                QSqlDatabase::removeDatabase(conn);
            });
        }
        for (auto &t : threads)
            t.join();

        QCOMPARE(ok.load(), 10);
        QCOMPARE(m_ledger->stockAt(QStringLiteral("F6-RACE"), m_b2), 0.0);
        QCOMPARE(m_products->findById(pid)->stock, 0.0);
        QCOMPARE(tableCount(QStringLiteral("sales")), salesBefore + 10);
        QSqlQuery neg(m_dbm->database());
        QVERIFY(neg.exec(QStringLiteral("SELECT COUNT(*) FROM products WHERE stock < -1e-9")));
        QVERIFY(neg.next());
        QCOMPARE(neg.value(0).toInt(), 0);
        QVERIFY(neg.exec(QStringLiteral(
            "SELECT COUNT(*) FROM stock_by_location WHERE qty < -1e-9")));
        QVERIFY(neg.next());
        QCOMPARE(neg.value(0).toInt(), 0);
    }

  private:
    // Alta vía repositorio (escribe el ledger en Principal) para aislar
    // cada caso con su propio SKU.
    int mkProduct(const QString &sku, double stock, double priceBuy = 700.0)
    {
        Product p;
        p.sku = sku;
        p.name = QStringLiteral("Prod ") + sku;
        p.price = Money::fromCop(1000.0);
        p.priceBuy = Money::fromCop(priceBuy);
        p.tax = QStringLiteral("IVA 19%");
        p.stock = stock;
        auto r = m_products->add(p);
        if (!r.ok())
            return -999; // el llamador verifica pid >= 0 con QVERIFY
        return r.value().id;
    }

    void mkProductChecked(const QString &sku, double stock, int &out, double priceBuy = 700.0)
    {
        const int pid = mkProduct(sku, stock, priceBuy);
        QVERIFY(pid >= 0);
        out = pid;
    }

    int tableCount(const QString &table)
    {
        QSqlQuery q(m_dbm->database());
        if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM \"%1\"").arg(table)) || !q.next())
            return -1;
        return q.value(0).toInt();
    }

    QTemporaryDir m_tmp;
    QString m_dbPath;
    DatabaseManager *m_dbm = nullptr;
    AuditRepository *m_audit = nullptr;
    ProductRepository *m_products = nullptr;
    ClientRepository *m_clients = nullptr;
    CajaRepository *m_caja = nullptr;
    SaleRepository *m_sales = nullptr;
    InventoryRepository *m_inventory = nullptr;
    PromoRepository *m_promos = nullptr;
    PurchaseRepository *m_purchases = nullptr;
    SupplierRepository *m_suppliers = nullptr;
    PayablesRepository *m_payables = nullptr;
    EventBus *m_bus = nullptr;
    InventoryService *m_invSvc = nullptr;
    PurchaseService *m_purSvc = nullptr;
    SalesService *m_salesSvc = nullptr;
    LocationRepository *m_ledger = nullptr;
    int m_b2 = 0;
};

QTEST_MAIN(TstFase6)
#include "tst_fase6.moc"
