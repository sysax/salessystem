#include "AuthController.h"

#include <QtConcurrent>

#include <QSqlDatabase>
#include <QThread>

#include "../core/EventBus.h"

int AuthController::SessionTimeoutMinutes = 30;

namespace
{
// Login completo en hilo de trabajo: conexión SQLite propia creada dentro
// del worker (las conexiones solo se usan en su hilo creador) y bus local;
// los eventos se capturan y el hilo UI los re-publica al terminar.
AuthController::AsyncOut workerLogin(AuthService::DbCloneParams params, const QString &username,
                                     const QString &password)
{
    AuthController::AsyncOut out;
    const QString workerConn = QStringLiteral("auth_worker_%1")
                                   .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(params.driver, workerConn);
        db.setDatabaseName(params.dbName);
        db.setConnectOptions(params.options);
        if (!db.open())
            return out; // result vacío = error de infraestructura
        EventBus workerBus;
        workerBus.subscribe(EventBus::UserLoggedIn, [&](const QVariantMap &p) {
            out.events << qMakePair(EventBus::UserLoggedIn, p);
        });
        workerBus.subscribe(EventBus::UserLoginFailed, [&](const QVariantMap &p) {
            out.events << qMakePair(EventBus::UserLoginFailed, p);
        });
        AuthService svc(db, &workerBus);
        out.result = svc.login(username, password);
        db.close();
    }
    QSqlDatabase::removeDatabase(workerConn);
    return out;
}
} // namespace

AuthController::AuthController(AuthService *auth, EventBus *bus, QObject *parent)
    : QObject(parent), m_auth(auth), m_bus(bus)
{
    connect(&m_watcher, &QFutureWatcher<AsyncOut>::finished, this, [this] {
        const AsyncOut out = m_watcher.result();
        m_loginBusy = false;
        emit loginBusyChanged();
        // Re-publicar en el hilo UI los eventos del worker.
        if (m_bus) {
            for (const auto &e : out.events)
                m_bus->publish(e.first, e.second);
        }
        if (!out.result.has_value()) {
            emit loginFinished(QVariantMap{
                {"ok", false}, {"error", QStringLiteral("No se pudo verificar la clave")}});
            return;
        }
        emit loginFinished(applyLoginResult(*out.result));
    });
}

QVariantMap AuthController::applyLoginResult(const Result<AuthService::LoginResult> &r)
{
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
    touch();
    emit sessionChanged();
    return {{"ok", true},
            {"user", m_user},
            {"role", m_role},
            {"mustChangePassword", r.value().mustChangePassword}};
}

QVariantMap AuthController::login(const QString &username, const QString &password)
{
    return applyLoginResult(m_auth->login(username, password));
}

void AuthController::loginAsync(const QString &username, const QString &password)
{
    if (m_loginBusy)
        return;
    m_loginBusy = true;
    emit loginBusyChanged();
    const AuthService::DbCloneParams params
        = m_auth ? m_auth->cloneParams() : AuthService::DbCloneParams{};
    m_watcher.setFuture(QtConcurrent::run(workerLogin, params, username, password));
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
    touch();
    emit sessionChanged();
    return {{"ok", true}, {"user", m_user}, {"role", m_role}, {"mustChangePassword", mustChange}};
}

QVariantMap AuthController::changePassword(const QString &username, const QString &currentPassword,
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

void AuthController::touch()
{
    if (!m_user.isEmpty())
        m_lastActivity = QDateTime::currentDateTime();
}

bool AuthController::checkIdle()
{
    if (m_user.isEmpty() || SessionTimeoutMinutes <= 0 || !m_lastActivity.isValid())
        return false;
    if (m_lastActivity.secsTo(QDateTime::currentDateTime()) < SessionTimeoutMinutes * 60)
        return false;
    logout(); // emite sessionChanged; la UI regresa al login
    return true;
}
