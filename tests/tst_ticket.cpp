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
        t.lines = {TicketPrinter::Ticket::Line{QStringLiteral("Mouse"), 2,
                                               Money::fromCop(45000), Money::fromCop(90000)}};
        t.discount = Money::fromCop(5000);
        t.promoCode = QStringLiteral("PROMO");
        t.tax = Money::fromCop(16150);
        t.total = Money::fromCop(100150);
        t.payments = QMap<QString, Money>{{QStringLiteral("efectivo"), Money::fromCop(100150.0)}};
        t.change = Money();

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
        t.lines = {TicketPrinter::Ticket::Line{QStringLiteral("Aspirina"), 2,
                                               Money::fromCop(5000), Money::fromCop(10000)}};
        t.total = Money::fromCop(10000);
        const QString text = TicketPrinter::buildText(t);
        QVERIFY(text.contains(QStringLiteral("Farmacia La Salud")));
        QVERIFY(text.contains(QStringLiteral("900123456-7")));
        QVERIFY(text.contains(QStringLiteral("Calle 10")));
        QVERIFY(text.contains(QStringLiteral("6015550101")));
        // Sin negocio configurado se conserva el aspecto anterior
        TicketPrinter::Ticket plain;
        plain.saleId = QStringLiteral("V889");
        plain.lines = {TicketPrinter::Ticket::Line{QStringLiteral("X"), 1, Money::fromCop(1000),
                                                   Money::fromCop(1000)}};
        plain.total = Money::fromCop(1000);
        QVERIFY(TicketPrinter::buildText(plain).contains(QStringLiteral("SISTEMA DE VENTAS")));
    }
};

QTEST_MAIN(TstTicket)
#include "tst_ticket.moc"
