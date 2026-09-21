#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// TOTP RFC 6238 sin dependencias Qt extra: paso 30 s, SHA1, 6 dígitos,
// ventana ±1. Port fiel de data/totp.py — compatible con Google
// Authenticator / Authy.
namespace Totp
{
constexpr int StepSeconds = 30;
constexpr int Digits = 6;
constexpr int VerifyWindow = 1;

// 20 bytes aleatorios en Base32 sin padding
QString generateSecret(int numBytes = 20);
// Código vigente para un instante Unix (por defecto: ahora)
QString currentCode(const QString &secret, qint64 unixTime = -1);
bool verify(const QString &secret, const QString &code, int window = VerifyWindow,
            qint64 unixTime = -1);
QString provisioningUri(const QString &secret, const QString &account,
                        const QString &issuer = QStringLiteral("SistemaVentas"));
// 8 códigos de un solo uso formato XXXX-XXXX
QStringList generateRecoveryCodes(int n = 8);
// SHA256("sistema-ventas:"+CODIGO) — así se guardan en recovery_json
QString hashCode(const QString &code);
// Decodifica Base32 (mayúsculas, con/sin padding); vacío si inválido
QByteArray decodeBase32(const QString &secret);
// Codifica a Base32 RFC 4648 sin padding
QString encodeBase32(const QByteArray &raw);
} // namespace Totp
