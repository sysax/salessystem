// InventoryService contra BD temporal: compras con costo promedio,
// ajustes justificados, transferencias, alertas y valorización.
#include <QtTest>

#include "core/DatabaseManager.h"
#include "core/EventBus.h"
#include "repositories/AuditRepository.h"
#include "repositories/InventoryRepository.h"
#include "repositories/ProductRepository.h"
#include "services/InventoryService.h"

#include <QTemporaryDir>

class TstInventoryService : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_dbm = new DatabaseManager(this);
        QVERIFY(m_dbm->initialize(m_tmp.filePath(QStringLiteral("inv_svc.db"))));
        QSqlDatabase db = m_dbm->database();
        m_audit = new AuditRepository(db, this);
        m_products = new ProductRepository(db, m_audit, this);
        m_inventory = new InventoryRepository(db, m_audit, this);
        m_svc = new InventoryService(db, m_products, m_inventory, nullptr, this);
    }

    void purchaseWeightedCost()
    {
        // P002: stock 45, costo 28000. +10 @30000 →
        // stock 55, costo (45*28000 + 10*30000)/55 = 28363.636...
        auto r = m_svc->registerPurchase(2, 10, 30000, QStringLiteral("TecnoMayorista SAS"),
                                         QStringLiteral("F-1"), QStringLiteral("tester"));
        QVERIFY(r.ok());
        QCOMPARE(r.value().newStock, 55);
        QVERIFY(qFuzzyCompare(r.value().newCost, 1560000.0 / 55.0));
        QCOMPARE(m_products->findById(2)->stock, 55);

        QVERIFY(!m_svc->registerPurchase(2, 0, 1, QStringLiteral("X"), QString(),
                                         QStringLiteral("t")).ok());
        QVERIFY(!m_svc->registerPurchase(2, -3, 1, QStringLiteral("X"), QString(),
                                         QStringLiteral("t")).ok());
        QVERIFY(!m_svc->registerPurchase(999, 1, 1, QStringLiteral("X"), QString(),
                                         QStringLiteral("t")).ok());
    }

    void adjustmentRules()
    {
        auto ok = m_svc->registerAdjustment(QStringLiteral("P003"), 5,
                                            QStringLiteral("conteo físico"),
                                            QStringLiteral("tester"));
        QVERIFY(ok.ok());
        QCOMPARE(ok.value().newStock, 25);

        QVERIFY(!m_svc->registerAdjustment(QStringLiteral("P003"), -100,
                                           QStringLiteral("baja"), QStringLiteral("tester"))
                     .ok()); // negativo
        QVERIFY(!m_svc->registerAdjustment(QStringLiteral("P003"), 0, QStringLiteral("x"),
                                           QStringLiteral("tester")).ok()); // cero
        QVERIFY(!m_svc->registerAdjustment(QStringLiteral("P003"), 1, QStringLiteral("  "),
                                           QStringLiteral("tester")).ok()); // sin motivo
        QVERIFY(!m_svc->registerAdjustment(QStringLiteral("NOPE"), 1, QStringLiteral("x"),
                                           QStringLiteral("tester")).ok());
        QCOMPARE(m_products->findBySku(QStringLiteral("P003"))->stock, 25);

        const auto movs = m_svc->movementsBySku(QStringLiteral("P003"));
        QVERIFY(!movs.isEmpty());
        QCOMPARE(movs.last().type, QStringLiteral("Entrada"));
    }

    void transferRules()
    {
        QVERIFY(m_svc->transfer(QStringLiteral("P006"), 5, QStringLiteral("B2-C1"),
                                QStringLiteral("reubicación"),
                                QStringLiteral("tester")).ok());
        QCOMPARE(m_products->findBySku(QStringLiteral("P006"))->location,
                 QStringLiteral("B2-C1"));

        QVERIFY(!m_svc->transfer(QStringLiteral("P006"), 9999, QStringLiteral("B9"),
                                 QStringLiteral("x"), QStringLiteral("tester")).ok());
        QVERIFY(!m_svc->transfer(QStringLiteral("P006"), 1, QStringLiteral("  "),
                                 QStringLiteral("x"), QStringLiteral("tester")).ok());
        QVERIFY(!m_svc->transfer(QStringLiteral("P006"), 1, QStringLiteral("B9"),
                                 QStringLiteral(""), QStringLiteral("tester")).ok());
        QVERIFY(!m_svc->transfer(QStringLiteral("NOPE"), 1, QStringLiteral("B9"),
                                 QStringLiteral("x"), QStringLiteral("tester")).ok());
    }

    void alertsAndValuation()
    {
        Product low;
        low.sku = QStringLiteral("LOW1");
        low.name = QStringLiteral("Casi agotado");
        low.price = 1000;
        low.stock = 2;
        low.stockMin = 10;
        QVERIFY(m_products->add(low).ok());

        const auto lows = m_svc->lowStock(1.0);
        bool found = false;
        for (const Product &p : lows) {
            if (p.sku == QLatin1String("LOW1"))
                found = true;
        }
        QVERIFY(found);

        const auto v = m_svc->valuation();
        QVERIFY(v.totalValue > 0);
        QVERIFY(v.productsCount > 0);
        const auto val = m_inventory->value();
        QCOMPARE(val.costValue, v.totalValue);
        QVERIFY(!m_inventory->belowMin().isEmpty());
    }

private:
    QTemporaryDir m_tmp;
    DatabaseManager *m_dbm = nullptr;
    AuditRepository *m_audit = nullptr;
    ProductRepository *m_products = nullptr;
    InventoryRepository *m_inventory = nullptr;
    InventoryService *m_svc = nullptr;
};

QTEST_MAIN(TstInventoryService)
#include "tst_inventory_service.moc"
