// Controllers end-to-end: auth + POS checkout + catálogo + ventas.
#include <QtTest>

#include "controllers/AuthController.h"
#include "controllers/CatalogController.h"
#include "controllers/PosController.h"
#include "controllers/SalesController.h"
#include "core/DatabaseManager.h"
#include "core/EventBus.h"
#include "repositories/AuditRepository.h"
#include "repositories/CajaRepository.h"
#include "repositories/ClientRepository.h"
#include "repositories/InventoryRepository.h"
#include "repositories/ProductRepository.h"
#include "repositories/PromoRepository.h"
#include "repositories/SaleRepository.h"
#include "services/AuthService.h"
#include "services/SalesService.h"
#include "services/SyncService.h"
#include "services/TicketPrinter.h"

#include <QTemporaryDir>

class TstControllers : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_dbm = new DatabaseManager(this);
        QVERIFY(m_dbm->initialize(m_tmp.filePath(QStringLiteral("ctl.db"))));
        QSqlDatabase db = m_dbm->database();
        auto *bus = new EventBus(this);
        auto *audit = new AuditRepository(db, this);
        auto *products = new ProductRepository(db, audit, this);
        auto *clients = new ClientRepository(db, audit, this);
        auto *caja = new CajaRepository(db, audit, this);
        auto *sales = new SaleRepository(db, clients, caja, audit, this);
        auto *inventory = new InventoryRepository(db, audit, this);
        auto *promos = new PromoRepository(db, products, audit, this);
        auto *authSvc = new AuthService(db, bus, this);
        auto *salesSvc = new SalesService(db, products, sales, inventory, clients, caja,
                                          promos, bus, this);
        auto *sync = new SyncService(db, bus, this);
        auto *printer = new TicketPrinter(m_tmp.path(), this);
        m_auth = new AuthController(authSvc, this);
        m_pos = new PosController(salesSvc, products, promos, caja, printer, sync, this);
        m_catalog = new CatalogController(products, this);
        m_salesCtl = new SalesController(sales, salesSvc, this);
    }

    void authFlow()
    {
        QVERIFY(!m_auth->loggedIn());
        auto bad = m_auth->login(QStringLiteral("admin"), QStringLiteral("mala"));
        QVERIFY(!bad["ok"].toBool());
        auto ok = m_auth->login(QStringLiteral("admin"), QStringLiteral("admin123"));
        QVERIFY(ok["ok"].toBool());
        QVERIFY(m_auth->loggedIn());
        QCOMPARE(m_auth->currentRole(), QStringLiteral("Administrador"));
        QVERIFY(m_auth->canAccess(QStringLiteral("users")));
        QVERIFY(m_auth->canAccess(QStringLiteral("login")));
        m_auth->logout();
        QVERIFY(!m_auth->loggedIn());
        QVERIFY(!m_auth->canAccess(QStringLiteral("pos")));
    }

    void posCheckout()
    {
        m_auth->login(QStringLiteral("cajero"), QStringLiteral("caja123"));
        QVERIFY(m_pos->openCaja(50000, QStringLiteral("cajero"))["ok"].toBool());
        QVERIFY(m_pos->addToCart(2, 1)["ok"].toBool());
        QVERIFY(m_pos->addToCart(2, 1)["ok"].toBool()); // acumula
        QCOMPARE(m_pos->cart().size(), 1);
        QCOMPARE(m_pos->cart().first().toMap()["qty"].toInt(), 2);
        QVERIFY(m_pos->totals()["total"].toDouble() > 0);

        QVariantMap r = m_pos->checkout(QStringLiteral("Mostrador"),
                                        {{"efectivo", 200000.0}}, QStringLiteral("Efectivo"),
                                        QStringLiteral("cajero"));
        QVERIFY(r["ok"].toBool());
        QVERIFY(!r["saleId"].toString().isEmpty());
        QVERIFY(!r["ticket"].toString().isEmpty());
        QVERIFY(QFile::exists(r["ticket"].toString()));
        QCOMPARE(m_pos->cart().size(), 0);
        QCOMPARE(m_pos->pendingSync(), 1); // encolado offline-first
        m_auth->logout();
    }

    void catalogFlow()
    {
        m_catalog->search(QStringLiteral("mouse"));
        QVERIFY(!m_catalog->products().isEmpty());
        QVariantMap add = m_catalog->add({{"sku", "CTL1"},
                                         {"name", "Control Test"},
                                         {"price", 5000.0},
                                         {"stock", 3}});
        QVERIFY(add["ok"].toBool());
        QVariantMap bad = m_catalog->add({{"sku", "CTL1"},
                                          {"name", "Dup"},
                                          {"price", 1.0},
                                          {"stock", 1}});
        QVERIFY(!bad["ok"].toBool());
        QVERIFY(m_catalog->remove(QStringLiteral("CTL1"))["ok"].toBool());
    }

    void salesFlow()
    {
        QVERIFY(!m_salesCtl->sales().isEmpty());
        const QString id = m_salesCtl->sales().first().toMap()["id"].toString();
        const QVariantMap d = m_salesCtl->detail(id);
        QVERIFY(d["ok"].toBool());
        QVERIFY(d.contains("items"));
    }

private:
    QTemporaryDir m_tmp;
    DatabaseManager *m_dbm = nullptr;
    AuthController *m_auth = nullptr;
    PosController *m_pos = nullptr;
    CatalogController *m_catalog = nullptr;
    SalesController *m_salesCtl = nullptr;
};

QTEST_MAIN(TstControllers)
#include "tst_controllers.moc"
