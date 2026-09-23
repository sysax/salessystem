#include "SettingsController.h"

SettingsController::SettingsController(SettingsService *settings, AuthService *auth,
                                       QObject *parent)
    : QObject(parent), m_settings(settings), m_auth(auth)
{
    if (m_settings) {
        connect(m_settings, &SettingsService::settingsChanged, this, [this] {
            load();
        });
    }
    load();
}

void SettingsController::load()
{
    if (!m_settings)
        return;
    m_cache = m_settings->all();
    emit settingsChanged();
}

QVariantMap SettingsController::save(const QVariantMap &m)
{
    if (!canEdit())
        return {{"ok", false}, {"error", QStringLiteral("Solo el administrador puede cambiar la configuración")}};
    const auto r = m_settings->save(m);
    if (!r.value(QStringLiteral("ok"), false).toBool())
        return r;
    load();
    return {{"ok", true}};
}

bool SettingsController::canEdit() const
{
    // Sin rol conocido (tests, arranque) se permite; la UI real fija el rol
    // desde auth.currentRole y solo admin edita.
    if (m_role.isEmpty())
        return true;
    return m_role == QLatin1String("Administrador");
}

void SettingsController::setRole(const QString &role)
{
    m_role = role;
}
