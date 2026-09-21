#include "AuthService.h"
#include "Totp.h"
#include "../core/EventBus.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>

#include <unordered_map>
#include <vector>

const QStringList AuthService::Roles = {
    QStringLiteral("Administrador"), QStringLiteral("Vendedor"),
    QStringLiteral("Cajero"), QStringLiteral("Almacén"), QStringLiteral("Contador"),
};
int AuthService::MaxFailedAttempts = 3;
int AuthService::LockoutMinutes = 5;
int AuthService::MinPasswordLength = 4;

namespace
{
// PBKDF2-HMAC-SHA256 (RFC 2898). Qt6 eliminó QPasswordDigestor, así que
// se implementa con QMessageAuthenticationCode. Parámetros idénticos a
// hashlib.pbkdf2_hmac('sha256', pwd, salt16, 100_000) de data/db.py.
QByteArray pbkdf2Sha256(const QByteArray &password, const QByteArray &salt, int iterations,
                        int dkLen)
{
    QByteArray dk;
    const int blocks = (dkLen + 31) / 32;
    for (int block = 1; block <= blocks; ++block) {
        QByteArray input = salt;
        input.append(static_cast<char>((block >> 24) & 0xFF));
        input.append(static_cast<char>((block >> 16) & 0xFF));
        input.append(static_cast<char>((block >> 8) & 0xFF));
        input.append(static_cast<char>(block & 0xFF));
        QByteArray u = QMessageAuthenticationCode::hash(input, password,
                                                        QCryptographicHash::Sha256);
        QByteArray t = u;
        for (int i = 1; i < iterations; ++i) {
            u = QMessageAuthenticationCode::hash(u, password, QCryptographicHash::Sha256);
            for (int k = 0; k < t.size(); ++k)
                t[k] = static_cast<char>(t[k] ^ u[k]);
        }
        dk.append(t);
    }
    return dk.left(dkLen);
}

// ROLE_PERMISSIONS (data/repository.py). "*" = acceso total.
bool roleScreens(const QString &role, QStringList &out)
{
    static const std::unordered_map<QString, std::vector<QString>> kPerms = {
        {QStringLiteral("Administrador"), {QStringLiteral("*")}},
        {QStringLiteral("Vendedor"),
         {QStringLiteral("dashboard"), QStringLiteral("products"), QStringLiteral("pos"),
          QStringLiteral("sales"), QStringLiteral("clients"), QStringLiteral("inventory")}},
        {QStringLiteral("Cajero"),
         {QStringLiteral("dashboard"), QStringLiteral("pos"), QStringLiteral("sales")}},
        {QStringLiteral("Almacén"),
         {QStringLiteral("dashboard"), QStringLiteral("products"), QStringLiteral("inventory"),
          QStringLiteral("purchases"), QStringLiteral("suppliers")}},
        {QStringLiteral("Contador"),
         {QStringLiteral("dashboard"), QStringLiteral("sales"), QStringLiteral("receivables"),
          QStringLiteral("payables"), QStringLiteral("reports"), QStringLiteral("purchases")}},
    };
    const auto it = kPerms.find(role);
    if (it == kPerms.end())
        return false;
    out = QStringList(it->second.begin(), it->second.end());
    return true;
}
} // namespace

AuthService::AuthService(QSqlDatabase db, EventBus *bus, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_bus(bus)
{
}

QString AuthService::nowIso()
{
    return QDateTime::currentDateTime().toString(Qt::ISODateWithMs).left(19);
}

bool AuthService::isLocked(const QString &lockedUntil, bool &expired)
{
    expired = false;
    if (lockedUntil.isEmpty())
        return false;
    const QDateTime dt = QDateTime::fromString(lockedUntil, Qt::ISODate);
    if (!dt.isValid())
        return false;
    if (QDateTime::currentDateTime() < dt)
        return true;
    expired = true;
    return false;
}

QString AuthService::hashPassword(const QString &password)
{
    QByteArray salt(16, 0);
    for (int i = 0; i < salt.size(); ++i)
        salt[i] = static_cast<char>(QRandomGenerator::system()->bounded(256));
    const QByteArray dk = pbkdf2Sha256(password.toUtf8(), salt, 100000, 32);
    return QString::fromLatin1(salt.toHex()) + u'$' + QString::fromLatin1(dk.toHex());
}

bool AuthService::verifyPassword(const QString &stored, const QString &password)
{
    const int sep = stored.indexOf(u'$');
    if (sep <= 0)
        return stored == password; // fallback texto plano (migración)
    const QByteArray salt = QByteArray::fromHex(stored.left(sep).toLatin1());
    const QByteArray want = QByteArray::fromHex(stored.mid(sep + 1).toLatin1());
    if (salt.size() != 16 || want.size() != 32)
        return false;
    const QByteArray dk = pbkdf2Sha256(password.toUtf8(), salt, 100000, 32);
    if (dk.size() != want.size())
        return false;
    // Comparación en tiempo constante para no filtrar por timing
    volatile int diff = 0;
    for (int i = 0; i < dk.size(); ++i)
        diff |= dk[i] ^ want[i];
    return diff == 0;
}

void AuthService::audit(const QString &user, const QString &action, const QString &detail) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO audit_log (ts, user, action, detail) VALUES (?,?,?,?)"));
    q.addBindValue(nowIso());
    q.addBindValue(user);
    q.addBindValue(action);
    q.addBindValue(detail);
    q.exec();
}

Result<AuthService::LoginResult> AuthService::login(const QString &username,
                                                    const QString &password)
{
    LoginResult r;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM users WHERE username=?"));
    q.addBindValue(username);
    if (!q.exec() || !q.next())
        return Result<LoginResult>::failure(QStringLiteral("Usuario o clave inválidos"));

    const QString stored = q.value(QStringLiteral("password")).toString();
    const bool active = q.value(QStringLiteral("active")).toInt() != 0;
    if (!active)
        return Result<LoginResult>::failure(QStringLiteral("Usuario inactivo"));

    bool expired = false;
    if (isLocked(q.value(QStringLiteral("locked_until")).toString(), expired)) {
        if (m_bus)
            m_bus->publish(EventBus::UserLoginFailed, {{"username", username}});
        return Result<LoginResult>::failure(QStringLiteral("Bloqueado por intentos — espere 5 min"));
    }
    if (expired) {
        QSqlQuery reset(m_db);
        reset.prepare(QStringLiteral(
            "UPDATE users SET failed_attempts=0, locked_until=NULL WHERE username=?"));
        reset.addBindValue(username);
        reset.exec();
    }

    if (verifyPassword(stored, password)) {
        QSqlQuery up(m_db);
        if (stored.indexOf(u'$') <= 0) {
            // Migración: re-hashear contraseñas en texto plano al entrar
            up.prepare(QStringLiteral(
                "UPDATE users SET password=?, failed_attempts=0, locked_until=NULL, "
                "last_login=? WHERE username=?"));
            up.addBindValue(hashPassword(password));
        } else {
            up.prepare(QStringLiteral(
                "UPDATE users SET failed_attempts=0, locked_until=NULL, last_login=? "
                "WHERE username=?"));
        }
        up.addBindValue(nowIso());
        up.addBindValue(username);
        up.exec();

        r.ok = true;
        r.username = username;
        r.role = q.value(QStringLiteral("role")).toString();
        r.totpRequired = q.value(QStringLiteral("totp_enabled")).toInt() != 0;
        if (m_bus)
            m_bus->publish(EventBus::UserLoggedIn, {{"username", username}, {"role", r.role}});
        return Result<LoginResult>::success(r);
    }

    const int attempts = q.value(QStringLiteral("failed_attempts")).toInt() + 1;
    QString lockedUntil;
    if (attempts >= MaxFailedAttempts) {
        lockedUntil = QDateTime::currentDateTime()
                          .addSecs(LockoutMinutes * 60)
                          .toString(Qt::ISODateWithMs)
                          .left(19);
    }
    QSqlQuery fail(m_db);
    fail.prepare(QStringLiteral(
        "UPDATE users SET failed_attempts=?, locked_until=? WHERE username=?"));
    fail.addBindValue(attempts);
    fail.addBindValue(lockedUntil.isEmpty() ? QVariant() : QVariant(lockedUntil));
    fail.addBindValue(username);
    fail.exec();
    if (m_bus)
        m_bus->publish(EventBus::UserLoginFailed, {{"username", username}});
    return Result<LoginResult>::failure(QStringLiteral("Usuario o clave inválidos"));
}

void AuthService::logout(const QString &username)
{
    audit(username, QStringLiteral("logout"), {});
    if (m_bus)
        m_bus->publish(EventBus::UserLoggedOut, {{"username", username}});
}

bool AuthService::canAccess(const QString &role, const QString &screen) const
{
    QStringList screens;
    if (!roleScreens(role, screens))
        return false;
    return screens.contains(QStringLiteral("*")) || screens.contains(screen);
}

QList<AuthService::UserInfo> AuthService::listUsers() const
{
    QList<UserInfo> out;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral(
        "SELECT username, role, active, failed_attempts, locked_until, created_at, "
        "last_login, totp_enabled FROM users ORDER BY username"));
    while (q.next()) {
        UserInfo u;
        u.username = q.value(0).toString();
        u.role = q.value(1).toString();
        u.active = q.value(2).toInt() != 0;
        u.failedAttempts = q.value(3).toInt();
        u.lockedUntil = q.value(4).toString();
        u.createdAt = q.value(5).toString();
        u.lastLogin = q.value(6).toString();
        u.totpEnabled = q.value(7).toInt() != 0;
        out << u;
    }
    return out;
}

std::optional<AuthService::UserInfo> AuthService::findUser(const QString &username) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT username, role, active, failed_attempts, locked_until, created_at, "
        "last_login, totp_enabled FROM users WHERE username=?"));
    q.addBindValue(username);
    if (!q.exec() || !q.next())
        return std::nullopt;
    UserInfo u;
    u.username = q.value(0).toString();
    u.role = q.value(1).toString();
    u.active = q.value(2).toInt() != 0;
    u.failedAttempts = q.value(3).toInt();
    u.lockedUntil = q.value(4).toString();
    u.createdAt = q.value(5).toString();
    u.lastLogin = q.value(6).toString();
    u.totpEnabled = q.value(7).toInt() != 0;
    return u;
}

StatusResult AuthService::addUser(const QString &username, const QString &password,
                                  const QString &role)
{
    if (findUser(username))
        return StatusResult::failure(QStringLiteral("Usuario ya existe"));
    if (!Roles.contains(role))
        return StatusResult::failure(QStringLiteral("Rol inválido: %1").arg(role));
    if (password.size() < MinPasswordLength)
        return StatusResult::failure(QStringLiteral("Contraseña mínimo 4 caracteres"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO users (username, password, role, active, failed_attempts, created_at) "
        "VALUES (?,?,?,?,?,?)"));
    q.addBindValue(username);
    q.addBindValue(hashPassword(password));
    q.addBindValue(role);
    q.addBindValue(1);
    q.addBindValue(0);
    q.addBindValue(nowIso());
    if (!q.exec())
        return StatusResult::failure(q.lastError().text());
    audit(username, QStringLiteral("alta_usuario"), QStringLiteral("rol=%1").arg(role));
    return StatusResult::success({});
}

StatusResult AuthService::updateUser(const QString &username, const QString &newPassword,
                                     const QString &newRole)
{
    if (!findUser(username))
        return StatusResult::failure(QStringLiteral("Usuario no encontrado"));
    QSqlQuery q(m_db);
    if (!newPassword.isEmpty()) {
        if (newPassword.size() < MinPasswordLength)
            return StatusResult::failure(QStringLiteral("Contraseña mínimo 4"));
        q.prepare(QStringLiteral("UPDATE users SET password=? WHERE username=?"));
        q.addBindValue(hashPassword(newPassword));
        q.addBindValue(username);
        if (!q.exec())
            return StatusResult::failure(q.lastError().text());
    }
    if (!newRole.isEmpty()) {
        if (!Roles.contains(newRole))
            return StatusResult::failure(QStringLiteral("Rol inválido: %1").arg(newRole));
        q.prepare(QStringLiteral("UPDATE users SET role=? WHERE username=?"));
        q.addBindValue(newRole);
        q.addBindValue(username);
        if (!q.exec())
            return StatusResult::failure(q.lastError().text());
    }
    return StatusResult::success({});
}

StatusResult AuthService::setUserActive(const QString &username, bool active)
{
    if (username == QLatin1String("admin") && !active)
        return StatusResult::failure(QStringLiteral("No se puede bloquear admin"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE users SET active=?, failed_attempts=0, locked_until=NULL WHERE username=?"));
    q.addBindValue(active ? 1 : 0);
    q.addBindValue(username);
    if (!q.exec() || q.numRowsAffected() == 0)
        return StatusResult::failure(QStringLiteral("Usuario no encontrado"));
    audit(QStringLiteral("sistema"),
          active ? QStringLiteral("usuario_desbloqueo") : QStringLiteral("usuario_bloqueo"),
          username);
    return StatusResult::success({});
}

StatusResult AuthService::resetPassword(const QString &username, const QString &newPassword)
{
    if (newPassword.size() < MinPasswordLength)
        return StatusResult::failure(QStringLiteral("Contraseña mínimo 4"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE users SET password=?, failed_attempts=0, locked_until=NULL WHERE username=?"));
    q.addBindValue(hashPassword(newPassword));
    q.addBindValue(username);
    if (!q.exec() || q.numRowsAffected() == 0)
        return StatusResult::failure(QStringLiteral("Usuario no encontrado"));
    audit(QStringLiteral("sistema"), QStringLiteral("reset_password"), username);
    return StatusResult::success({});
}

StatusResult AuthService::deleteUser(const QString &username)
{
    if (username == QLatin1String("admin"))
        return StatusResult::failure(QStringLiteral("No se puede eliminar admin"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM users WHERE username=?"));
    q.addBindValue(username);
    if (!q.exec() || q.numRowsAffected() == 0)
        return StatusResult::failure(QStringLiteral("Usuario no encontrado"));
    return StatusResult::success({});
}

bool AuthService::is2faEnabled(const QString &username) const
{
    const auto u = findUser(username);
    return u && u->totpEnabled;
}

Result<QString> AuthService::enable2fa(const QString &username)
{
    if (!findUser(username))
        return Result<QString>::failure(QStringLiteral("Usuario no encontrado"));
    const QString secret = Totp::generateSecret();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE users SET totp_secret=?, totp_enabled=0 WHERE username=?"));
    q.addBindValue(secret);
    q.addBindValue(username);
    if (!q.exec())
        return Result<QString>::failure(q.lastError().text());
    audit(QStringLiteral("sistema"), QStringLiteral("2fa_secret"), username);
    return Result<QString>::success(secret);
}

Result<QStringList> AuthService::confirm2fa(const QString &username, const QString &code)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT totp_secret FROM users WHERE username=?"));
    q.addBindValue(username);
    if (!q.exec() || !q.next() || q.value(0).toString().isEmpty())
        return Result<QStringList>::failure(
            QStringLiteral("Sin secreto 2FA — genere primero"));
    if (!Totp::verify(q.value(0).toString(), code))
        return Result<QStringList>::failure(QStringLiteral("Código 2FA inválido"));
    const QStringList codes = Totp::generateRecoveryCodes(8);
    QJsonArray hashed;
    for (const QString &c : codes)
        hashed << Totp::hashCode(c);
    QSqlQuery up(m_db);
    up.prepare(QStringLiteral("UPDATE users SET totp_enabled=1, recovery_json=? WHERE username=?"));
    up.addBindValue(QString::fromUtf8(QJsonDocument(hashed).toJson(QJsonDocument::Compact)));
    up.addBindValue(username);
    up.exec();
    audit(username, QStringLiteral("2fa_activado"), {});
    return Result<QStringList>::success(codes);
}

bool AuthService::verify2fa(const QString &username, const QString &code)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT totp_secret, recovery_json FROM users WHERE username=?"));
    q.addBindValue(username);
    if (!q.exec() || !q.next())
        return false;
    const QString secret = q.value(0).toString();
    if (!secret.isEmpty() && Totp::verify(secret, code))
        return true;
    // Código de recuperación (un solo uso: se consume)
    const QByteArray raw = q.value(1).toByteArray();
    QJsonArray arr = QJsonDocument::fromJson(raw.isEmpty() ? "[]" : raw).array();
    const QString h = Totp::hashCode(code);
    for (int i = 0; i < arr.size(); ++i) {
        if (arr[i].toString() == h) {
            arr.removeAt(i);
            QSqlQuery up(m_db);
            up.prepare(QStringLiteral("UPDATE users SET recovery_json=? WHERE username=?"));
            up.addBindValue(
                QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
            up.addBindValue(username);
            up.exec();
            audit(username, QStringLiteral("2fa_recovery_usado"),
                  QStringLiteral("quedan %1").arg(arr.size()));
            return true;
        }
    }
    return false;
}

StatusResult AuthService::disable2fa(const QString &username)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE users SET totp_secret=NULL, totp_enabled=0, recovery_json='[]' WHERE username=?"));
    q.addBindValue(username);
    if (!q.exec() || q.numRowsAffected() == 0)
        return StatusResult::failure(QStringLiteral("Usuario no encontrado"));
    audit(QStringLiteral("sistema"), QStringLiteral("2fa_desactivado"), username);
    return StatusResult::success({});
}

Result<QStringList> AuthService::regenerateRecoveryCodes(const QString &username)
{
    if (!is2faEnabled(username))
        return Result<QStringList>::failure(QStringLiteral("2FA no activo para este usuario"));
    const QStringList codes = Totp::generateRecoveryCodes(8);
    QJsonArray hashed;
    for (const QString &c : codes)
        hashed << Totp::hashCode(c);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE users SET recovery_json=? WHERE username=?"));
    q.addBindValue(QString::fromUtf8(QJsonDocument(hashed).toJson(QJsonDocument::Compact)));
    q.addBindValue(username);
    q.exec();
    audit(username, QStringLiteral("2fa_recovery_regenerado"), {});
    return Result<QStringList>::success(codes);
}

int AuthService::recoveryCodesLeft(const QString &username) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT recovery_json FROM users WHERE username=?"));
    q.addBindValue(username);
    if (!q.exec() || !q.next())
        return 0;
    const QByteArray raw = q.value(0).toByteArray();
    return QJsonDocument::fromJson(raw.isEmpty() ? "[]" : raw).array().size();
}

Result<QString> AuthService::requestRecovery(const QString &username)
{
    if (!findUser(username))
        return Result<QString>::failure(QStringLiteral("Usuario no existe"));
    QByteArray raw(3, 0);
    for (int i = 0; i < 3; ++i)
        raw[i] = static_cast<char>(QRandomGenerator::system()->bounded(256));
    const QString token = QString::fromLatin1(raw.toHex()).toUpper();
    const QString now = nowIso();
    const QString exp =
        QDateTime::currentDateTime().addSecs(30 * 60).toString(Qt::ISODateWithMs).left(19);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO recovery_tokens (token, username, created_ts, expires_ts, used) "
        "VALUES (?,?,?,?,0)"));
    q.addBindValue(token);
    q.addBindValue(username);
    q.addBindValue(now);
    q.addBindValue(exp);
    if (!q.exec())
        return Result<QString>::failure(q.lastError().text());
    audit(username, QStringLiteral("recovery_solicitado"),
          QStringLiteral("expira %1").arg(exp));
    return Result<QString>::success(token);
}

StatusResult AuthService::redeemRecovery(const QString &username, const QString &token,
                                         const QString &newPassword)
{
    if (newPassword.size() < MinPasswordLength)
        return StatusResult::failure(QStringLiteral("Contraseña mínimo 4"));
    const QString clean = token.trimmed().toUpper();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT username, expires_ts, used FROM recovery_tokens WHERE token=?"));
    q.addBindValue(clean);
    if (!q.exec() || !q.next())
        return StatusResult::failure(QStringLiteral("Token inválido"));
    if (q.value(0).toString().toLower() != username.trimmed().toLower())
        return StatusResult::failure(QStringLiteral("Token no corresponde a este usuario"));
    if (q.value(2).toInt() != 0)
        return StatusResult::failure(QStringLiteral("Token ya usado"));
    const QDateTime exp = QDateTime::fromString(q.value(1).toString(), Qt::ISODate);
    if (!exp.isValid() || QDateTime::currentDateTime() > exp)
        return StatusResult::failure(QStringLiteral("Token expirado (30 min)"));
    QSqlQuery up(m_db);
    up.prepare(QStringLiteral(
        "UPDATE users SET password=?, failed_attempts=0, locked_until=NULL WHERE username=?"));
    up.addBindValue(hashPassword(newPassword));
    up.addBindValue(username);
    up.exec();
    QSqlQuery used(m_db);
    used.prepare(QStringLiteral("UPDATE recovery_tokens SET used=1 WHERE token=?"));
    used.addBindValue(clean);
    used.exec();
    audit(username, QStringLiteral("recovery_completado"), {});
    return StatusResult::success({});
}
