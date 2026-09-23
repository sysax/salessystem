#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include "../core/Result.h"

#include <optional>

class EventBus;

// Capa Services (arquitectura.txt): reglas de autenticación.
// Port de Repository.find_user + 2FA/recovery + ROLE_PERMISSIONS
// (data/repository.py) y data/db.py::_hash_password/_verify_password.
//
// Formato password: "salthex$dkhex" PBKDF2-HMAC-SHA256 100k, salt 16 B.
// Lockout: 3 intentos → 5 min. Recovery: token 6-hex, 30 min, 1 uso.
// La bitácora se escribe directo a audit_log (fase 3: AuditRepository).
class AuthService : public QObject
{
    Q_OBJECT

public:
    struct LoginResult {
        bool ok = false;
        bool totpRequired = false;
        bool mustChangePassword = false; // clave por defecto: forzar cambio
        QString username;
        QString role;
        QString error;
    };
    struct UserInfo {
        QString username;
        QString role;
        bool active = true;
        int failedAttempts = 0;
        QString lockedUntil;
        QString createdAt;
        QString lastLogin;
        bool totpEnabled = false;
    };

    // Roles válidos (fases.md §1, antes mock.ROLES)
    static const QStringList Roles;
    static int MaxFailedAttempts;
    static int LockoutMinutes;
    static int MinPasswordLength;

    explicit AuthService(QSqlDatabase db, EventBus *bus = nullptr, QObject *parent = nullptr);

    Result<LoginResult> login(const QString &username, const QString &password);
    void logout(const QString &username);
    bool canAccess(const QString &role, const QString &screen) const;

    // Gestión usuarios (admin)
    StatusResult addUser(const QString &username, const QString &password, const QString &role);
    StatusResult updateUser(const QString &username, const QString &newPassword,
                            const QString &newRole);
    StatusResult setUserActive(const QString &username, bool active);
    StatusResult resetPassword(const QString &username, const QString &newPassword);
    // Cambio propio de clave (verifica la actual): limpia must_change_password.
    StatusResult changePassword(const QString &username, const QString &currentPassword,
                                const QString &newPassword);
    StatusResult deleteUser(const QString &username);
    QList<UserInfo> listUsers() const;
    std::optional<UserInfo> findUser(const QString &username) const;

    // 2FA TOTP
    bool is2faEnabled(const QString &username) const;
    Result<QString> enable2fa(const QString &username); // retorna secreto pendiente
    Result<QStringList> confirm2fa(const QString &username, const QString &code);
    bool verify2fa(const QString &username, const QString &code);
    StatusResult disable2fa(const QString &username);
    Result<QStringList> regenerateRecoveryCodes(const QString &username);
    int recoveryCodesLeft(const QString &username) const;

    // Recuperación contraseña
    Result<QString> requestRecovery(const QString &username); // retorna token
    StatusResult redeemRecovery(const QString &username, const QString &token,
                                const QString &newPassword);

    // Utilidades cripto (públicas para tests de compatibilidad)
    static QString hashPassword(const QString &password);
    static bool verifyPassword(const QString &stored, const QString &password);

private:
    void audit(const QString &user, const QString &action, const QString &detail) const;
    static QString nowIso();
    static bool isLocked(const QString &lockedUntil, bool &expired);

    QSqlDatabase m_db;
    EventBus *m_bus = nullptr;
};
