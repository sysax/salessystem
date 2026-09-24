// Fase 3: verticales — attrs, farmacia (lote/vencimiento/receta/controlado),
// celulares (seriales/garantía/RMA), EAN-13.
// Fase 4: mermas, reporte de seriales, SerialsController.
#include <QtTest>

#include "controllers/CatalogController.h"
#include "controllers/SerialsController.h"
#include "core/DatabaseManager.h"
#include "core/EventBus.h"
#include "domain/Attrs.h"
#include "repositories/AuditRepository.h"
#include "repositories/CajaRepository.h"
#include "repositories/ClientRepository.h"
#include "repositories/InventoryRepository.h"
#include "repositories/ProductRepository.h"
#include "repositories/PromoRepository.h"
#include "repositories/SaleRepository.h"
#include "repositories/SerialRepository.h"
#include "repositories/SettingsRepository.h"
#include "services/InventoryService.h"
#include "services/ReportService.h"
#include "services/SalesService.h"
#include "services/SettingsService.h"

#include <QDate>
#include <QSqlQuery>
#include <QTemporaryDir>

using SI = SalesService::ServiceItem;

class TstVertical : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_dbm = new DatabaseManager(this);
        QVERIFY(m_dbm->initialize(m_tmp.filePath(QStringLiteral("vertical.db"))));
        QSqlDatabase db = m_dbm->database();
        auto *bus = new EventBus(this);
        auto *audit = new AuditRepository(db, this);
        m_products = new ProductRepository(db, audit, this);
        auto *clients = new ClientRepository(db, audit, this);
        m_clients = clients;
        auto *caja = new CajaRepository(db, audit, this);
        auto *sales = new SaleRepository(db, clients, caja, audit, this);
        auto *inventory = new InventoryRepository(db, audit, this);
        m_inventory = inventory;
        auto *promos = new PromoRepository(db, m_products, audit, this);
        auto *settingsRepo = new SettingsRepository(db, this);
        m_settings = new SettingsService(settingsRepo, bus, this);
        m_serials = new SerialRepository(db, this);
        m_svc = new SalesService(db, m_products, sales, inventory, clients, caja,
                                 promos, bus, m_settings, audit, m_serials, this);
        m_ctl = new CatalogController(m_products, nullptr, m_settings, this);
        m_db = db;
    }

    void attrsRoundTrip()
    {
        QVERIFY(Attrs::isObject(QStringLiteral("{}")));
        QVERIFY(Attrs::isObject(QStringLiteral("")));
        QVERIFY(!Attrs::isObject(QStringLiteral("[1,2]")));
        QVERIFY(!Attrs::isObject(QStringLiteral("roto")));
        const QString a = Attrs::set(QStringLiteral("{}"), Attrs::KTrackSerial, true);
        QVERIFY(Attrs::boolean(a, Attrs::KTrackSerial));
        QCOMPARE(Attrs::integer(Attrs::set(a, Attrs::KWarrantyMonths, 24),
                                Attrs::KWarrantyMonths, 12),
                 24);
        Product p;
        p.sku = QStringLiteral("AT1");
        p.name = QStringLiteral("Con attrs");
        p.price = 1000.0;
        p.stock = 5.0;
        p.attrsJson = a;
        QVERIFY(m_products->add(p).ok());
        QVERIFY(Attrs::boolean(m_products->findBySku(QStringLiteral("AT1"))->attrsJson,
                               Attrs::KTrackSerial));
        // attrs no-objeto rechazado
        Product bad = p;
        bad.sku = QStringLiteral("AT2");
        bad.attrsJson = QStringLiteral("[1]");
        QVERIFY(!m_products->add(bad).ok());
        // vencimiento malformado rechazado
        Product badDate = p;
        badDate.sku = QStringLiteral("AT3");
        badDate.vencimiento = QStringLiteral("mañana");
        QVERIFY(!m_products->add(badDate).ok());
    }

    void expiryRequired()
    {
        QVERIFY(m_settings->save({{QStringLiteral("require_expiry"), QStringLiteral("1")}})
                    [QStringLiteral("ok")]
                        .toBool());
        // Sin lote/vencimiento falla vía controller...
        QVERIFY(!m_ctl->add({{"sku", "FAR-X"},
                             {"name", "Sin lote"},
                             {"price", 1000.0},
                             {"stock", 5.0}})["ok"]
                     .toBool());
        // ...y con ambos pasa.
        QVERIFY(m_ctl->add({{"sku", "FAR-OK"},
                            {"name", "Con lote"},
                            {"price", 1000.0},
                            {"stock", 5.0},
                            {"lote", "L1"},
                            {"vencimiento", "2027-01-01"}})["ok"]
                    .toBool());
        // Flag apagado: pasa sin lote.
        QVERIFY(m_settings->save({{QStringLiteral("require_expiry"), QStringLiteral("0")}})
                    [QStringLiteral("ok")]
                        .toBool());
        QVERIFY(m_ctl->add({{"sku", "GEN-OK"},
                            {"name", "Sin lote ok"},
                            {"price", 500.0},
                            {"stock", 3.0}})["ok"]
                    .toBool());
    }

    void expiredBlocked()
    {
        const QString past =
            QDate::currentDate().addDays(-10).toString(Qt::ISODate);
        const QString future =
            QDate::currentDate().addDays(100).toString(Qt::ISODate);
        Product v;
        v.sku = QStringLiteral("VENC");
        v.name = QStringLiteral("Vencido");
        v.price = 2000.0;
        v.stock = 10.0;
        v.lote = QStringLiteral("LV");
        v.vencimiento = past;
        QVERIFY(m_products->add(v).ok());
        Product f;
        f.sku = QStringLiteral("VIG");
        f.name = QStringLiteral("Vigente");
        f.price = 2000.0;
        f.stock = 10.0;
        f.lote = QStringLiteral("LF");
        f.vencimiento = future;
        QVERIFY(m_products->add(f).ok());
        const int vid = m_products->findBySku(QStringLiteral("VENC"))->id;
        const int fid = m_products->findBySku(QStringLiteral("VIG"))->id;
        auto bad = m_svc->create({SI{vid, 1.0}}, QStringLiteral("Mostrador"), {},
                                 QStringLiteral("Efectivo"), QString(), QStringLiteral("tester"));
        QVERIFY(!bad.ok());
        QVERIFY(bad.error().contains(QStringLiteral("LV")));
        auto good = m_svc->create({SI{fid, 1.0}}, QStringLiteral("Mostrador"), {},
                                  QStringLiteral("Efectivo"), QString(), QStringLiteral("tester"));
        QVERIFY(good.ok());
    }

    void recetaAndControlled()
    {
        Product rx;
        rx.sku = QStringLiteral("RX1");
        rx.name = QStringLiteral("Con receta");
        rx.price = 15000.0;
        rx.stock = 5.0;
        rx.attrsJson = Attrs::set(QStringLiteral("{}"), Attrs::KRequiresPrescription, true);
        QVERIFY(m_products->add(rx).ok());
        Product ct;
        ct.sku = QStringLiteral("CT1");
        ct.name = QStringLiteral("Controlado");
        ct.price = 50000.0;
        ct.stock = 5.0;
        ct.attrsJson = Attrs::set(QStringLiteral("{}"), Attrs::KControlled, true);
        QVERIFY(m_products->add(ct).ok());
        const int rxid = m_products->findBySku(QStringLiteral("RX1"))->id;
        const int ctid = m_products->findBySku(QStringLiteral("CT1"))->id;
        // Sin receta falla; con receta pasa (rol cualquiera).
        QVERIFY(!m_svc->create({SI{rxid, 1.0}}, QStringLiteral("X"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("vendedor"))
                     .ok());
        SI withRx{rxid, 1.0};
        withRx.receta = QStringLiteral("RX-2026-001");
        auto ok = m_svc->create({withRx}, QStringLiteral("X"), {},
                                QStringLiteral("Efectivo"), QString(), QStringLiteral("vendedor"));
        QVERIFY(ok.ok());
        // Receta en línea + auditoría.
        QSqlQuery q(m_db);
        QVERIFY(q.exec(QStringLiteral("SELECT attrs_json FROM sale_items WHERE sale_id='")
                       + ok.value().id + QStringLiteral("'")));
        QVERIFY(q.next());
        QVERIFY(q.value(0).toString().contains(QStringLiteral("RX-2026-001")));
        QVERIFY(q.exec(QStringLiteral(
            "SELECT COUNT(*) FROM audit_log WHERE action='venta_receta'")));
        QVERIFY(q.next() && q.value(0).toInt() >= 1);
        // Controlado: vendedor falla, admin pasa.
        QVERIFY(!m_svc->create({SI{ctid, 1.0}}, QStringLiteral("X"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("vendedor"),
                               false, QStringLiteral("Vendedor"))
                     .ok());
        QVERIFY(m_svc->create({SI{ctid, 1.0}}, QStringLiteral("X"), {},
                              QStringLiteral("Efectivo"), QString(), QStringLiteral("admin"),
                              false, QStringLiteral("Administrador"))
                    .ok());
    }

    void serialFlow()
    {
        Product eq;
        eq.sku = QStringLiteral("EQ1");
        eq.name = QStringLiteral("Equipo X");
        eq.price = 500000.0;
        eq.stock = 2.0;
        eq.attrsJson = Attrs::set(Attrs::set(QStringLiteral("{}"), Attrs::KTrackSerial, true),
                                  Attrs::KWarrantyMonths, 12);
        QVERIFY(m_products->add(eq).ok());
        const int eqid = m_products->findBySku(QStringLiteral("EQ1"))->id;
        QVERIFY(m_serials->add(eqid, QStringLiteral("EQ1"), QStringLiteral("IMEI001")).ok());
        QVERIFY(m_serials->add(eqid, QStringLiteral("EQ1"), QStringLiteral("IMEI002")).ok());
        // Duplicado rechazado.
        QVERIFY(!m_serials->add(eqid, QStringLiteral("EQ1"), QStringLiteral("IMEI001")).ok());
        // Sin serial falla; serial inexistente falla.
        QVERIFY(!m_svc->create({SI{eqid, 1.0}}, QStringLiteral("X"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("t")).ok());
        SI bad{eqid, 1.0};
        bad.serial = QStringLiteral("NOPE");
        QVERIFY(!m_svc->create({bad}, QStringLiteral("X"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("t")).ok());
        // Venta con serial válido.
        SI good{eqid, 1.0};
        good.serial = QStringLiteral("IMEI001");
        auto r = m_svc->create({good}, QStringLiteral("Juan"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("t"));
        QVERIFY(r.ok());
        QCOMPARE(m_serials->find(QStringLiteral("IMEI001"))->status, QStringLiteral("sold"));
        QCOMPARE(m_products->findBySku(QStringLiteral("EQ1"))->stock, 1.0);
        // Vender el mismo serial de nuevo falla.
        SI again{eqid, 1.0};
        again.serial = QStringLiteral("IMEI001");
        QVERIFY(!m_svc->create({again}, QStringLiteral("X"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("t")).ok());
        // Garantía vigente (venta hoy + 12 meses).
        const auto w = m_serials->warrantyStatus(QStringLiteral("IMEI001"), 12);
        QVERIFY(w[QStringLiteral("inWarranty")].toBool());
        // Cancelar devuelve el serial.
        QVERIFY(m_svc->cancel(r.value().id, QStringLiteral("test"), QStringLiteral("t")).ok());
        QCOMPARE(m_serials->find(QStringLiteral("IMEI001"))->status,
                 QStringLiteral("in_stock"));
        // RMA.
        QVERIFY(m_serials->setStatus(QStringLiteral("IMEI002"), QStringLiteral("rma"),
                                     QStringLiteral("pantalla")).ok());
        QCOMPARE(m_serials->find(QStringLiteral("IMEI002"))->status, QStringLiteral("rma"));
        QVERIFY(!m_serials->setStatus(QStringLiteral("IMEI002"), QStringLiteral("volar")).ok());
    }

    void wasteAndSerialsReport()
    {
        // Fase 4: merma descuenta stock, tipo Merma y reporte valorizado.
        Product g;
        g.sku = QStringLiteral("MERMA1");
        g.name = QStringLiteral("Perecedero");
        g.price = 5000.0;
        g.priceBuy = 3000.0;
        g.stock = 10.0;
        g.unit = QStringLiteral("kg");
        QVERIFY(m_products->add(g).ok());
        InventoryService wasteSvc(m_db, m_products, m_inventory, nullptr, this);
        QVERIFY(!wasteSvc.registerWaste(QStringLiteral("MERMA1"), 0.0, QStringLiteral("x"),
                                        QStringLiteral("t"))
                     .ok());
        QVERIFY(!wasteSvc.registerWaste(QStringLiteral("MERMA1"), 99.0, QStringLiteral("x"),
                                        QStringLiteral("t"))
                     .ok());
        auto w = wasteSvc.registerWaste(QStringLiteral("MERMA1"), 2.5,
                                        QStringLiteral("vencido"), QStringLiteral("t"));
        QVERIFY(w.ok());
        QCOMPARE(w.value().newStock, 7.5);
        ReportService rep(m_db);
        const QVariantList wr = rep.wasteReport();
        bool found = false;
        for (const QVariant &v : wr) {
            const QVariantMap m = v.toMap();
            if (m["sku"].toString() == QStringLiteral("MERMA1")) {
                QCOMPARE(m["qty"].toDouble(), 2.5);
                QCOMPARE(m["cost"].toDouble(), 7500.0);
                found = true;
            }
        }
        QVERIFY(found);
        QVERIFY(!rep.exportCsv(QStringLiteral("mermas"), m_tmp.path()).isEmpty());

        // Fase 4: reporte de seriales + SerialsController (producto propio,
        // sin depender del orden de ejecución de los casos).
        Product se;
        se.sku = QStringLiteral("EQC");
        se.name = QStringLiteral("Equipo Ctl");
        se.price = 300000.0;
        se.stock = 3.0;
        se.attrsJson = Attrs::set(QStringLiteral("{}"), Attrs::KTrackSerial, true);
        QVERIFY(m_products->add(se).ok());
        const QVariantMap sr = rep.serialsReport();
        QVERIFY(sr.contains("counts") && sr.contains("items"));
        QVERIFY(!rep.exportCsv(QStringLiteral("seriales"), m_tmp.path()).isEmpty());
        SerialsController ctl(m_serials, m_products, this);
        QVERIFY(ctl.addSerial(QStringLiteral("EQC"), QStringLiteral("CTL-001"))["ok"].toBool());
        QVERIFY(!ctl.addSerial(QStringLiteral("NOPE"), QStringLiteral("CTL-002"))["ok"].toBool());
        ctl.search(QStringLiteral("CTL-001"), QStringLiteral("in_stock"));
        QVERIFY(!ctl.serials().isEmpty());
        QVERIFY(ctl.warrantyFor(QStringLiteral("CTL-001"))["ok"].toBool());
        QVERIFY(ctl.inStockCount(QStringLiteral("EQC")) >= 1);
        QVERIFY(!ctl.inStock(QStringLiteral("EQC")).isEmpty());
        QVERIFY(ctl.setStatus(QStringLiteral("CTL-001"), QStringLiteral("rma"),
                              QStringLiteral("test"), QStringLiteral("t"))["ok"].toBool());
        QVERIFY(!ctl.setStatus(QStringLiteral("CTL-001"), QStringLiteral("volar"),
                               QStringLiteral("x"), QStringLiteral("t"))["ok"].toBool());
    }

    void serialQtyOne()
    {
        // Fase 4 (criterio): no se venden 2 equipos con 1 serial disponible.
        Product eq;
        eq.sku = QStringLiteral("EQ-QTY");
        eq.name = QStringLiteral("Equipo Qty");
        eq.price = 400000.0;
        eq.stock = 5.0;
        eq.attrsJson = Attrs::set(QStringLiteral("{}"), Attrs::KTrackSerial, true);
        QVERIFY(m_products->add(eq).ok());
        const int eqid = m_products->findBySku(QStringLiteral("EQ-QTY"))->id;
        QVERIFY(m_serials->add(eqid, QStringLiteral("EQ-QTY"), QStringLiteral("QTY-001")).ok());
        // qty 2 con 1 serial → rechazado.
        SI two{eqid, 2.0};
        two.serial = QStringLiteral("QTY-001");
        QVERIFY(!m_svc->create({two}, QStringLiteral("X"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("t"))
                     .ok());
        // 2 líneas con el mismo serial → rechazado (duplicado).
        SI a{eqid, 1.0};
        a.serial = QStringLiteral("QTY-001");
        SI b{eqid, 1.0};
        b.serial = QStringLiteral("QTY-001");
        QVERIFY(!m_svc->create({a, b}, QStringLiteral("X"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("t"))
                     .ok());
        // 1 línea qty 1 con su serial → pasa.
        QVERIFY(m_svc->create({a}, QStringLiteral("X"), {},
                              QStringLiteral("Efectivo"), QString(), QStringLiteral("t"))
                    .ok());
    }

    void wholesalePriceList()
    {
        // Fase 4 (abarrotes): cliente mayorista paga price_wholesale.
        Client mayorista;
        mayorista.name = QStringLiteral("MAYORISTA-TEST");
        mayorista.priceList = QStringLiteral("mayorista");
        QVERIFY(m_clients->add(mayorista).ok());
        Product p;
        p.sku = QStringLiteral("AB-GRANO");
        p.name = QStringLiteral("Grano");
        p.price = 1000.0;
        p.priceWholesale = 800.0;
        p.stock = 100.0;
        p.tax = QStringLiteral("Excluido");
        QVERIFY(m_products->add(p).ok());
        const int pid = m_products->findBySku(QStringLiteral("AB-GRANO"))->id;
        auto w = m_svc->create({SI{pid, 2.0}}, QStringLiteral("MAYORISTA-TEST"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("t"));
        QVERIFY(w.ok());
        QCOMPARE(w.value().total, 1600.0);
        auto d = m_svc->create({SI{pid, 2.0}}, QStringLiteral("Mostrador"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("t"));
        QVERIFY(d.ok());
        QCOMPARE(d.value().total, 2000.0);
    }

    void ean13()
    {        // Ejemplo GS1 válido + variante con dígito malo.
        QVERIFY(ProductRepository::isValidEan13(QStringLiteral("5901234123457")));
        QVERIFY(!ProductRepository::isValidEan13(QStringLiteral("5901234123458")));
        QVERIFY(!ProductRepository::isValidEan13(QStringLiteral("123")));
        QVERIFY(!ProductRepository::isValidEan13(QStringLiteral("770123456001X")));
        // El generador produce EAN válido.
        for (int i = 0; i < 20; ++i) {
            const QString b = ProductRepository::generateBarcode(
                QStringLiteral("EAN%1").arg(i));
            QVERIFY2(ProductRepository::isValidEan13(b),
                     qPrintable(QStringLiteral("barcode inválido: ") + b));
        }
        // Alta con EAN inválido se rechaza; UPC-12 se acepta.
        Product p;
        p.sku = QStringLiteral("EAN-BAD");
        p.name = QStringLiteral("Ean malo");
        p.price = 1000.0;
        p.stock = 1.0;
        p.barcode = QStringLiteral("7701234560010");
        if (ProductRepository::isValidEan13(p.barcode))
            p.barcode = QStringLiteral("7701234560019");
        QVERIFY(!ProductRepository::isValidEan13(p.barcode));
        QVERIFY(!m_products->add(p).ok());
        Product u;
        u.sku = QStringLiteral("UPC-OK");
        u.name = QStringLiteral("Upc");
        u.price = 1000.0;
        u.stock = 1.0;
        u.barcode = QStringLiteral("123456789012");
        QVERIFY(m_products->add(u).ok());
    }

private:
    QTemporaryDir m_tmp;
    DatabaseManager *m_dbm = nullptr;
    QSqlDatabase m_db;
    ProductRepository *m_products = nullptr;
    InventoryRepository *m_inventory = nullptr;
    ClientRepository *m_clients = nullptr;
    SettingsService *m_settings = nullptr;
    SerialRepository *m_serials = nullptr;
    SalesService *m_svc = nullptr;
    CatalogController *m_ctl = nullptr;
};

QTEST_MAIN(TstVertical)
#include "tst_vertical.moc"
