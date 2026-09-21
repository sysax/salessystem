// ReportService contra seed: verifica consultas agregadas.
#include <QtTest>

#include "core/DatabaseManager.h"
#include "services/ReportService.h"

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

private:
    QTemporaryDir m_tmp;
    DatabaseManager *m_dbm = nullptr;
    ReportService *m_rep = nullptr;
};

QTEST_MAIN(TstReport)
#include "tst_report.moc"
