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

    void businessHeader()
    {
        // Fase 0 multinegocio: la cabecera sale de SettingsService.
        TicketPrinter::Ticket t;
        t.saleId = QStringLiteral("V888");
        t.clientName = QStringLiteral("Ana");
        t.businessName = QStringLiteral("Farmacia La Salud");
        t.businessNit = QStringLiteral("900123456-7");
        t.businessAddress = QStringLiteral("Calle 10 #5-20");
        t.businessPhone = QStringLiteral("6015550101");
        t.lines = {{"Aspirina", 2, 5000, 10000}};
        t.total = 10000;
        const QString text = TicketPrinter::buildText(t);
        QVERIFY(text.contains(QStringLiteral("Farmacia La Salud")));
        QVERIFY(text.contains(QStringLiteral("900123456-7")));
        QVERIFY(text.contains(QStringLiteral("Calle 10")));
        QVERIFY(text.contains(QStringLiteral("6015550101")));
        // Sin negocio configurado se conserva el aspecto anterior
        TicketPrinter::Ticket plain;
        plain.saleId = QStringLiteral("V889");
        plain.lines = {{"X", 1, 1000, 1000}};
        plain.total = 1000;
        QVERIFY(TicketPrinter::buildText(plain).contains(QStringLiteral("SISTEMA DE VENTAS")));
    }
};

QTEST_MAIN(TstTicket)
#include "tst_ticket.moc"
