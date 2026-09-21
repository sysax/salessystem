// Money: céntimos exactos, formato COP, IVA 19%.
#include <QtTest>

#include "core/Money.h"

class TstMoney : public QObject
{
    Q_OBJECT

private slots:
    void roundtrip()
    {
        QCOMPARE(Money::fromCop(1850000.0).cents(), (qint64)185000000);
        QCOMPARE(Money::fromCop(1850000.0).toCop(), 1850000.0);
        QCOMPARE(Money::fromCents(1).toCop(), 0.01);
    }

    void arithmetic()
    {
        const Money a = Money::fromCop(100000.0);
        const Money b = Money::fromCop(50000.0);
        QCOMPARE((a + b).toCop(), 150000.0);
        QCOMPARE((a - b).toCop(), 50000.0);
        QCOMPARE((b * 3).toCop(), 150000.0);
        QVERIFY(b < a);
    }

    void iva19()
    {
        // 100.000 neto + 19 % = 119.000 (port de sales_service IVA por línea)
        QCOMPARE(Money::withIva(Money::fromCop(100000.0)).toCop(), 119000.0);
        // Redondeo al céntimo: 10.005 * 1.19
        const Money net = Money::fromCents(1000500); // 10.005,00
        QCOMPARE(Money::withIva(net).cents(), (qint64)1190595);
    }

    void formatCop()
    {
        const QString s = Money::fromCop(1850000.0).format();
        QVERIFY(s.startsWith(QStringLiteral("$ ")));
        QVERIFY(s.contains(QStringLiteral("1.850.000")));
        QVERIFY(!s.contains(u'.') || s.endsWith(QStringLiteral("0")));
    }
};

QTEST_MAIN(TstMoney)
#include "tst_money.moc"
