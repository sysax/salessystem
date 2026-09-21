// TicketPrinter: texto DIAN + archivo + cajón.
#include <QtTest>

#include "services/TicketPrinter.h"

#include <QTemporaryDir>

class TstTicket : public QObject
{
    Q_OBJECT

private slots:
    void buildAndPrint()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        TicketPrinter printer(tmp.path());

        TicketPrinter::Ticket t;
        t.saleId = QStringLiteral("V777");
        t.clientName = QStringLiteral("Juan Pérez");
        t.docType = QStringLiteral("Ticket de venta");
        t.cufe = QStringLiteral("CUFE-TEST");
        t.lines = {{"Mouse", 2, 45000, 90000}};
        t.discount = 5000;
        t.promoCode = QStringLiteral("PROMO");
        t.tax = 16150;
        t.total = 100150;
        t.payments = {{"efectivo", 100150.0}};
        t.change = 0;

        const QString text = TicketPrinter::buildText(t);
        QVERIFY(text.contains(QStringLiteral("V777")));
        QVERIFY(text.contains(QStringLiteral("CUFE-TEST")));
        QVERIFY(text.contains(QStringLiteral("Juan Pérez")));
        QVERIFY(text.contains(QStringLiteral("TOTAL:")));

        const auto r = printer.print(t);
        QVERIFY(r.ok());
        QVERIFY(QFile::exists(r.value().path));
        QVERIFY(r.value().path.endsWith(QStringLiteral("ticket_V777.txt")));
        QCOMPARE(QFile(r.value().path).size() > 0, true);
    }

    void drawerSignal()
    {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        TicketPrinter printer(tmp.path());
        QVERIFY(printer.openDrawer());
        QVERIFY(QFile::exists(tmp.path() + QStringLiteral("/cajon_signal")));
    }
};

QTEST_MAIN(TstTicket)
#include "tst_ticket.moc"
