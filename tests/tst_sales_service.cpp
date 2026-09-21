// SalesService contra BD temporal: totales, promos, crédito,
// cancelación con reverso y eventos.
#include <QtTest>

#include "core/DatabaseManager.h"
#include "core/EventBus.h"
#include "repositories/AuditRepository.h"
#include "repositories/CajaRepository.h"
#include "repositories/ClientRepository.h"
#include "repositories/InventoryRepository.h"
#include "repositories/ProductRepository.h"
#include "repositories/PromoRepository.h"
#include "repositories/SaleRepository.h"
#include "services/SalesService.h"

#include <QTemporaryDir>

using SI = SalesService::ServiceItem;

class TstSalesService : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_dbm = new DatabaseManager(this);
        QVERIFY(m_dbm->initialize(m_tmp.filePath(QStringLiteral("sales_svc.db"))));
        QSqlDatabase db = m_dbm->database();
        m_audit = new AuditRepository(db, this);
        m_products = new ProductRepository(db, m_audit, this);
        m_clients = new ClientRepository(db, m_audit, this);
        m_caja = new CajaRepository(db, m_audit, this);
        m_sales = new SaleRepository(db, m_clients, m_caja, m_audit, this);
        m_inventory = new InventoryRepository(db, m_audit, this);
        m_promos = new PromoRepository(db, m_products, m_audit, this);
        m_bus = new EventBus(this);
        m_svc = new SalesService(db, m_products, m_sales, m_inventory, m_clients, m_caja,
                                 m_promos, m_bus, this);
    }

    void taxParsing()
    {
        QCOMPARE(SalesService::parseTaxRate(QStringLiteral("IVA 19%")), 19.0);
        QCOMPARE(SalesService::parseTaxRate(QStringLiteral("IVA 5% / Exento")), 5.0);
        QCOMPARE(SalesService::parseTaxRate(QStringLiteral("19")), 19.0);
        QCOMPARE(SalesService::parseTaxRate(QStringLiteral("Exento")), 0.0);
        QCOMPARE(SalesService::parseTaxRate(QString()), 0.0);
    }

    void createCash()
    {
        int salesEvents = 0, invEvents = 0;
        m_bus->subscribe(EventBus::SaleCreated, [&](const QVariantMap &) { ++salesEvents; });
        m_bus->subscribe(EventBus::InventoryUpdated,
                         [&](const QVariantMap &) { ++invEvents; });

        // Mouse 45000 ×2, IVA 19 % → subtotal 90000, tax 17100, total 107100
        auto r = m_svc->create({SI{2, 2}}, QStringLiteral("Juan Pérez"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("tester"));
        QVERIFY(r.ok());
        QCOMPARE(r.value().status, QStringLiteral("Pagada"));
        QVERIFY(qFuzzyCompare(r.value().total, 107100.0));
        QVERIFY(!r.value().cufe.isEmpty());
        QCOMPARE(m_products->findById(2)->stock, 45 - 2);
        QCOMPARE(salesEvents, 1);
        QCOMPARE(invEvents, 1);
        // Movimiento registrado
        const auto movs = m_inventory->movementsBySku(QStringLiteral("P002"));
        QVERIFY(!movs.isEmpty());
        QCOMPARE(movs.last().type, QStringLiteral("Salida"));
    }

    void createWithPromo()
    {
        // Laptop 1850000 + IVA 351500 − ELEC10 185000 = 2016500
        auto r = m_svc->create({SI{1, 1}}, QStringLiteral("Ana Martínez"), {},
                               QStringLiteral("Efectivo"), QStringLiteral("ELEC10"),
                               QStringLiteral("tester"));
        QVERIFY(r.ok());
        QVERIFY(qFuzzyCompare(r.value().total, 2016500.0));
        QCOMPARE(m_products->findById(1)->stock, 12 - 1);
    }

    void createCredit()
    {
        // Monitor 550000 + IVA 104500 = 654500; 50000 efectivo + resto crédito
        QMap<QString, double> pay{{"efectivo", 50000.0}, {"credito", 604500.0}};
        auto r = m_svc->create({SI{4, 1}}, QStringLiteral("María López"), pay,
                               QStringLiteral("Mixto"), QString(), QStringLiteral("tester"));
        QVERIFY(r.ok());
        QCOMPARE(r.value().status, QStringLiteral("Pendiente"));
        const auto s = m_sales->find(r.value().id);
        QVERIFY(s.has_value());
        QCOMPARE(s->paid, 50000.0);
        QCOMPARE(s->balance, 604500.0);
        // Crédito cargado al cliente (500000 seed + 604500)
        const auto c = m_clients->findByName(QStringLiteral("María López"));
        QVERIFY(c.has_value());
        QCOMPARE(c->balance, 500000.0 + 604500.0);
    }

    void validationErrors()
    {
        QVERIFY(!m_svc->create({}, QStringLiteral("X"), {}, QStringLiteral("Efectivo"),
                               QString(), QStringLiteral("tester")).ok()); // vacía
        QVERIFY(!m_svc->create({SI{1, 999}}, QStringLiteral("X"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("tester"))
                     .ok()); // sin stock
        QVERIFY(!m_svc->create({SI{999, 1}}, QStringLiteral("X"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("tester"))
                     .ok()); // inexistente
        QVERIFY(!m_svc->create({SI{2, 1}}, QStringLiteral("X"), {},
                               QStringLiteral("Efectivo"), QStringLiteral("NOPE"),
                               QStringLiteral("tester")).ok()); // promo mala
        // El stock no se movió tras los fallos
        QCOMPARE(m_products->findById(1)->stock, 12 - 1);
    }

    void cancelRevertsStock()
    {
        auto r = m_svc->create({SI{6, 3}}, QStringLiteral("Juan Pérez"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("tester"));
        QVERIFY(r.ok());
        QCOMPARE(m_products->findById(6)->stock, 30 - 3);
        auto c = m_svc->cancel(r.value().id, QStringLiteral("arrepentido"),
                               QStringLiteral("tester"));
        QVERIFY(c.ok());
        QCOMPARE(c.value().status, QStringLiteral("Cancelada"));
        QCOMPARE(m_products->findById(6)->stock, 30);
        QVERIFY(!m_svc->cancel(r.value().id, QStringLiteral("otra vez"),
                               QStringLiteral("tester")).ok());
        QVERIFY(!m_svc->cancel(QStringLiteral("NOPE"), QStringLiteral("x"),
                               QStringLiteral("tester")).ok());
    }

    void totalsDryRun()
    {
        const auto t = m_svc->calculateTotals({SI{3, 2}});
        // Teclado 145000×2=290000, IVA 19 % = 55100 → 345100
        QCOMPARE(t.itemsCount, 1);
        QVERIFY(qFuzzyCompare(t.total, 345100.0));
        QCOMPARE(m_products->findById(3)->stock, 20); // sin efecto
    }

private:
    QTemporaryDir m_tmp;
    DatabaseManager *m_dbm = nullptr;
    AuditRepository *m_audit = nullptr;
    ProductRepository *m_products = nullptr;
    ClientRepository *m_clients = nullptr;
    CajaRepository *m_caja = nullptr;
    SaleRepository *m_sales = nullptr;
    InventoryRepository *m_inventory = nullptr;
    PromoRepository *m_promos = nullptr;
    EventBus *m_bus = nullptr;
    SalesService *m_svc = nullptr;
};

QTEST_MAIN(TstSalesService)
#include "tst_sales_service.moc"
