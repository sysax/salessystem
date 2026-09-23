#include "AuthController.h"

AuthController::AuthController(AuthService *auth, QObject *parent)
    : QObject(parent), m_auth(auth)
{
}

QVariantMap AuthController::login(const QString &username, const QString &password)
{
    const auto r = m_auth->login(username, password);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    if (r.value().totpRequired) {
        m_pendingUser = r.value().username;
        m_pendingRole = r.value().role;
        m_pendingMustChange = r.value().mustChangePassword;
        return {{"ok", false},
                {"totpRequired", true},
                {"mustChangePassword", m_pendingMustChange},
                {"pendingUser", m_pendingUser}};
    }
    m_user = r.value().username;
    m_role = r.value().role;
    emit sessionChanged();
    return {{"ok", true},
            {"user", m_user},
            {"role", m_role},
            {"mustChangePassword", r.value().mustChangePassword}};
}

QVariantMap AuthController::verifyTotp(const QString &code)
{
    if (m_pendingUser.isEmpty())
        return {{"ok", false}, {"error", QStringLiteral("Sin sesión pendiente de 2FA")}};
    if (!m_auth->verify2fa(m_pendingUser, code))
        return {{"ok", false}, {"error", QStringLiteral("Código 2FA inválido")}};
    m_user = m_pendingUser;
    m_role = m_pendingRole;
    const bool mustChange = m_pendingMustChange;
    m_pendingUser.clear();
    m_pendingRole.clear();
    m_pendingMustChange = false;
    emit sessionChanged();
    return {{"ok", true}, {"user", m_user}, {"role", m_role}, {"mustChangePassword", mustChange}};
}

QVariantMap AuthController::changePassword(const QString &username,
                                           const QString &currentPassword,
                                           const QString &newPassword)
{
    const auto r = m_auth->changePassword(username, currentPassword, newPassword);
    if (!r.ok())
        return {{"ok", false}, {"error", r.error()}};
    return {{"ok", true}};
}

void AuthController::logout()
{
    if (!m_user.isEmpty())
        m_auth->logout(m_user);
    m_user.clear();
    m_role.clear();
    m_pendingUser.clear();
    m_pendingRole.clear();
    m_pendingMustChange = false;
    emit sessionChanged();
}

bool AuthController::canAccess(const QString &screen) const
{
    if (screen == QLatin1String("login"))
        return true;
    if (m_user.isEmpty())
        return false;
    return m_auth->canAccess(m_role, screen);
}
