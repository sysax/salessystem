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

    void rateHalfUp()
    {
        // Fase 2: tasa genérica (multinegocio) con round-half-up al céntimo.
        QCOMPARE(Money::taxCents(10000, 19.0), (qint64)1900);    // 100.00 * 19 %
        QCOMPARE(Money::taxCents(10000, 5.0), (qint64)500);      // 100.00 * 5 %
        QCOMPARE(Money::taxCents(599997, 19.0), (qint64)113999); // 5999.97 * 19 %
        QCOMPARE(Money::taxCents(1, 50.0), (qint64)1);           // 0.01 * 50 % = 0.005 → 1
        QCOMPARE(Money::taxCents(199, 19.0), (qint64)38);        // 1.99 * 19 % = 0.3781 → 38
        QCOMPARE(Money::withRate(Money::fromCop(100000.0), 5.0).toCop(), 105000.0);
    }

    void formatCop()
    {
        const QString s = Money::fromCop(1850000.0).format();
        QVERIFY(s.startsWith(QStringLiteral("$ ")));
        QVERIFY(s.contains(QStringLiteral("1.850.000")));
        QVERIFY(!s.contains(u'.') || s.endsWith(QStringLiteral("0")));
    }

    void fromCopHalfAway()
    {
        // fromCop usa llround (half-away sobre el valor double real).
        // Ojo binario: 1.005 en double es 1.004999... → 100 cents.
        QCOMPARE(Money::fromCop(1.005).cents(), (qint64)100);
        // Casos exactos en binario sí redondean half-away hacia arriba.
        QCOMPARE(Money::fromCop(0.005).cents(), (qint64)1);
        QCOMPARE(Money::fromCop(2.675).cents(), (qint64)268);
        QCOMPARE(Money::fromCop(-0.005).cents(), (qint64)-1);
    }

    void tryParseEsCo()
    {
        Money out;
        QString err;
        QVERIFY(Money::tryParse(QStringLiteral("10.000,50"), false, out, err));
        QCOMPARE(out.cents(), (qint64)1000050);
        QVERIFY(Money::tryParse(QStringLiteral("1.850.000"), false, out, err));
        QCOMPARE(out.cents(), (qint64)185000000);
        // "10.999" con un punto y 3 decimales se lee como miles es_CO
        // ("1.850" ⇒ 1850): 10999 COP.
        QVERIFY(Money::tryParse(QStringLiteral("10.999"), false, out, err));
        QCOMPARE(out.cents(), (qint64)1099900);
        // Con coma decimal, 3 decimales sí se rechazan (>2 decimales).
        QVERIFY(!Money::tryParse(QStringLiteral("10,999"), false, out, err));
        // Negativos rechazados por defecto, aceptados con flag.
        QVERIFY(!Money::tryParse(QStringLiteral("-5"), false, out, err));
        QVERIFY(Money::tryParse(QStringLiteral("-5"), true, out, err));
        QCOMPARE(out, Money::fromCop(-5.0));
    }

    void maxCentsClamp()
    {
        // Tope operativo: fromCop recorta a MaxCents, tryParse lo rechaza.
        QCOMPARE(Money::fromCop(1e15).cents(), Money::MaxCents);
        Money out;
        QString err;
        QVERIFY(!Money::tryParse(QStringLiteral("99999999999999"), false, out, err));
    }

    void operators()
    {
        const Money a = Money::fromCop(100.0);
        const Money b = Money::fromCop(100.0);
        QVERIFY(a == b);
        QVERIFY(!(a < b));
        QVERIFY(a <= b);
        QVERIFY(!(a > b));
        QVERIFY(a >= b);
        QVERIFY(Money() < a);
        // Multiplicación por cantidad entera y fraccionaria (granel).
        QCOMPARE((a * 3).cents(), Money::fromCop(300.0).cents());
        QCOMPARE((a * 0.35).cents(), Money::fromCop(35.0).cents());
        QCOMPARE((0.35 * a).cents(), Money::fromCop(35.0).cents());
    }
};

QTEST_MAIN(TstMoney)
#include "tst_money.moc"
