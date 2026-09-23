// Repositories contra BD temporal sembrada: CRUD, promos, caja,
// compras, CxC/CxP, documentos y bitácora.
#include <QtTest>

#include "core/DatabaseManager.h"
#include "TestDb.h"
#include "repositories/AuditRepository.h"
#include "repositories/CajaRepository.h"
#include "repositories/ClientRepository.h"
#include "repositories/CreditRepository.h"
#include "repositories/InventoryRepository.h"
#include "repositories/ProductRepository.h"
#include "repositories/PromoRepository.h"
#include "repositories/PurchaseRepository.h"
#include "repositories/SaleRepository.h"
#include "services/PurchaseService.h"

#include <QDate>
#include <QTemporaryDir>

class TstRepositories : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_dbm = new DatabaseManager(this);
        QVERIFY(m_dbm->initialize(m_tmp.filePath(QStringLiteral("repos.db"))));
        // Datos demo solo-tests (la app siembra base limpia)
        QVERIFY(TestDb::loadDemo(m_dbm->database()));
        QSqlDatabase db = m_dbm->database();
        m_audit = new AuditRepository(db, this);
        m_products = new ProductRepository(db, m_audit, this);
        m_clients = new ClientRepository(db, m_audit, this);
        m_suppliers = new SupplierRepository(db, m_audit, this);
        m_promos = new PromoRepository(db, m_products, m_audit, this);
        m_caja = new CajaRepository(db, m_audit, this);
        m_sales = new SaleRepository(db, m_clients, m_caja, m_audit, this);
        m_inventory = new InventoryRepository(db, m_audit, this);
        m_purchases = new PurchaseRepository(db, m_audit, this);
        m_cxc = new ReceivablesRepository(db, m_sales, m_audit, this);
        m_cxp = new PayablesRepository(db, m_audit, this);
    }

    void productCrud()
    {
        Product p;
        p.sku = QStringLiteral("T001");
        p.name = QStringLiteral("Producto Test");
        p.price = 10000;
        p.stock = 5;
        auto r = m_products->add(p);
        QVERIFY(r.ok());
        QCOMPARE(r.value().priceBuy, 7000.0); // default 70 %
        QCOMPARE(r.value().priceWholesale, 9000.0); // default 90 %

        Product dup = p;
        QVERIFY(!m_products->add(dup).ok()); // SKU duplicado
        Product shortName = p;
        shortName.sku = QStringLiteral("T002");
        shortName.name = QStringLiteral("X");
        QVERIFY(!m_products->add(shortName).ok());
        Product badPrice = p;
        badPrice.sku = QStringLiteral("T003");
        badPrice.price = 0;
        QVERIFY(!m_products->add(badPrice).ok());

        Product upd = r.value();
        upd.price = 12000;
        upd.stock = 7;
        auto u = m_products->update(QStringLiteral("T001"), upd);
        QVERIFY(u.ok());
        QCOMPARE(u.value().price, 12000.0);
        QCOMPARE(u.value().stock, 7);

        QVERIFY(m_products->remove(QStringLiteral("T001")).ok());
        QVERIFY(!m_products->findBySku(QStringLiteral("T001")).has_value());
        QVERIFY(!m_products->remove(QStringLiteral("T001")).ok());
    }

    void productSearch()
    {
        QVERIFY(!m_products->search(QStringLiteral("laptop")).isEmpty());
        QCOMPARE(m_products->search(QStringLiteral("laptop")).first().sku,
                 QStringLiteral("P001"));
        QVERIFY(m_products->search(QStringLiteral("zzz-no-existe")).isEmpty());
        QVERIFY(m_products->findByBarcode(QStringLiteral("7701234560011")).has_value());
    }

    void kitFlow()
    {
        QList<KitComponent> comps = {KitComponent{QStringLiteral("P002"), 2},
                                     KitComponent{QStringLiteral("P003"), 1}};
        // stock kit = min(45//2, 20//1) = 20
        auto r = m_products->createKit(QStringLiteral("KIT1"), QStringLiteral("Kit Oficina"),
                                       comps, 0.0, QStringLiteral("tester"));
        QVERIFY(r.ok());
        QCOMPARE(r.value().stock, 20);
        // precio default = (45000*2 + 145000) * 0.95 = 223250
        QCOMPARE(r.value().price, 223250.0);
        QCOMPARE(m_products->kitComponents(QStringLiteral("KIT1")).size(), 2);

        QList<KitComponent> bad = {KitComponent{QStringLiteral("NOPE"), 1}};
        QVERIFY(!m_products->createKit(QStringLiteral("KIT2"), QStringLiteral("Malo"), bad,
                                       0.0, QStringLiteral("tester")).ok());
        QVERIFY(!m_products->createKit(QStringLiteral("KIT1"), QStringLiteral("Dup"), comps,
                                       0.0, QStringLiteral("tester")).ok());
    }

    void promoEvaluate()
    {
        CartLine laptop{1, QStringLiteral("P001"), QStringLiteral("Electronica"), 1850000.0,
                        1, 1850000.0};
        auto e1 = m_promos->evaluate({laptop}, QStringLiteral("ELEC10"));
        QVERIFY(e1.ok());
        QCOMPARE(e1.value().discount, 185000.0);

        CartLine rice{9, QStringLiteral("AB01"), QStringLiteral("Abarrotes"), 23500.0, 1,
                      23500.0};
        auto e2 = m_promos->evaluate({rice}, QStringLiteral("ELEC10"));
        QVERIFY(e2.ok());
        QCOMPARE(e2.value().discount, 0.0); // categoría no coincide

        CartLine big{1, QStringLiteral("P001"), QStringLiteral("Electronica"), 600000.0, 1,
                     600000.0};
        QCOMPARE(m_promos->evaluate({big}, QStringLiteral("50KOFF")).value().discount,
                 50000.0);
        CartLine small{1, QStringLiteral("P001"), QStringLiteral("Electronica"), 100000.0, 1,
                       100000.0};
        QCOMPARE(m_promos->evaluate({small}, QStringLiteral("50KOFF")).value().discount,
                 0.0); // bajo el mínimo

        CartLine buds{5, QStringLiteral("P005"), QStringLiteral("Audio"), 320000.0, 2,
                      640000.0};
        QCOMPARE(m_promos->evaluate({buds}, QStringLiteral("2X1AUD")).value().discount,
                 320000.0);

        QList<CartLine> vol;
        for (int i = 0; i < 10; ++i)
            vol << CartLine{2, QStringLiteral("P002"), QStringLiteral("Accesorios"), 45000.0,
                            1, 45000.0};
        QCOMPARE(m_promos->evaluate(vol, QStringLiteral("VOL5")).value().discount,
                 450000.0 * 0.05);

        QVERIFY(!m_promos->evaluate({laptop}, QStringLiteral("3X2ACC")).ok()); // inactiva
        QVERIFY(!m_promos->evaluate({laptop}, QStringLiteral("NOPE")).ok());
        auto empty = m_promos->evaluate({laptop}, QString());
        QVERIFY(empty.ok());
        QCOMPARE(empty.value().discount, 0.0);
    }

    void cajaFlow()
    {
        auto o = m_caja->open(100000, QStringLiteral("cajero"));
        QVERIFY(o.ok());
        QVERIFY(o.value().open);
        QVERIFY(!m_caja->open(50000, QStringLiteral("cajero")).ok()); // ya abierta
        QVERIFY(m_caja->recordSale(QStringLiteral("VX1"), 25000));
        QVERIFY(m_caja->recordSale(QStringLiteral("VX2"), 15000));
        auto c = m_caja->close(140000, QStringLiteral("cajero"));
        QVERIFY(c.ok());
        QCOMPARE(c.value().expected, 140000.0);
        QCOMPARE(c.value().diff, 0.0);
        QCOMPARE(c.value().salesCount, 2);
        QVERIFY(!m_caja->close(0, QStringLiteral("cajero")).ok()); // cerrada
    }

    void purchaseFlow()
    {
        PurchaseService svc(m_dbm->database(), m_purchases, m_products, m_suppliers,
                            m_inventory, m_cxp, m_audit);
        auto c = svc.create(QStringLiteral("TecnoMayorista SAS"), QStringLiteral("P002"), 5,
                            QStringLiteral("tester"));
        QVERIFY(c.ok());
        QCOMPARE(c.value().total, 28000.0 * 5);
        QCOMPARE(c.value().status, QStringLiteral("Pendiente"));

        auto r = svc.receive(c.value().id, QStringLiteral("tester"));
        QVERIFY(r.ok());
        QCOMPARE(r.value().status, QStringLiteral("Recibida"));
        QCOMPARE(m_products->findBySku(QStringLiteral("P002"))->stock, 45 + 5);
        auto cxp = m_cxp->find(c.value().id);
        QVERIFY(cxp.has_value());
        QCOMPARE(cxp->balance, 140000.0);
        QCOMPARE(cxp->discountEarly, 2.0); // regla TecnoMayorista

        QVERIFY(!svc.receive(c.value().id, QStringLiteral("tester")).ok()); // ya recibida
        QVERIFY(!svc.cancel(c.value().id, QStringLiteral("tester")).ok()); // recibida

        auto c2 = svc.create(QStringLiteral("OfiSurte SAS"), QStringLiteral("P007"), 2,
                             QStringLiteral("tester"));
        QVERIFY(c2.ok());
        auto cancelled = svc.cancel(c2.value().id, QStringLiteral("tester"));
        QVERIFY(cancelled.ok());
        QCOMPARE(cancelled.value().status, QStringLiteral("Cancelada"));
        QVERIFY(!svc.create(QStringLiteral("Nadie"), QStringLiteral("P002"), 1,
                            QStringLiteral("tester")).ok());
    }

    void cxcFlow()
    {
        SaleRepository::NewSale ns;
        ns.clientName = QStringLiteral("María López");
        ns.vendedor = QStringLiteral("tester");
        ns.subtotal = 200000;
        ns.total = 200000;
        ns.payments = {{"efectivo", 50000.0}, {"credito", 150000.0}};
        ns.items = {SaleItem{2, 1, 45000.0}};
        auto s = m_sales->create(ns);
        QVERIFY(s.ok());
        QCOMPARE(s.value().status, QStringLiteral("Pendiente"));
        QCOMPARE(s.value().paid, 50000.0);
        QCOMPARE(s.value().balance, 150000.0);

        QVERIFY(!m_cxc->pending().isEmpty());
        QVERIFY(!m_cxc->addPayment(s.value().id, 999999, QStringLiteral("Efectivo"),
                                  QStringLiteral("tester")).ok()); // sobrepago
        auto p1 = m_cxc->addPayment(s.value().id, 50000, QStringLiteral("Efectivo"),
                                    QStringLiteral("tester"));
        QVERIFY(p1.ok());
        QCOMPARE(p1.value().balance, 100000.0);
        auto p2 = m_cxc->addPayment(s.value().id, 100000, QStringLiteral("Transferencia"),
                                    QStringLiteral("tester"));
        QVERIFY(p2.ok());
        QCOMPARE(p2.value().status, QStringLiteral("Pagada"));
        QCOMPARE(m_cxc->paymentsFor(s.value().id).size(), 2);
        QVERIFY(!m_cxc->addPayment(s.value().id, 1000, QStringLiteral("Efectivo"),
                                  QStringLiteral("tester")).ok()); // sin saldo
    }

    void cxpFlow()
    {
        Payable p;
        p.id = QStringLiteral("CXPT1");
        p.supplier = QStringLiteral("OfiSurte SAS");
        p.due = QDate::currentDate().addDays(20).toString(Qt::ISODate);
        p.amount = 100000;
        p.balance = 100000;
        p.discountEarly = 10.0;
        p.status = QStringLiteral("Pendiente");
        QVERIFY(m_cxp->create(p).ok());
        QVERIFY(!m_cxp->pending().isEmpty());

        auto r1 = m_cxp->addPayment(QStringLiteral("CXPT1"), 40000,
                                    QStringLiteral("Transferencia"), QStringLiteral("tester"));
        QVERIFY(r1.ok());
        QCOMPARE(r1.value().payable.balance, 60000.0);
        QCOMPARE(r1.value().earlyDiscount, 4000.0); // pronto pago 10 %
        auto r2 = m_cxp->addPayment(QStringLiteral("CXPT1"), 60000,
                                    QStringLiteral("Transferencia"), QStringLiteral("tester"));
        QVERIFY(r2.ok());
        QCOMPARE(r2.value().payable.status, QStringLiteral("Pagada"));
        QVERIFY(!m_cxp->addPayment(QStringLiteral("CXPT1"), 1, QStringLiteral("Efectivo"),
                                   QStringLiteral("tester")).ok()); // excede saldo
        QVERIFY(!m_cxp->addPayment(QStringLiteral("NOPE"), 1, QStringLiteral("Efectivo"),
                                   QStringLiteral("tester")).ok());
    }

    void moraCalc()
    {
        Sale overdue;
        overdue.balance = 300000;
        overdue.due = QDate::currentDate().addDays(-60).toString(Qt::ISODate);
        // 2 meses × 2 % × 300000 = 12000
        QVERIFY(qFuzzyCompare(ReceivablesRepository::mora(overdue), 12000.0));

        Sale future = overdue;
        future.due = QDate::currentDate().addDays(10).toString(Qt::ISODate);
        QCOMPARE(ReceivablesRepository::mora(future), 0.0);

        Sale settled = overdue;
        settled.balance = 0;
        QCOMPARE(ReceivablesRepository::mora(settled), 0.0);
    }

    void docFlow()
    {
        auto cot = m_sales->createDocument(QStringLiteral("Cotización"),
                                           QStringLiteral("Juan Pérez"), 50000,
                                           QStringLiteral("tester"));
        QVERIFY(cot.ok());
        QVERIFY(cot.value().id.startsWith(QStringLiteral("COT")));
        QVERIFY(!m_sales->createDocument(QStringLiteral("Factura X"),
                                         QStringLiteral("Juan Pérez"), 1,
                                         QStringLiteral("tester")).ok());

        auto f1 = m_sales->advanceStatus(cot.value().id, QStringLiteral("Facturada"),
                                         QStringLiteral("tester"));
        QVERIFY(f1.ok());
        auto f2 = m_sales->advanceStatus(cot.value().id, QStringLiteral("Pagada"),
                                         QStringLiteral("tester"));
        QVERIFY(f2.ok());
        QCOMPARE(f2.value().balance, 0.0);
        QCOMPARE(f2.value().paid, f2.value().total);
        QVERIFY(!m_sales->advanceStatus(cot.value().id, QStringLiteral("Volando"),
                                        QStringLiteral("tester")).ok());
        auto cancelled = m_sales->advanceStatus(cot.value().id, QStringLiteral("Cancelada"),
                                                QStringLiteral("tester"));
        QVERIFY(cancelled.ok());
        QVERIFY(!m_sales->advanceStatus(cot.value().id, QStringLiteral("Pagada"),
                                        QStringLiteral("tester")).ok()); // cancelada no avanza
    }

    void creditNoteFlow()
    {
        const auto v1 = m_sales->find(QStringLiteral("V001"));
        QVERIFY(v1.has_value());
        auto nc = m_sales->createCreditNote(QStringLiteral("V001"), 100000,
                                            QStringLiteral("Devolución parcial"),
                                            QStringLiteral("tester"));
        QVERIFY(nc.ok());
        QVERIFY(nc.value().id.startsWith(QStringLiteral("NC")));
        QVERIFY(!m_sales->createCreditNote(QStringLiteral("V001"), v1->total + 1,
                                           QStringLiteral("Motivo"),
                                           QStringLiteral("tester")).ok());
        QVERIFY(!m_sales->createCreditNote(QStringLiteral("V001"), 100,
                                           QStringLiteral("  "),
                                           QStringLiteral("tester")).ok());
        auto nd = m_sales->createDebitNote(QStringLiteral("V001"), 50000,
                                           QStringLiteral("Ajuste"), QStringLiteral("tester"));
        QVERIFY(nd.ok());
        QVERIFY(!m_sales->createDebitNote(QStringLiteral("NOPE"), 10, QStringLiteral("x"),
                                          QStringLiteral("tester")).ok());
    }

    void auditTrail()
    {
        // Las operaciones anteriores debieron dejar rastro
        const auto entries = m_audit->list(500);
        QVERIFY(!entries.isEmpty());
        bool hasCaja = false, hasCompra = false, hasNota = false;
        for (const AuditEntry &e : entries) {
            if (e.action == QLatin1String("caja_apertura"))
                hasCaja = true;
            if (e.action == QLatin1String("compra_recibida"))
                hasCompra = true;
            if (e.action == QLatin1String("nota_credito"))
                hasNota = true;
        }
        QVERIFY(hasCaja && hasCompra && hasNota);
    }

private:
    QTemporaryDir m_tmp;
    DatabaseManager *m_dbm = nullptr;
    AuditRepository *m_audit = nullptr;
    ProductRepository *m_products = nullptr;
    ClientRepository *m_clients = nullptr;
    SupplierRepository *m_suppliers = nullptr;
    PromoRepository *m_promos = nullptr;
    CajaRepository *m_caja = nullptr;
    SaleRepository *m_sales = nullptr;
    InventoryRepository *m_inventory = nullptr;
    PurchaseRepository *m_purchases = nullptr;
    ReceivablesRepository *m_cxc = nullptr;
    PayablesRepository *m_cxp = nullptr;
};

QTEST_MAIN(TstRepositories)
#include "tst_repositories.moc"
