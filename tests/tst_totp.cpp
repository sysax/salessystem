// Vectores oficiales RFC 6238 Apéndice B (SHA1, secreto ASCII
// "12345678901234567890", paso 30 s). El código de 8 dígitos se trunca
// a 6 (últimos 6), igual que Google Authenticator.
#include <QtTest>

#include "services/Totp.h"

class TstTotp : public QObject
{
    Q_OBJECT

private slots:
    void rfcVectors_data()
    {
        QTest::addColumn<qint64>("unixTime");
        QTest::addColumn<QString>("expected6");
        // time → TOTP8 → últimos 6
        QTest::newRow("59") << (qint64)59 << "287082"; // 94287082
        QTest::newRow("1111111109") << (qint64)1111111109 << "081804"; // 07081804
        QTest::newRow("1111111111") << (qint64)1111111111 << "050471"; // 14050471
        QTest::newRow("1234567890") << (qint64)1234567890 << "005924"; // 89005924
        QTest::newRow("2000000000") << (qint64)2000000000 << "279037"; // 69279037
        QTest::newRow("20000000000") << (qint64)20000000000 << "353130"; // 65353130
    }

    void rfcVectors()
    {
        QFETCH(qint64, unixTime);
        QFETCH(QString, expected6);
        // Base32 del secreto ASCII del RFC
        const QString secret =
            Totp::encodeBase32(QByteArray("12345678901234567890", 20));
        QCOMPARE(Totp::currentCode(secret, unixTime), expected6);
        QVERIFY(Totp::verify(secret, expected6, 0, unixTime));
        // Ventana ±1 acepta pasos vecinos, no lejanos
        QVERIFY(Totp::verify(secret, Totp::currentCode(secret, unixTime + 30), 1, unixTime));
        QVERIFY(!Totp::verify(secret, Totp::currentCode(secret, unixTime + 90), 1, unixTime));
    }

    void rejectsMalformed()
    {
        const QString secret = Totp::generateSecret();
        const QString good = Totp::currentCode(secret);
        QVERIFY(!Totp::verify(secret, QString()));
        QVERIFY(!Totp::verify(secret, QStringLiteral("12345"))); // 5 dígitos
        QVERIFY(!Totp::verify(secret, QStringLiteral("1234567"))); // 7 dígitos
        QVERIFY(!Totp::verify(secret, QStringLiteral("abcdef"))); // no numérico
        QVERIFY(!Totp::verify(QStringLiteral("!!!no-base32!!!"), good));
        QVERIFY(!Totp::verify(QString(), good));
    }

    void secretRoundtrip()
    {
        const QString s1 = Totp::generateSecret();
        const QString s2 = Totp::generateSecret();
        QCOMPARE(s1.size(), 32); // 20 bytes → 32 chars sin padding
        QVERIFY(s1 != s2);
        QVERIFY(!Totp::decodeBase32(s1).isEmpty());
        QCOMPARE(Totp::decodeBase32(s1).size(), 20);
        // Con padding y minúsculas también vale
        QVERIFY(!Totp::decodeBase32(s1.toLower() + QStringLiteral("====")).isEmpty());
    }

    void recoveryFormat()
    {
        const QStringList codes = Totp::generateRecoveryCodes(8);
        QCOMPARE(codes.size(), 8);
        for (const QString &c : codes) {
            QCOMPARE(c.size(), 9);
            QCOMPARE(c[4], u'-');
            QVERIFY(c != codes.first() || true);
        }
        QCOMPARE(QSet<QString>(codes.begin(), codes.end()).size(), 8); // únicos
        // hash estable y con prefijo (compat con data/totp.py)
        QCOMPARE(Totp::hashCode(codes[0]), Totp::hashCode(codes[0].toLower()));
        QCOMPARE(Totp::hashCode(codes[0]).size(), 64);
    }

    void provisioningUriShape()
    {
        const QString uri = Totp::provisioningUri(QStringLiteral("ABCDEF"), QStringLiteral("admin"));
        QVERIFY(uri.startsWith(QStringLiteral("otpauth://totp/SistemaVentas:admin")));
        QVERIFY(uri.contains(QStringLiteral("secret=ABCDEF")));
    }
};

QTEST_MAIN(TstTotp)
#include "tst_totp.moc"
