#pragma once

#include <QObject>
#include <QVariantMap>

#include "../services/AuthService.h"
#include "../services/SettingsService.h"

// Configuración del negocio para QML (Fase 0 multinegocio).
// Patrón AuthController: Q_PROPERTY + Q_INVOKABLE que retornan {ok, error}.
class SettingsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)

public:
    explicit SettingsController(SettingsService *settings, AuthService *auth,
                                QObject *parent = nullptr);

    QVariantMap settings() const { return m_cache; }

    Q_INVOKABLE void load();
    Q_INVOKABLE QVariantMap save(const QVariantMap &m);
    Q_INVOKABLE bool canEdit() const;
    // Main.qml lo llama en onSessionChanged con auth.currentRole.
    Q_INVOKABLE void setRole(const QString &role);

signals:
    void settingsChanged();

private:
    SettingsService *m_settings = nullptr;
    AuthService *m_auth = nullptr;
    QString m_role; // rol capturado en load() para canEdit() sin sesión QML
    QVariantMap m_cache;
};
