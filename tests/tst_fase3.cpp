// Fase 3 (MAP_PRO.md): políticas de negocio + config externalizada + permisos.
// - Config: defaults y validación de SettingsService (credit_days,
//   payable_days, default_credit_limit, fixed_costs_monthly,
//   promo_volumen_min_qty, folio_series_json, mora_rate_monthly).
// - Cableado: SalesService usa credit_days, PurchaseService usa
//   payable_days, SaleRepository usa folio_series_json, PromoRepository
//   usa promo_volumen_min_qty, ReceivablesService usa mora_rate_monthly.
// - Permisos: AuthService::canAccess == Permissions::canAccess.
// - Regresión: límite de crédito vigente (SalesService::create).
#include <QtTest>

#include <QDate>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "TestDb.h"
#include "core/DatabaseManager.h"
#include "core/EventBus.h"
#include "core/Money.h"
#include "core/Permissions.h"
#include "repositories/AuditRepository.h"
#include "repositories/CajaRepository.h"
#include "repositories/ClientRepository.h"
#include "repositories/CreditRepository.h"
#include "repositories/InventoryRepository.h"
#include "repositories/ProductRepository.h"
#include "repositories/PromoRepository.h"
#include "repositories/PurchaseRepository.h"
#include "repositories/SaleRepository.h"
#include "repositories/SettingsRepository.h"
#include "services/AuthService.h"
#include "services/CreditService.h"
#include "services/PurchaseService.h"
#include "services/SalesService.h"
#include "services/SettingsService.h"

using SI = SalesService::ServiceItem;

class TstFase3 : public QObject
{
    Q_OBJECT

  private slots:
    void configDefaults()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager dbm;
        QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("f3_defaults.db"))));
        SettingsRepository repo(dbm.database());
        SettingsService svc(&repo, nullptr);

        QCOMPARE(svc.creditDays(), 15);
        QCOMPARE(svc.payableDays(), 30);
        QCOMPARE(svc.defaultCreditLimit(), Money::fromCop(5000000.0));
        QCOMPARE(svc.fixedCostsMonthly(), Money::fromCop(5000000.0));
        QCOMPARE(svc.promoVolumenMinQty(), 10);

        const auto series = svc.folioSeries();
        QCOMPARE(series.size(), 6);
        QCOMPARE(series[QStringLiteral("Cotización")].prefix, QStringLiteral("COT"));
        QCOMPARE(series[QStringLiteral("Pedido")].prefix, QStringLiteral("PED"));
        QCOMPARE(series[QStringLiteral("Remisión")].prefix, QStringLiteral("REM"));
        QCOMPARE(series[QStringLiteral("Factura")].prefix, QStringLiteral("FE"));
        QCOMPARE(series[QStringLiteral("Nota crédito")].prefix, QStringLiteral("NC"));
        QCOMPARE(series[QStringLiteral("Nota cargo")].prefix, QStringLiteral("ND"));
    }

    void configValidation()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager dbm;
        QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("f3_validation.db"))));
        SettingsRepository repo(dbm.database());
        SettingsService svc(&repo, nullptr);

        QVERIFY(!svc.save({{QStringLiteral("credit_days"), QStringLiteral("-1")}})
                     [QStringLiteral("ok")]
                         .toBool());
        QVERIFY(!svc.save({{QStringLiteral("payable_days"), QStringLiteral("400")}})
                     [QStringLiteral("ok")]
                         .toBool());
        QVERIFY(!svc.save({{QStringLiteral("promo_volumen_min_qty"), QStringLiteral("0")}})
                     [QStringLiteral("ok")]
                         .toBool());
        QVERIFY(!svc.save({{QStringLiteral("folio_series_json"), QStringLiteral("roto{")}})
                     [QStringLiteral("ok")]
                         .toBool());
        QVERIFY(!svc.save({{QStringLiteral("mora_rate_monthly"), QStringLiteral("101")}})
                     [QStringLiteral("ok")]
                         .toBool());
        // Los rechazos no mutan lo previo (defaults intactos).
        QCOMPARE(svc.creditDays(), 15);
        QCOMPARE(svc.payableDays(), 30);
        QCOMPARE(svc.promoVolumenMinQty(), 10);
        QCOMPARE(svc.moraRate(), 2.0);

        // JSON roto directo en BD → folioSeries() fail-safe a defaults.
        QVERIFY(svc.save({{QStringLiteral("folio_series_json"),
                           QStringLiteral("{\"Factura\":{\"prefix\":\"FV\",\"counter\":\"FV_C\"}}")}})
                    [QStringLiteral("ok")]
                        .toBool());
        QCOMPARE(svc.folioSeries()[QStringLiteral("Factura")].prefix, QStringLiteral("FV"));
        QSqlQuery q(dbm.database());
        QVERIFY(q.exec(QStringLiteral("UPDATE settings SET value='roto{' WHERE "
                                      "key='folio_series_json'")));
        const auto series = svc.folioSeries();
        QCOMPARE(series.size(), 6);
        QCOMPARE(series[QStringLiteral("Factura")].prefix, QStringLiteral("FE"));
    }

    void creditDaysApplied()
    {
        // Con credit_days=7 la venta a crédito vence hoy+7.
        {
            QTemporaryDir tmp;
            QVERIFY(tmp.isValid());
            DatabaseManager dbm;
            QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("f3_credit7.db"))));
            QSqlDatabase db = dbm.database();
            AuditRepository audit(db);
            ProductRepository products(db, &audit);
            ClientRepository clients(db, &audit);
            CajaRepository caja(db, &audit);
            SaleRepository sales(db, &clients, &caja, &audit);
            InventoryRepository inventory(db, &audit);
            PromoRepository promos(db, &products, &audit);
            SettingsRepository settingsRepo(db);
            EventBus bus;
            SettingsService settings(&settingsRepo, &bus);
            QVERIFY(settings.save({{QStringLiteral("credit_days"), QStringLiteral("7")}})
                        [QStringLiteral("ok")]
                            .toBool());
            SalesService svc(db, &products, &sales, &inventory, &clients, &caja, &promos,
                             &bus, &settings, &audit, nullptr);

            const int pid = mkProduct(products, QStringLiteral("F3C7"));
            mkClient(clients, QStringLiteral("Credito Siete"), 50000000.0);
            auto r = svc.create({SI{.productId = pid, .qty = 1.0}},
                                QStringLiteral("Credito Siete"), {}, QStringLiteral("Credito"),
                                QString(), QStringLiteral("tester"));
            QVERIFY(r.ok());
            const auto s = sales.find(r.value().id);
            QVERIFY(s.has_value());
            QCOMPARE(s->status, QStringLiteral("Pendiente"));
            QCOMPARE(s->due, QDate::currentDate().addDays(7).toString(Qt::ISODate));
        }
        // Sin settings (nullptr) → default 15.
        {
            QTemporaryDir tmp;
            QVERIFY(tmp.isValid());
            DatabaseManager dbm;
            QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("f3_credit15.db"))));
            QSqlDatabase db = dbm.database();
            AuditRepository audit(db);
            ProductRepository products(db, &audit);
            ClientRepository clients(db, &audit);
            CajaRepository caja(db, &audit);
            SaleRepository sales(db, &clients, &caja, &audit);
            InventoryRepository inventory(db, &audit);
            PromoRepository promos(db, &products, &audit);
            EventBus bus;
            SalesService svc(db, &products, &sales, &inventory, &clients, &caja, &promos,
                             &bus, nullptr, &audit, nullptr);

            const int pid = mkProduct(products, QStringLiteral("F3C15"));
            mkClient(clients, QStringLiteral("Credito Quince"), 50000000.0);
            auto r = svc.create({SI{.productId = pid, .qty = 1.0}},
                                QStringLiteral("Credito Quince"), {}, QStringLiteral("Credito"),
                                QString(), QStringLiteral("tester"));
            QVERIFY(r.ok());
            const auto s = sales.find(r.value().id);
            QVERIFY(s.has_value());
            QCOMPARE(s->due, QDate::currentDate().addDays(15).toString(Qt::ISODate));
        }
    }

    void payableDaysApplied()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager dbm;
        QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("f3_payable.db"))));
        QSqlDatabase db = dbm.database();
        AuditRepository audit(db);
        ProductRepository products(db, &audit);
        SupplierRepository suppliers(db, &audit);
        InventoryRepository inventory(db, &audit);
        PurchaseRepository purchases(db, &audit);
        PayablesRepository payables(db, &audit);
        SettingsRepository settingsRepo(db);
        SettingsService settings(&settingsRepo, nullptr);
        QVERIFY(settings.save({{QStringLiteral("payable_days"), QStringLiteral("10")}})
                    [QStringLiteral("ok")]
                        .toBool());
        PurchaseService svc(db, &purchases, &products, &suppliers, &inventory, &payables,
                            &audit, this, &settings);

        Supplier sup;
        sup.name = QStringLiteral("Proveedor F3");
        QVERIFY(suppliers.add(sup).ok());
        Product p;
        p.sku = QStringLiteral("F3PAY");
        p.name = QStringLiteral("Insumo F3");
        p.price = Money::fromCop(20000.0);
        p.priceBuy = Money::fromCop(14000.0);
        p.stock = 0;
        QVERIFY(products.add(p).ok());

        auto c = svc.create(QStringLiteral("Proveedor F3"), QStringLiteral("F3PAY"), 3,
                            QStringLiteral("tester"));
        QVERIFY(c.ok());
        auto r = svc.receive(c.value().id, QStringLiteral("tester"));
        QVERIFY(r.ok());
        const auto cxp = payables.find(c.value().id);
        QVERIFY(cxp.has_value());
        QCOMPARE(cxp->due, QDate::currentDate().addDays(10).toString(Qt::ISODate));
    }

    void folioSeriesOverride()
    {
        // Con override Factura→FV el folio empieza con "FV".
        {
            QTemporaryDir tmp;
            QVERIFY(tmp.isValid());
            DatabaseManager dbm;
            QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("f3_foliofv.db"))));
            QSqlDatabase db = dbm.database();
            AuditRepository audit(db);
            ClientRepository clients(db, &audit);
            CajaRepository caja(db, &audit);
            SaleRepository sales(db, &clients, &caja, &audit);
            SettingsRepository settingsRepo(db);
            SettingsService settings(&settingsRepo, nullptr);
            QVERIFY(settings
                        .save({{QStringLiteral("folio_series_json"),
                                QStringLiteral("{\"Factura\":{\"prefix\":\"FV\",\"counter\":"
                                               "\"FV_COUNTER\"}}")}})
                            [QStringLiteral("ok")]
                                .toBool());
            sales.setSettings(&settings);
            auto d = sales.createDocument(QStringLiteral("Factura"),
                                          QStringLiteral("Juan Pérez"), Money::fromCop(1000.0),
                                          QStringLiteral("tester"));
            QVERIFY(d.ok());
            QVERIFY(d.value().id.startsWith(QStringLiteral("FV")));
        }
        // Sin settings → "FE".
        {
            QTemporaryDir tmp;
            QVERIFY(tmp.isValid());
            DatabaseManager dbm;
            QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("f3_foliofe.db"))));
            QSqlDatabase db = dbm.database();
            AuditRepository audit(db);
            ClientRepository clients(db, &audit);
            CajaRepository caja(db, &audit);
            SaleRepository sales(db, &clients, &caja, &audit);
            auto d = sales.createDocument(QStringLiteral("Factura"),
                                          QStringLiteral("Juan Pérez"), Money::fromCop(1000.0),
                                          QStringLiteral("tester"));
            QVERIFY(d.ok());
            QVERIFY(d.value().id.startsWith(QStringLiteral("FE")));
        }
    }

    void volumenThreshold()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager dbm;
        QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("f3_volumen.db"))));
        QSqlDatabase db = dbm.database();
        AuditRepository audit(db);
        ProductRepository products(db, &audit);
        PromoRepository promos(db, &products, &audit);
        SettingsRepository settingsRepo(db);
        SettingsService settings(&settingsRepo, nullptr);
        promos.setSettings(&settings);

        Promo p;
        p.name = QStringLiteral("Volumen F3");
        p.type = QStringLiteral("volumen");
        p.value = 10.0;
        p.code = QStringLiteral("VOLF3");
        QVERIFY(promos.add(p).ok());

        // Carrito: 5 uds × $10.000 = $50.000.
        const QList<CartLine> cart{
            CartLine{.productId = 1,
                     .sku = QStringLiteral("F3V"),
                     .price = Money::fromCop(10000.0),
                     .qty = 5.0,
                     .subtotal = Money::fromCop(50000.0)},
        };
        // Umbral alto (100): bajo el umbral → descuento 0.
        QVERIFY(settings.save({{QStringLiteral("promo_volumen_min_qty"), QStringLiteral("100")}})
                    [QStringLiteral("ok")]
                        .toBool());
        auto below = promos.evaluate(cart, QStringLiteral("VOLF3"));
        QVERIFY(below.ok());
        QVERIFY(below.value().discount.isZero());
        // Umbral bajo (2): sobre el umbral → 10 % de 50000 = 5000.
        QVERIFY(settings.save({{QStringLiteral("promo_volumen_min_qty"), QStringLiteral("2")}})
                    [QStringLiteral("ok")]
                        .toBool());
        auto above = promos.evaluate(cart, QStringLiteral("VOLF3"));
        QVERIFY(above.ok());
        QCOMPARE(above.value().discount, Money::fromCop(5000.0));
    }

    void moraFromSettings()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager dbm;
        QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("f3_mora.db"))));
        QSqlDatabase db = dbm.database();
        AuditRepository audit(db);
        ProductRepository products(db, &audit);
        ClientRepository clients(db, &audit);
        CajaRepository caja(db, &audit);
        SaleRepository sales(db, &clients, &caja, &audit);
        InventoryRepository inventory(db, &audit);
        PromoRepository promos(db, &products, &audit);
        ReceivablesRepository cxc(db, &sales, &audit);
        SettingsRepository settingsRepo(db);
        EventBus bus;
        SettingsService settings(&settingsRepo, &bus);
        ReceivablesService cxcSvc(&cxc, &bus, this, &settings);
        ReceivablesService cxcNoSettings(&cxc, &bus, this, nullptr);

        // Venta vencida con saldo: 60 días ≈ 2 meses × 300000.
        Sale overdue;
        overdue.balance = Money::fromCop(300000.0);
        overdue.due = QDate::currentDate().addDays(-60).toString(Qt::ISODate);
        QCOMPARE(ReceivablesRepository::mora(overdue, 0.02), Money::fromCop(12000.0));
        QVERIFY(ReceivablesRepository::mora(overdue, 0.02)
                < ReceivablesRepository::mora(overdue, 0.10));

        // Cableado del servicio: default 2 % (fracción 0.02), tras save 10 %.
        QCOMPARE(cxcNoSettings.moraMonthlyRate(), 0.02);
        QCOMPARE(cxcSvc.moraMonthlyRate(), 0.02);
        QVERIFY(settings.save({{QStringLiteral("mora_rate_monthly"), QStringLiteral("10")}})
                    [QStringLiteral("ok")]
                        .toBool());
        QCOMPARE(cxcSvc.moraMonthlyRate(), 0.10);

        // pending() expone "mora" (vía saleToMap) sobre una venta con saldo.
        const int pid = mkProduct(products, QStringLiteral("F3MORA"));
        mkClient(clients, QStringLiteral("Mora Tester"), 50000000.0);
        SalesService svc(db, &products, &sales, &inventory, &clients, &caja, &promos, &bus,
                         &settings, &audit, nullptr);
        auto r = svc.create({SI{.productId = pid, .qty = 1.0}},
                            QStringLiteral("Mora Tester"), {}, QStringLiteral("Credito"),
                            QString(), QStringLiteral("tester"));
        QVERIFY(r.ok());
        const QVariantList pend = cxcSvc.pending();
        QVERIFY(!pend.isEmpty());
        QVERIFY(pend.first().toMap().contains(QStringLiteral("mora")));
    }

    void permissionsMatrix()
    {
        AuthService auth(QSqlDatabase(), nullptr);
        const QStringList screens{QStringLiteral("dashboard"), QStringLiteral("pos"),
                                 QStringLiteral("reports"),   QStringLiteral("users"),
                                 QStringLiteral("settings"),  QStringLiteral("audit")};
        for (const QString &role : Permissions::roles()) {
            for (const QString &screen : screens)
                QCOMPARE(auth.canAccess(role, screen), Permissions::canAccess(role, screen));
        }
        // Rol desconocido → false en todo.
        for (const QString &screen : screens) {
            QVERIFY(!auth.canAccess(QStringLiteral("Fantasma"), screen));
            QVERIFY(!Permissions::canAccess(QStringLiteral("Fantasma"), screen));
        }
        // Solo Administrador es admin.
        for (const QString &role : Permissions::roles())
            QCOMPARE(Permissions::isAdmin(role), role == QStringLiteral("Administrador"));
        QVERIFY(!Permissions::isAdmin(QStringLiteral("Fantasma")));
        // Vendedor: pos sí, reports no.
        const QStringList vend = Permissions::screensForRole(QStringLiteral("Vendedor"));
        QVERIFY(vend.contains(QStringLiteral("pos")));
        QVERIFY(!vend.contains(QStringLiteral("reports")));
    }

    void creditLimitPolicy()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        DatabaseManager dbm;
        QVERIFY(dbm.initialize(tmp.filePath(QStringLiteral("f3_limit.db"))));
        QSqlDatabase db = dbm.database();
        AuditRepository audit(db);
        ProductRepository products(db, &audit);
        ClientRepository clients(db, &audit);
        CajaRepository caja(db, &audit);
        SaleRepository sales(db, &clients, &caja, &audit);
        InventoryRepository inventory(db, &audit);
        PromoRepository promos(db, &products, &audit);
        EventBus bus;
        SalesService svc(db, &products, &sales, &inventory, &clients, &caja, &promos, &bus,
                         nullptr, nullptr, nullptr);

        // P002-like: $10.000 + IVA 19 % = $11.900 por unidad.
        const int pid = mkProduct(products, QStringLiteral("F3LIM"));
        Client c;
        c.name = QStringLiteral("Limite F3");
        c.creditLimit = Money::fromCop(20000.0);
        QVERIFY(clients.add(c).ok());
        QVERIFY(svc.create({SI{.productId = pid, .qty = 1.0}},
                           QStringLiteral("Limite F3"), {}, QStringLiteral("Credito"),
                           QString(), QStringLiteral("tester"))
                    .ok());
        // Saldo 11900 + otros 11900 > 20000 → bloqueada.
        QVERIFY(!svc.create({SI{.productId = pid, .qty = 1.0}},
                            QStringLiteral("Limite F3"), {}, QStringLiteral("Credito"),
                            QString(), QStringLiteral("tester"))
                     .ok());
        // Sin límite (0) → crédito bloqueado; contado sí pasa.
        Client z;
        z.name = QStringLiteral("Sinlimite F3");
        z.creditLimit = Money();
        QVERIFY(clients.add(z).ok());
        QVERIFY(!svc.create({SI{.productId = pid, .qty = 1.0}},
                            QStringLiteral("Sinlimite F3"), {}, QStringLiteral("Credito"),
                            QString(), QStringLiteral("tester"))
                     .ok());
        QVERIFY(svc.create({SI{.productId = pid, .qty = 1.0}},
                           QStringLiteral("Sinlimite F3"), {}, QStringLiteral("Efectivo"),
                           QString(), QStringLiteral("tester"))
                    .ok());
    }

  private:
    static int mkProduct(ProductRepository &products, const QString &sku)
    {
        if (const auto ex = products.findBySku(sku))
            return ex->id;
        Product p;
        p.sku = sku;
        p.name = QStringLiteral("Producto ") + sku;
        p.price = Money::fromCop(10000.0);
        p.stock = 100;
        const auto r = products.add(p);
        if (!r.ok()) {
            QTest::qFail(qPrintable(QStringLiteral("mkProduct add: ") + r.error()), __FILE__,
                         __LINE__);
            return -1;
        }
        return r.value().id;
    }

    static void mkClient(ClientRepository &clients, const QString &name, double limitCop)
    {
        Client c;
        c.name = name;
        c.creditLimit = Money::fromCop(limitCop);
        if (!clients.add(c).ok()) {
            QTest::qFail(qPrintable(QStringLiteral("mkClient add: ") + name), __FILE__,
                         __LINE__);
        }
    }
};

QTEST_MAIN(TstFase3)
#include "tst_fase3.moc"
