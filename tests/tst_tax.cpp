// Fase 1: impuestos flexibles — tasas configuradas, resolución por línea
// y desglose por tasa. Suite aislada (no depende de tst_sales_service).
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
#include "repositories/SettingsRepository.h"
#include "services/SalesService.h"
#include "services/SettingsService.h"

#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>

using SI = SalesService::ServiceItem;

class TstTax : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_dbm = new DatabaseManager(this);
        QVERIFY(m_dbm->initialize(m_tmp.filePath(QStringLiteral("tax.db"))));
        QSqlDatabase db = m_dbm->database();
        auto *bus = new EventBus(this);
        auto *audit = new AuditRepository(db, this);
        m_products = new ProductRepository(db, audit, this);
        auto *clients = new ClientRepository(db, audit, this);
        auto *caja = new CajaRepository(db, audit, this);
        m_saleRepo = new SaleRepository(db, clients, caja, audit, this);
        auto *inventory = new InventoryRepository(db, audit, this);
        auto *promos = new PromoRepository(db, m_products, audit, this);
        auto *settingsRepo = new SettingsRepository(db, this);
        m_settings = new SettingsService(settingsRepo, bus, this);
        m_svc = new SalesService(db, m_products, m_saleRepo, inventory, clients, caja,
                                 promos, bus, m_settings, nullptr, nullptr, this);
    }

    void taxRatesParsing()
    {
        // Defaults: IVA 19% + Excluido
        const auto dflt = m_settings->taxRates();
        QCOMPARE(dflt.size(), 2);
        QCOMPARE(m_settings->defaultTaxRateValue(), 19.0);
        QCOMPARE(m_settings->taxNameForRate(19.0), QStringLiteral("IVA 19%"));
        QCOMPARE(m_settings->taxNameForRate(0.0), QStringLiteral("Excluido"));

        // Custom: tres tasas
        QVERIFY(m_settings
                    ->save({{QStringLiteral("tax_rates_json"),
                             QStringLiteral("[{\"name\":\"IVA 19%\",\"rate\":19},"
                                            "{\"name\":\"IVA 5%\",\"rate\":5},"
                                            "{\"name\":\"Excluido\",\"rate\":0}]")},
                            {QStringLiteral("default_tax_rate"), QStringLiteral("5")}})
                    [QStringLiteral("ok")]
                        .toBool());
        QCOMPARE(m_settings->taxRates().size(), 3);
        QCOMPARE(m_settings->defaultTaxRateValue(), 5.0);
        QCOMPARE(m_settings->taxNameForRate(5.0), QStringLiteral("IVA 5%"));

        // JSON roto → fallback seguro (luego se restaura para los demás tests)
        QSqlQuery q(m_dbm->database());
        QVERIFY(q.exec(QStringLiteral("UPDATE settings SET value='roto' WHERE key='tax_rates_json'")));
        QCOMPARE(m_settings->taxRates().size(), 2);
        QVERIFY(m_settings
                    ->save({{QStringLiteral("tax_rates_json"),
                             QStringLiteral("[{\"name\":\"IVA 19%\",\"rate\":19},"
                                            "{\"name\":\"Excluido\",\"rate\":0}]")},
                            {QStringLiteral("default_tax_rate"), QStringLiteral("19")}})
                    [QStringLiteral("ok")]
                        .toBool());
    }

    void mixedTicketBreakdown()
    {
        // Exento $10.000 ×1 + IVA 19% $10.000 ×1 → total 21.900
        const auto t = m_svc->calculateTotals({SI{mkProduct("T0", "Excluido"), 1},
                                               SI{mkProduct("T19", "IVA 19%"), 1}});
        QCOMPARE(t.itemsCount, 2);
        QCOMPARE(t.subtotal, 20000.0);
        QCOMPARE(t.tax, 1900.0);
        QCOMPARE(t.total, 21900.0);
        QCOMPARE(t.buckets.size(), 2);
        double baseSum = 0.0, taxSum = 0.0;
        for (const auto &b : t.buckets) {
            baseSum += b.base;
            taxSum += b.tax;
        }
        // Invariante: Σ buckets == subtotal−descuento / impuesto
        QCOMPARE(baseSum, t.subtotal - t.discount);
        QCOMPARE(taxSum, t.tax);
        // Bucket 0% y 19% correctos
        bool seen0 = false, seen19 = false;
        for (const auto &b : t.buckets) {
            if (qFuzzyCompare(b.rate + 1.0, 1.0)) {
                QCOMPARE(b.base, 10000.0);
                QCOMPARE(b.tax, 0.0);
                seen0 = true;
            }
            if (qFuzzyCompare(b.rate + 1.0, 20.0)) {
                QCOMPARE(b.base, 10000.0);
                QCOMPARE(b.tax, 1900.0);
                seen19 = true;
            }
        }
        QVERIFY(seen0 && seen19);
    }

    void legacyRateFallsBackToDefault()
    {
        // "IVA 8%" no está en {19, 0} → tasa por defecto (19)
        QCOMPARE(m_svc->resolveTaxRate(QStringLiteral("IVA 8%")), 19.0);
        QCOMPARE(m_svc->resolveTaxName(QStringLiteral("IVA 8%")), QStringLiteral("IVA 19%"));
        // Tasas configuradas se respetan
        QCOMPARE(m_svc->resolveTaxRate(QStringLiteral("Excluido")), 0.0);
        QCOMPARE(m_svc->resolveTaxRate(QStringLiteral("IVA 19%")), 19.0);
        // Cambio de default en settings afecta la próxima resolución (sin recompilar)
        QVERIFY(m_settings->save({{QStringLiteral("default_tax_rate"), QStringLiteral("0")}})
                    [QStringLiteral("ok")]
                        .toBool());
        QCOMPARE(m_svc->resolveTaxRate(QStringLiteral("IVA 8%")), 0.0);
        QVERIFY(m_settings->save({{QStringLiteral("default_tax_rate"), QStringLiteral("19")}})
                    [QStringLiteral("ok")]
                        .toBool());
    }

    void createPersistsBreakdown()
    {
        auto r = m_svc->create({SI{mkProduct("T0", "Excluido"), 1},
                                SI{mkProduct("T19", "IVA 19%"), 1}},
                               QStringLiteral("Mostrador"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("tester"));
        QVERIFY(r.ok());
        QCOMPARE(r.value().buckets.size(), 2);
        QCOMPARE(r.value().total, 21900.0);
        const auto s = m_saleRepo->find(r.value().id);
        QVERIFY(s.has_value());
        const auto buckets = SalesService::bucketsFromJson(s->taxBreakdown);
        QCOMPARE(buckets.size(), 2);
        // Round-trip JSON
        QVERIFY(!SalesService::bucketsToJson(buckets).isEmpty());
    }

    void decimalGranelTicket()
    {
        // Fase 2: 0.350 kg de tomate a $10.000/kg + 2 cajas de aspirina a $5.000.
        const int tomato = mkProductFull(QStringLiteral("TOM"), QStringLiteral("Tomate"),
                                         10000.0, 5.0, QStringLiteral("Excluido"),
                                         QStringLiteral("kg"));
        const int asp = mkProductFull(QStringLiteral("ASP"), QStringLiteral("Aspirina"),
                                      5000.0, 10.0, QStringLiteral("IVA 19%"),
                                      QStringLiteral("caja"));
        const auto t = m_svc->calculateTotals({SI{tomato, 0.35}, SI{asp, 2.0}});
        QCOMPARE(t.total, 3500.0 - 0.0 + 0.0 + 10000.0 + 1900.0); // 15400
        QCOMPARE(t.buckets.size(), 2);
        auto r = m_svc->create({SI{tomato, 0.35}, SI{asp, 2.0}},
                               QStringLiteral("Mostrador"), {},
                               QStringLiteral("Efectivo"), QString(), QStringLiteral("tester"));
        QVERIFY(r.ok());
        QVERIFY(qFuzzyCompare(r.value().total, 15400.0));
        QCOMPARE(m_products->findById(tomato)->stock, 5.0 - 0.35);
        QCOMPARE(m_products->findById(asp)->stock, 8.0);
        // Stock insuficiente decimal
        QVERIFY(!m_svc->create({SI{tomato, 99.0}}, QStringLiteral("X"), {},
                               QStringLiteral("Efectivo"), QString(),
                               QStringLiteral("tester")).ok());
    }

    void legacyMigrationReaddsColumn()
    {
        // BD anterior sin tax_breakdown: reabrir migra (solo aditiva).
        QSqlQuery drop(m_dbm->database());
        QVERIFY(drop.exec(QStringLiteral("ALTER TABLE sales DROP COLUMN tax_breakdown")));
        m_dbm->close();
        QVERIFY(m_dbm->initialize(m_tmp.filePath(QStringLiteral("tax.db"))));
        QSqlQuery p(m_dbm->database());
        QVERIFY(p.exec(QStringLiteral("PRAGMA table_info(sales)")));
        bool found = false;
        while (p.next()) {
            if (p.value(1).toString() == QStringLiteral("tax_breakdown"))
                found = true;
        }
        QVERIFY(found);
    }

private:
    int mkProductFull(const QString &sku, const QString &name, double price, double stock,
                      const QString &tax, const QString &unit)
    {
        if (const auto ex = m_products->findBySku(sku))
            return ex->id;
        Product p;
        p.sku = sku;
        p.name = name;
        p.price = price;
        p.stock = stock;
        p.tax = tax;
        p.unit = unit;
        const auto r = m_products->add(p);
        if (!r.ok()) {
            QTest::qFail(qPrintable(QStringLiteral("mkProductFull add: ") + r.error()),
                         __FILE__, __LINE__);
            return -1;
        }
        return r.value().id;
    }

    int mkProduct(const QString &suffix, const QString &tax)
    {
        const QString sku = QStringLiteral("TAX") + suffix;
        if (const auto ex = m_products->findBySku(sku))
            return ex->id;
        Product p;
        p.sku = sku;
        p.name = QStringLiteral("Prod ") + suffix;
        p.price = 10000.0;
        p.stock = 50;
        p.tax = tax;
        const auto r = m_products->add(p);
        if (!r.ok()) {
            QTest::qFail(qPrintable(QStringLiteral("mkProduct add: ") + r.error()),
                         __FILE__, __LINE__);
            return -1;
        }
        return r.value().id;
    }

    QTemporaryDir m_tmp;
    DatabaseManager *m_dbm = nullptr;
    SettingsService *m_settings = nullptr;
    SalesService *m_svc = nullptr;
    ProductRepository *m_products = nullptr;
    SaleRepository *m_saleRepo = nullptr;
};

QTEST_MAIN(TstTax)
#include "tst_tax.moc"
