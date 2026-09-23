#pragma once

#include <QMutex>
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <functional>
#include <unordered_map>

// Bus de eventos intra-proceso (port de utils/event_bus.py).
// Los servicios publican; QML/controllers se suscriben vía la señal
// published() o vía handlers C++. Thread-safe.
class EventBus : public QObject
{
    Q_OBJECT

public:
    using Payload = QVariantMap;
    using Handler = std::function<void(const Payload &)>;

    // Nombres canónicos (antes constantes en event_bus.py)
    static const QString UserLoggedIn;
    static const QString UserLoggedOut;
    static const QString UserLoginFailed;
    static const QString SaleCreated;
    static const QString InventoryUpdated;
    static const QString SyncStatusChanged;
    static const QString CashOpened;
    static const QString CashClosed;
    static const QString SettingsChanged;

    explicit EventBus(QObject *parent = nullptr);

    // Suscribe un handler C++; retorna id para unsubscribe()
    int subscribe(const QString &event, Handler handler);
    void unsubscribe(int id);
    void publish(const QString &event, const Payload &payload = {});

signals:
    void published(const QString &event, const QVariantMap &payload);

private:
    mutable QMutex m_mutex;
    int m_nextId = 1;
    std::unordered_map<int, std::pair<QString, Handler>> m_handlers;
};
