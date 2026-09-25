#pragma once

#include <QDateTime>
#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <optional>

#include "../services/AuthService.h"

class EventBus;

// Sesión y permisos para QML (antes SalesApp.current_user + navigate_to).
class AuthController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY sessionChanged)
    Q_PROPERTY(QString currentUser READ currentUser NOTIFY sessionChanged)
    Q_PROPERTY(QString currentRole READ currentRole NOTIFY sessionChanged)
    Q_PROPERTY(bool loginBusy READ loginBusy NOTIFY loginBusyChanged)

  public:
    explicit AuthController(AuthService *auth, EventBus *bus = nullptr, QObject *parent = nullptr);

    // Salida del login en hilo de trabajo (pública para el worker).
    struct AsyncOut
    {
        std::optional<Result<AuthService::LoginResult>> result;
        QList<QPair<QString, QVariantMap>> events; // para re-publicar en UI
    };

    // Fase 2: expiración por inactividad en minutos (0 = sin expiración).
    static int SessionTimeoutMinutes;

    bool loggedIn() const
    {
        return !m_user.isEmpty();
    }
    QString currentUser() const
    {
        return m_user;
    }
    QString currentRole() const
    {
        return m_role;
    }
    bool loginBusy() const
    {
        return m_loginBusy;
    }

    Q_INVOKABLE QVariantMap login(const QString &username, const QString &password);
    // Fase 2: login fuera del hilo UI (PBKDF2 600k). Retorna al instante;
    // el resultado llega por loginFinished (misma forma que login()).
    Q_INVOKABLE void loginAsync(const QString &username, const QString &password);
    Q_INVOKABLE QVariantMap verifyTotp(const QString &code);
    Q_INVOKABLE QVariantMap changePassword(const QString &username, const QString &currentPassword,
                                           const QString &newPassword);
    Q_INVOKABLE void logout();
    Q_INVOKABLE bool canAccess(const QString &screen) const;
    // Fase 2: registra actividad (navegación/operación) y expiración.
    Q_INVOKABLE void touch();
    // true si expiró (y cerró la sesión). La UI lo llama cada minuto.
    Q_INVOKABLE bool checkIdle();

  signals:
    void sessionChanged();
    void loginBusyChanged();
    void loginFinished(const QVariantMap &result);

  private:
    QVariantMap applyLoginResult(const Result<AuthService::LoginResult> &r);

    AuthService *m_auth = nullptr;
    EventBus *m_bus = nullptr;
    QString m_user;
    QString m_role;
    QString m_pendingUser; // login OK con contraseña, falta 2FA
    QString m_pendingRole;
    bool m_pendingMustChange = false;
    // Async (watcher vive en el hilo UI; el trabajo corre en el pool).
    QFutureWatcher<AsyncOut> m_watcher;
    bool m_loginBusy = false;
    QDateTime m_lastActivity;
};
