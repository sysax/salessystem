// Controllers de gestión: clientes, proveedores, inventario,
// compras, CxC/CxP, promos y usuarios.
#include <QtTest>

#include "controllers/MgmtControllers.h"
#include "controllers/OpsControllers.h"
#include "core/DatabaseManager.h"
#include "core/EventBus.h"
#include "repositories/AuditRepository.h"
#include "repositories/CajaRepository.h"
#include "repositories/ClientRepository.h"
#include "repositories/CreditRepository.h"
#include "repositories/InventoryRepository.h"
#include "repositories/ProductRepository.h"
#include "repositories/PromoRepository.h"
#include "repositories/PurchaseRepository.h"
#include "repositories/SaleRepository.h"
#include "services/AuthService.h"
#include "services/CreditService.h"
#include "services/InventoryService.h"
#include "services/PurchaseService.h"
#include "services/SalesService.h"
#include "services/SyncService.h"
#include "services/TicketPrinter.h"

#include <QTemporaryDir>

class TstMgmt : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_dbm = new DatabaseManager(this);
        QVERIFY(m_dbm->initialize(m_tmp.filePath(QStringLiteral("mgmt.db"))));
        QSqlDatabase db = m_dbm->database();
        auto *bus = new EventBus(this);
        auto *audit = new AuditRepository(db, this);
        auto *products = new ProductRepository(db, audit, this);
        auto *clients = new ClientRepository(db, audit, this);
        auto *suppliers = new SupplierRepository(db, audit, this);
        auto *promos = new PromoRepository(db, products, audit, this);
        auto *caja = new CajaRepository(db, audit, this);
        auto *sales = new SaleRepository(db, clients, caja, audit, this);
        auto *inventory = new InventoryRepository(db, audit, this);
        auto *purchases = new PurchaseRepository(db, audit, this);
        auto *cxc = new ReceivablesRepository(db, sales, audit, this);
        auto *cxp = new PayablesRepository(db, audit, this);
        auto *auth = new AuthService(db, bus, this);
        auto *invSvc = new InventoryService(db, products, inventory, bus, this);
        auto *purSvc = new PurchaseService(db, purchases, products, suppliers, inventory,
                                           cxp, audit, this);
        auto *cxcSvc = new ReceivablesService(cxc, bus, this);
        auto *cxpSvc = new PayablesService(cxp, bus, this);
        m_clients = new ClientsController(clients, cxcSvc, this);
        m_suppliers = new SuppliersController(suppliers, this);
        m_inventory = new InventoryController(invSvc, inventory, products, this);
        m_purchases = new PurchasesController(purSvc, purchases, this);
        m_cxc = new ReceivablesController(cxcSvc, this);
        m_cxp = new PayablesController(cxpSvc, this);
        m_promos = new PromosController(promos, this);
        m_users = new UsersController(auth, this);
    }

    void clientsFlow()
    {
        QVERIFY(m_clients->add({{"name", "Cliente QML"},
                                {"nit", "900999111-2"},
                                {"city", "Bogotá"}})["ok"].toBool());
        QVERIFY(!m_clients->add({{"name", "X"}})["ok"].toBool()); // corto
        m_clients->search(QStringLiteral("qml"));
        QCOMPARE(m_clients->clients().size(), 1);
        const int id = m_clients->clients().first().toMap()["id"].toInt();
        QVERIFY(m_clients->update(id, {{"phone", "3000000000"}})["ok"].toBool());
        QVERIFY(m_clients->update(id, {{"discount", 150}})["ok"].toBool() == false); // 0-100
        // Estado de cuenta de cliente con deuda seed
        QVERIFY(!m_clients->statement(QStringLiteral("María López")).isEmpty());
    }

    void suppliersFlow()
    {
        QVERIFY(m_suppliers->add({{"name", "Prov QML"}, {"contact", "Ana"}})["ok"].toBool());
        QVERIFY(!m_suppliers->add({{"name", "Prov QML"}})["ok"].toBool()); // duplicado
        m_suppliers->search(QStringLiteral("qml"));
        QCOMPARE(m_suppliers->suppliers().size(), 1);
        const int id = m_suppliers->suppliers().first().toMap()["id"].toInt();
        QVERIFY(m_suppliers->update(id, {{"phone", "6010000000"}})["ok"].toBool());
        QVERIFY(m_suppliers->remove(id)["ok"].toBool());
        QVERIFY(!m_suppliers->remove(id)["ok"].toBool());
    }

    void inventoryFlow()
    {
        auto a = m_inventory->adjust(QStringLiteral("P001"), -2, QStringLiteral("prueba"),
                                     QStringLiteral("tester"));
        QVERIFY(a["ok"].toBool());
        QCOMPARE(a["newStock"].toInt(), 10);
        QVERIFY(!m_inventory->adjust(QStringLiteral("P001"), 0, QStringLiteral("x"),
                                     QStringLiteral("t"))["ok"].toBool());
        QVERIFY(m_inventory->transfer(QStringLiteral("P001"), 1, QStringLiteral("Z9"),
                                      QStringLiteral("prueba"),
                                      QStringLiteral("tester"))["ok"].toBool());
        QVERIFY(m_inventory->valuation()["totalValue"].toDouble() > 0);
        QVERIFY(!m_inventory->alerts().isEmpty());
    }

    void purchasesFlow()
    {
        auto c = m_purchases->create(QStringLiteral("OfiSurte SAS"), QStringLiteral("P007"), 1,
                                     QStringLiteral("tester"));
        QVERIFY(c["ok"].toBool());
        QVERIFY(!m_purchases->orders().isEmpty());
        QVERIFY(m_purchases->receive(c["id"].toString(), QStringLiteral("tester"))["ok"]
                    .toBool());
        QVERIFY(!m_purchases->cancel(c["id"].toString(), QStringLiteral("tester"))["ok"]
                     .toBool());
    }

    void receivablesFlow()
    {
        QVERIFY(!m_cxc->pending().isEmpty()); // V002/V003 seed con saldo
        const QString id = m_cxc->pending().first().toMap()["id"].toString();
        const double bal = m_cxc->pending().first().toMap()["balance"].toDouble();
        auto p = m_cxc->pay(id, bal, QStringLiteral("Efectivo"), QStringLiteral("tester"));
        QVERIFY(p["ok"].toBool());
        QCOMPARE(p["balance"].toDouble(), 0.0);
    }

    void payablesFlow()
    {
        QVERIFY(!m_cxp->pending().isEmpty()); // seed trae CxP
        const QString id = m_cxp->pending().first().toMap()["id"].toString();
        const double bal = m_cxp->pending().first().toMap()["balance"].toDouble();
        auto p = m_cxp->pay(id, bal / 2, QStringLiteral("Transferencia"),
                            QStringLiteral("tester"));
        QVERIFY(p["ok"].toBool());
        QVERIFY(p["balance"].toDouble() > 0);
        auto p2 = m_cxp->pay(id, p["balance"].toDouble(), QStringLiteral("Transferencia"),
                             QStringLiteral("tester"));
        QVERIFY(p2["ok"].toBool());
        QCOMPARE(p2["balance"].toDouble(), 0.0);
    }

    void promosFlow()
    {
        const int before = m_promos->promos().size();
        QVERIFY(m_promos->add({{"code", "QML20"},
                               {"name", "QML 20%"},
                               {"type", "porcentaje"},
                               {"value", 20.0}})["ok"].toBool());
        QCOMPARE(m_promos->promos().size(), before + 1);
        int newId = 0;
        for (const QVariant &v : m_promos->promos()) {
            if (v.toMap()["code"].toString() == QLatin1String("QML20"))
                newId = v.toMap()["id"].toInt();
        }
        QVERIFY(newId > 0);
        QVERIFY(m_promos->setActive(newId, false)["ok"].toBool());
        QVERIFY(m_promos->remove(newId)["ok"].toBool());
    }

    void usersFlow()
    {
        QCOMPARE(m_users->users().size(), 5);
        QVERIFY(m_users->add(QStringLiteral("qmluser"), QStringLiteral("qml1234"),
                             QStringLiteral("Vendedor"))["ok"].toBool());
        QVERIFY(!m_users->add(QStringLiteral("qmluser"), QStringLiteral("otra1234"),
                              QStringLiteral("Vendedor"))["ok"].toBool());
        QVERIFY(m_users->setActive(QStringLiteral("qmluser"), false)["ok"].toBool());
        QVERIFY(m_users->resetPassword(QStringLiteral("qmluser"),
                                       QStringLiteral("nueva1234"))["ok"].toBool());
        QVERIFY(m_users->remove(QStringLiteral("qmluser"))["ok"].toBool());
        QVERIFY(!m_users->remove(QStringLiteral("admin"))["ok"].toBool());
    }

private:
    QTemporaryDir m_tmp;
    DatabaseManager *m_dbm = nullptr;
    ClientsController *m_clients = nullptr;
    SuppliersController *m_suppliers = nullptr;
    InventoryController *m_inventory = nullptr;
    PurchasesController *m_purchases = nullptr;
    ReceivablesController *m_cxc = nullptr;
    PayablesController *m_cxp = nullptr;
    PromosController *m_promos = nullptr;
    UsersController *m_users = nullptr;
};

QTEST_MAIN(TstMgmt)
#include "tst_mgmt.moc"
