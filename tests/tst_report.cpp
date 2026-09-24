// ReportService contra fixture demo: verifica consultas agregadas.
#include <QtTest>

#include "core/DatabaseManager.h"
#include "TestDb.h"
#include "services/ReportService.h"

#include <QDate>
#include <QTemporaryDir>

class TstReport : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY(m_tmp.isValid());
        m_dbm = new DatabaseManager(this);
        QVERIFY(m_dbm->initialize(m_tmp.filePath(QStringLiteral("report.db"))));
        // Datos demo solo-tests (la app siembra base limpia)
        QVERIFY(TestDb::loadDemo(m_dbm->database()));
        m_rep = new ReportService(m_dbm->database(), this);
    }

    void statsShape()
    {
        const QVariantMap s = m_rep->stats();
        QCOMPARE(s["totalProducts"].toInt(), 10);
        QCOMPARE(s["totalClients"].toInt(), 5);
        QVERIFY(s["totalSales"].toDouble() > 0);
        QVERIFY(s.contains("pendingOrders") && s.contains("lowStockAlerts"));
    }

    void rankings()
    {
        const QVariantList top = m_rep->topProducts(5);
        QCOMPARE(top.size(), 5);
        // El más vendido primero (orden desc por unidades)
        int prev = INT_MAX;
        for (const QVariant &v : top) {
            const int sold = v.toMap()["sold"].toInt();
            QVERIFY(sold <= prev);
            prev = sold;
        }
        const QVariantList least = m_rep->leastSold(3);
        QCOMPARE(least.size(), 3);
        QVERIFY(!m_rep->topClients(3).isEmpty());
        QVERIFY(!m_rep->topSellers(2).isEmpty());
    }

    void financials()
    {
        const QVariantMap e = m_rep->incomeStatement();
        QVERIFY(e["ingresos"].toDouble() > 0);
        QVERIFY(e["costo"].toDouble() > 0);
        QCOMPARE(e["bruto"].toDouble(), e["ingresos"].toDouble() - e["costo"].toDouble());
        const QVariantMap f = m_rep->cashFlow();
        QCOMPARE(f["neto"].toDouble(), f["entradas"].toDouble() - f["salidas"].toDouble());
        QVERIFY(m_rep->taxes()["total"].toDouble() >= 0);
        QVERIFY(m_rep->averageTicket() > 0);
        const QVariantMap k = m_rep->kpis();
        QVERIFY(k.contains("rotacion") && k.contains("conversion"));
    }

    void periods()
    {
        const QVariantMap d = m_rep->salesForPeriod(QStringLiteral("dia"));
        QVERIFY(d.contains("total") && d.contains("count"));
        // El seed es de 2026-08/09 (V005 está Cancelada → 7 no-canceladas)
        const QVariantMap y = m_rep->salesForPeriod(QStringLiteral("año"));
        QVERIFY(y["count"].toInt() >= 7);
        const QVariantList byDay = m_rep->salesByDay(7);
        QCOMPARE(byDay.size(), 7);
        const QVariantMap sum = m_rep->salesSummary();
        // Seed: 4 Pagada + 1 Pendiente + 1 Cancelada + 1 Cotización + 1 Pedido
        int totalDocs = 0;
        for (const QVariant &v : sum.values())
            totalDocs += v.toInt();
        QCOMPARE(totalDocs, 8);
        QCOMPARE(sum["Pagada"].toInt(), 4);
        QVERIFY(!m_rep->marginPerProduct().isEmpty());
    }

    void csvExport()
    {
        const QString op = m_rep->exportCsv(QStringLiteral("operativo"), m_tmp.path());
        QVERIFY(!op.isEmpty() && QFile::exists(op));
        const QString fin = m_rep->exportCsv(QStringLiteral("financiero"), m_tmp.path());
        QVERIFY(!fin.isEmpty() && QFile::exists(fin));
        QVERIFY(m_rep->exportCsv(QStringLiteral("x"), QStringLiteral("/no/existe/dir")).isEmpty());
    }

    void taxBreakdown()
    {
        // Fase 1: venta con tax_breakdown JSON se desglosa por tasa;
        // el histórico sin breakdown sigue en iva_19.
        QSqlQuery q(m_dbm->database());
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO sales (id, date, client, total, subtotal, tax, status, tax_breakdown) "
            "VALUES ('VTAX1','2026-09-20','Mostrador',21900,20000,1900,'Pagada',"
            "'[{\"name\":\"Excluido\",\"rate\":0,\"base\":10000,\"tax\":0},"
            "{\"name\":\"IVA 19%\",\"rate\":19,\"base\":10000,\"tax\":1900}]')")));
        const QVariantMap tx = m_rep->taxes();
        QVERIFY(tx.contains("breakdown"));
        const QVariantList bd = tx["breakdown"].toList();
        QVERIFY(bd.size() >= 2);
        bool seen0 = false, seen19 = false;
        for (const QVariant &v : bd) {
            const QVariantMap m = v.toMap();
            if (m["rate"].toDouble() == 0.0)
                seen0 = true;
            if (m["rate"].toDouble() == 19.0 && m["tax"].toDouble() >= 1900.0)
                seen19 = true;
        }
        QVERIFY(seen0 && seen19);
        QVERIFY(tx["total"].toDouble() >= 1900.0);
    }

    void csvCarriesBusinessHeader()
    {
        // Fase 0: la cabecera del CSV usa business_name/NIT de `settings`.
        QSqlQuery q(m_dbm->database());
        QVERIFY(q.exec(QStringLiteral(
            "INSERT OR REPLACE INTO settings (key, value) VALUES "
            "('business_name','Farmacia La Salud'),('business_nit','900123456-7')")));
        const QString path = m_rep->exportCsv(QStringLiteral("financiero"), m_tmp.path());
        QVERIFY(!path.isEmpty());
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(f.readAll());
        QVERIFY(content.contains(QStringLiteral("Farmacia La Salud")));
        QVERIFY(content.contains(QStringLiteral("900123456-7")));
    }

    void pdfExport()
    {
        const QString op = m_rep->exportPdf(QStringLiteral("operativo"), m_tmp.path());
        QVERIFY(!op.isEmpty() && QFile::exists(op));
        QVERIFY(QFile(op).size() > 1000); // PDF real, no vacío
        const QString fin = m_rep->exportPdf(QStringLiteral("financiero"), m_tmp.path());
        QVERIFY(!fin.isEmpty() && QFile::exists(fin));
        QFile f(op);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.read(5), QByteArray("%PDF-"));
    }

    void verticalReports()
    {
        // Fase 5: genéricos (rotación por categoría + valorizado).
        const QVariantList rot = m_rep->rotationByCategory();
        QVERIFY(!rot.isEmpty());
        for (const QVariant &v : rot)
            QVERIFY(v.toMap().contains("category") && v.toMap().contains("rotation"));
        const QVariantMap iv = m_rep->inventoryValue();
        QVERIFY(iv["cost"].toDouble() > 0 && iv["units"].toDouble() > 0);
        // El seed ya vende en 'unidad' (granel rinde por unidad).
        const QVariantList bulk = m_rep->bulkPerformance();
        QVERIFY(!bulk.isEmpty());
        QVERIFY(bulk.first().toMap().contains("unit"));

        // Fixture propio: controlado vendido + serial en garantía.
        const QString today = QDate::currentDate().toString(Qt::ISODate);
        QSqlQuery q(m_dbm->database());
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO products (sku, name, price, price_buy, stock, attrs_json) VALUES "
            "('CT-W','Controlado W',10000,6000,10,'{\"controlled\":true}')")));
        const int ctId = q.lastInsertId().toInt();
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO products (sku, name, price, price_buy, stock, attrs_json) VALUES "
            "('EQ-W','Equipo W',500000,400000,3,'{\"track_serial\":true,\"warranty_months\":12}')")));
        const int eqId = q.lastInsertId().toInt();
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO sales (id, date, client, total, subtotal, tax, status) VALUES "
            "('S-CT','%1','Mostrador',11900,10000,1900,'Pagada')")
                           .arg(today)));
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO sale_items (sale_id, product_id, qty, subtotal) VALUES "
            "('S-CT',%1,2,20000)")
                           .arg(ctId)));
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO sales (id, date, client, total, subtotal, tax, status) VALUES "
            "('S-W','%1','Mostrador',595000,500000,95000,'Pagada')")
                           .arg(today)));
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO sale_items (sale_id, product_id, qty, subtotal) VALUES "
            "('S-W',%1,1,500000)")
                           .arg(eqId)));
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO serials (product_id, sku, serial, status, sale_id) VALUES "
            "(%1,'EQ-W','W-001','sold','S-W')")
                           .arg(eqId)));

        bool seenCt = false;
        for (const QVariant &v : m_rep->controlledSales()) {
            const QVariantMap m = v.toMap();
            if (m["sku"].toString() == QStringLiteral("CT-W")
                && m["saleId"].toString() == QStringLiteral("S-CT")) {
                QCOMPARE(m["qty"].toDouble(), 2.0);
                seenCt = true;
            }
        }
        QVERIFY(seenCt);
        bool seenW = false;
        for (const QVariant &v : m_rep->warrantyOpen()) {
            const QVariantMap m = v.toMap();
            if (m["serial"].toString() == QStringLiteral("W-001")) {
                QVERIFY(!m["expiresAt"].toString().isEmpty());
                seenW = true;
            }
        }
        QVERIFY(seenW);

        // CSV + PDF de los 5 tipos nuevos.
        for (const QString &t : {QStringLiteral("rotacion"), QStringLiteral("inventario"),
                                 QStringLiteral("controlados"), QStringLiteral("garantias"),
                                 QStringLiteral("granel")}) {
            const QString p = m_rep->exportCsv(t, m_tmp.path());
            QVERIFY2(!p.isEmpty() && QFile::exists(p), qPrintable(t));
        }
        const QString pdf = m_rep->exportPdf(QStringLiteral("garantias"), m_tmp.path());
        QVERIFY(!pdf.isEmpty() && QFile::exists(pdf));
    }

private:
    QTemporaryDir m_tmp;
    DatabaseManager *m_dbm = nullptr;
    ReportService *m_rep = nullptr;
};

QTEST_MAIN(TstReport)
#include "tst_report.moc"
