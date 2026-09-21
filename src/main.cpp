// QtSalesSystem — punto de entrada Qt6/QML (C++20).
// Arquitectura (arquitectura.txt): QML -> Controllers -> Services ->
// Repositories -> SQLite. El grafo de dependencias se cablea aquí
// (DI manual, sin framework).
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "controllers/AuthController.h"
#include "controllers/CatalogController.h"
#include "controllers/DashboardController.h"
#include "controllers/MgmtControllers.h"
#include "controllers/OpsControllers.h"
#include "controllers/PosController.h"
#include "controllers/SalesController.h"
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
#include "services/ReportService.h"
#include "services/SalesService.h"
#include "services/SyncService.h"
#include "services/TicketPrinter.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("QtSalesSystem"));
    app.setApplicationName(QStringLiteral("QtSalesSystem"));

    DatabaseManager db;
    const QString overridePath = QString::fromLocal8Bit(qgetenv("QTSALES_DB"));
    if (!db.initialize(overridePath)) {
        qWarning("DatabaseManager: %s", qPrintable(db.statusMessage()));
        return -1;
    }
    const QSqlDatabase conn = db.database();

    EventBus bus;
    AuditRepository audit(conn);
    ProductRepository products(conn, &audit);
    ClientRepository clients(conn, &audit);
    SupplierRepository suppliers(conn, &audit);
    PromoRepository promos(conn, &products, &audit);
    CajaRepository caja(conn, &audit);
    SaleRepository sales(conn, &clients, &caja, &audit);
    InventoryRepository inventory(conn, &audit);
    PurchaseRepository purchases(conn, &audit);
    ReceivablesRepository cxc(conn, &sales, &audit);
    PayablesRepository cxp(conn, &audit);

    AuthService auth(conn, &bus);
    SalesService salesSvc(conn, &products, &sales, &inventory, &clients, &caja, &promos,
                          &bus);
    InventoryService invSvc(conn, &products, &inventory, &bus);
    PurchaseService purSvc(conn, &purchases, &products, &suppliers, &inventory, &cxp,
                           &audit);
    ReportService reports(conn);
    SyncService sync(conn, &bus);
    ReceivablesService cxcSvc(&cxc, &bus);
    PayablesService cxpSvc(&cxp, &bus);
    TicketPrinter printer;

    AuthController authCtl(&auth);
    DashboardController dashCtl(&reports, &inventory);
    PosController posCtl(&salesSvc, &products, &promos, &caja, &printer, &sync);
    CatalogController catalogCtl(&products);
    SalesController salesCtl(&sales, &salesSvc);
    ClientsController clientsCtl(&clients, &cxcSvc);
    SuppliersController suppliersCtl(&suppliers);
    InventoryController inventoryCtl(&invSvc, &inventory, &products);
    PurchasesController purchasesCtl(&purSvc, &purchases);
    ReceivablesController cxcCtl(&cxcSvc);
    PayablesController cxpCtl(&cxpSvc);
    PromosController promosCtl(&promos);
    UsersController usersCtl(&auth);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("db"), &db);
    engine.rootContext()->setContextProperty(QStringLiteral("auth"), &authCtl);
    engine.rootContext()->setContextProperty(QStringLiteral("dash"), &dashCtl);
    engine.rootContext()->setContextProperty(QStringLiteral("pos"), &posCtl);
    engine.rootContext()->setContextProperty(QStringLiteral("catalog"), &catalogCtl);
    engine.rootContext()->setContextProperty(QStringLiteral("salesCtl"), &salesCtl);
    engine.rootContext()->setContextProperty(QStringLiteral("syncSvc"), &sync);
    engine.rootContext()->setContextProperty(QStringLiteral("reports"), &reports);
    engine.rootContext()->setContextProperty(QStringLiteral("clientsCtl"), &clientsCtl);
    engine.rootContext()->setContextProperty(QStringLiteral("suppliersCtl"), &suppliersCtl);
    engine.rootContext()->setContextProperty(QStringLiteral("inventoryCtl"), &inventoryCtl);
    engine.rootContext()->setContextProperty(QStringLiteral("purchasesCtl"), &purchasesCtl);
    engine.rootContext()->setContextProperty(QStringLiteral("cxcCtl"), &cxcCtl);
    engine.rootContext()->setContextProperty(QStringLiteral("cxpCtl"), &cxpCtl);
    engine.rootContext()->setContextProperty(QStringLiteral("promosCtl"), &promosCtl);
    engine.rootContext()->setContextProperty(QStringLiteral("usersCtl"), &usersCtl);

    engine.loadFromModule(QStringLiteral("QtSalesSystem"), QStringLiteral("Main"));
    if (engine.rootObjects().isEmpty())
        return -1;
    return app.exec();
}
