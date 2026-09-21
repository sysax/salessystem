#include "Totp.h"

#include <QDateTime>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>

namespace Totp
{
namespace
{
// Contador TOTP (pasos de 30 s desde época Unix), big-endian 8 bytes
QByteArray counterMessage(qint64 unixTime)
{
    const quint64 counter = static_cast<quint64>(unixTime / StepSeconds);
    QByteArray msg(8, 0);
    for (int i = 7; i >= 0; --i) {
        msg[i] = static_cast<char>(counter >> ((7 - i) * 8));
    }
    return msg;
}

QString codeFor(const QByteArray &key, qint64 unixTime)
{
    if (key.isEmpty())
        return {};
    const QByteArray digest = QMessageAuthenticationCode::hash(
        counterMessage(unixTime), key, QCryptographicHash::Sha1);
    const int offset = digest.back() & 0x0F;
    quint32 num = 0;
    for (int i = 0; i < 4; ++i)
        num = (num << 8) | static_cast<quint8>(digest[offset + i]);
    num &= 0x7FFFFFFFu;
    quint32 mod = 1;
    for (int i = 0; i < Digits; ++i)
        mod *= 10;
    return QString::number(num % mod).rightJustified(Digits, u'0');
}
} // namespace

QString generateSecret(int numBytes)
{
    QByteArray raw(numBytes, 0);
    for (int i = 0; i < raw.size(); ++i)
        raw[i] = static_cast<char>(QRandomGenerator::system()->bounded(256));
    return encodeBase32(raw);
}

// Base32 RFC 4648 sin padding (el inverso de decodeBase32)

QString currentCode(const QString &secret, qint64 unixTime)
{
    if (unixTime < 0)
        unixTime = QDateTime::currentSecsSinceEpoch();
    return codeFor(decodeBase32(secret), unixTime);
}

bool verify(const QString &secret, const QString &code, int window, qint64 unixTime)
{
    QString clean = code.trimmed();
    clean.remove(u' ');
    if (clean.size() != Digits)
        return false;
    for (const QChar c : clean) {
        if (!c.isDigit())
            return false;
    }
    if (unixTime < 0)
        unixTime = QDateTime::currentSecsSinceEpoch();
    const QByteArray key = decodeBase32(secret);
    const qint64 base = unixTime / StepSeconds;
    for (int d = -window; d <= window; ++d) {
        if (codeFor(key, (base + d) * StepSeconds) == clean)
            return true;
    }
    return false;
}

QString provisioningUri(const QString &secret, const QString &account, const QString &issuer)
{
    return QStringLiteral("otpauth://totp/%1:%2?secret=%3&issuer=%1&digits=6&period=30")
        .arg(issuer, account, secret);
}

QStringList generateRecoveryCodes(int n)
{
    QStringList out;
    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        QByteArray raw(4, 0);
        for (int b = 0; b < 4; ++b)
            raw[b] = static_cast<char>(QRandomGenerator::system()->bounded(256));
        const QString hex = QString::fromLatin1(raw.toHex()).toUpper();
        out << hex.left(4) + u'-' + hex.mid(4);
    }
    return out;
}

QString hashCode(const QString &code)
{
    const QByteArray data =
        QStringLiteral("sistema-ventas:").toUtf8() + code.trimmed().toUpper().toUtf8();
    return QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

QByteArray decodeBase32(const QString &secret)
{
    static const QByteArray alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    static int table[256];
    static bool init = false;
    if (!init) {
        std::fill(std::begin(table), std::end(table), -1);
        for (int i = 0; i < alphabet.size(); ++i) {
            table[static_cast<unsigned char>(alphabet[i])] = i;
            table[static_cast<unsigned char>(alphabet[i] | 0x20)] = i; // minúsculas
        }
        init = true;
    }
    QByteArray clean = secret.toUpper().toLatin1();
    clean.replace("=", "");
    clean.replace(" ", "");
    if (clean.isEmpty())
        return {};
    QByteArray out;
    int buffer = 0, bits = 0;
    for (char c : clean) {
        const int v = table[static_cast<unsigned char>(c)];
        if (v < 0)
            return {};
        buffer = (buffer << 5) | v;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            out.append(static_cast<char>((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

QString encodeBase32(const QByteArray &raw)
{
    static const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    QString out;
    int buffer = 0, bits = 0;
    for (unsigned char c : raw) {
        buffer = (buffer << 8) | c;
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            out.append(QLatin1Char(alphabet[(buffer >> bits) & 31]));
        }
    }
    if (bits > 0)
        out.append(QLatin1Char(alphabet[(buffer << (5 - bits)) & 31]));
    return out;
}

} // namespace Totp
