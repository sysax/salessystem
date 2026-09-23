#include "EventBus.h"

const QString EventBus::UserLoggedIn = QStringLiteral("user_logged_in");
const QString EventBus::UserLoggedOut = QStringLiteral("user_logged_out");
const QString EventBus::UserLoginFailed = QStringLiteral("user_login_failed");
const QString EventBus::SaleCreated = QStringLiteral("sale_created");
const QString EventBus::InventoryUpdated = QStringLiteral("inventory_updated");
const QString EventBus::SyncStatusChanged = QStringLiteral("sync_status_changed");
const QString EventBus::CashOpened = QStringLiteral("cash_opened");
const QString EventBus::CashClosed = QStringLiteral("cash_closed");
const QString EventBus::SettingsChanged = QStringLiteral("settings_changed");

EventBus::EventBus(QObject *parent)
    : QObject(parent)
{
}

int EventBus::subscribe(const QString &event, Handler handler)
{
    QMutexLocker lock(&m_mutex);
    const int id = m_nextId++;
    m_handlers.emplace(id, std::make_pair(event, std::move(handler)));
    return id;
}

void EventBus::unsubscribe(int id)
{
    QMutexLocker lock(&m_mutex);
    m_handlers.erase(id);
}

void EventBus::publish(const QString &event, const Payload &payload)
{
    // Copiar handlers fuera del lock: un handler puede publicar a su vez.
    QList<Handler> targets;
    {
        QMutexLocker lock(&m_mutex);
        for (const auto &[id, entry] : m_handlers) {
            if (entry.first == event)
                targets.append(entry.second);
        }
    }
    for (const Handler &h : targets)
        h(payload);
    emit published(event, payload);
}
