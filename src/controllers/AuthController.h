#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include "../services/AuthService.h"

// Sesión y permisos para QML (antes SalesApp.current_user + navigate_to).
class AuthController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY sessionChanged)
    Q_PROPERTY(QString currentUser READ currentUser NOTIFY sessionChanged)
    Q_PROPERTY(QString currentRole READ currentRole NOTIFY sessionChanged)

public:
    explicit AuthController(AuthService *auth, QObject *parent = nullptr);

    bool loggedIn() const { return !m_user.isEmpty(); }
    QString currentUser() const { return m_user; }
    QString currentRole() const { return m_role; }

    Q_INVOKABLE QVariantMap login(const QString &username, const QString &password);
    Q_INVOKABLE QVariantMap verifyTotp(const QString &code);
    Q_INVOKABLE QVariantMap changePassword(const QString &username,
                                           const QString &currentPassword,
                                           const QString &newPassword);
    Q_INVOKABLE void logout();
    Q_INVOKABLE bool canAccess(const QString &screen) const;

signals:
    void sessionChanged();

private:
    AuthService *m_auth = nullptr;
    QString m_user;
    QString m_role;
    QString m_pendingUser; // login OK con contraseña, falta 2FA
    QString m_pendingRole;
    bool m_pendingMustChange = false;
};
