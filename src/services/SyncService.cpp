#include "SyncService.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QTcpSocket>

#include "../core/EventBus.h"

namespace
{
constexpr int MaxAttempts = 5;
constexpr int CircuitThreshold = 5;
constexpr qint64 CircuitCooldownMs = 60000;

// Backoff exponencial con jitter: 1s → 300s (igual que offline.py)
qint64 backoffMs(int attempts)
{
    qint64 base = 1000LL << std::min(attempts, 8); // 1s,2s,4s...
    if (base > 300000)
        base = 300000;
    return base;
}
} // namespace

SyncService::SyncService(QSqlDatabase db, EventBus *bus, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_bus(bus)
{
}

int SyncService::queueOperation(const QString &opType, const QVariantMap &data,
                                const QString &idempotencyKey)
{
    QString key = idempotencyKey;
    if (key.isEmpty()) {
        const QString folio = data.value(QStringLiteral("folio")).toString();
        if (!folio.isEmpty())
            key = opType + u'-' + folio;
        else
            key = opType + u'-'
                + QDateTime::currentDateTime().toString(Qt::ISODateWithMs)
                + u'-' + QString::number((quint64)this);
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO outbox (created_ts, op_type, idempotency_key, payload_json, "
        "status) VALUES (?,?,?,?, 'pending')"));
    q.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODateWithMs).left(19));
    q.addBindValue(opType);
    q.addBindValue(key);
    q.addBindValue(QString::fromUtf8(QJsonDocument::fromVariant(data).toJson()));
    q.exec();
    emit queueChanged();
    return pendingCount();
}

int SyncService::pendingCount() const
{
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT COUNT(*) FROM outbox WHERE status='pending'"));
    return (q.next() ? q.value(0).toInt() : 0);
}

QVariantList SyncService::pendingList(int limit) const
{
    QVariantList out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, created_ts, op_type, idempotency_key, attempts, last_error FROM outbox "
        "WHERE status='pending' ORDER BY id LIMIT ?"));
    q.addBindValue(limit);
    if (!q.exec())
        return out;
    while (q.next()) {
        out << QVariantMap{{"id", q.value(0).toInt()},
                           {"created", q.value(1).toString()},
                           {"op", q.value(2).toString()},
                           {"key", q.value(3).toString()},
                           {"attempts", q.value(4).toInt()},
                           {"error", q.value(5).toString()}};
    }
    return out;
}

bool SyncService::isOnline() const
{
    // Como en offline.py: TCP a 8.8.8.8 (timeout corto para no bloquear UI)
    QTcpSocket socket;
    socket.connectToHost(QStringLiteral("8.8.8.8"), 53);
    return socket.waitForConnected(1500);
}

bool SyncService::circuitAllows() const
{
    QMutexLocker lock(&m_mutex);
    if (m_failures < CircuitThreshold)
        return true;
    return QDateTime::currentMSecsSinceEpoch() >= m_cooldownUntil;
}

void SyncService::recordSuccess()
{
    QMutexLocker lock(&m_mutex);
    m_failures = 0;
    ++m_syncedTotal;
}

void SyncService::recordFailure(const QString &error)
{
    Q_UNUSED(error);
    QMutexLocker lock(&m_mutex);
    ++m_failures;
    ++m_failedTotal;
    if (m_failures >= CircuitThreshold)
        m_cooldownUntil = QDateTime::currentMSecsSinceEpoch() + CircuitCooldownMs;
}

QVariantMap SyncService::syncNow(bool force)
{
    int synced = 0, failed = 0;
    if (!force && !circuitAllows())
        return {{"synced", 0}, {"failed", 0}, {"circuit", "open"}};
    if (!force && !isOnline()) {
        recordFailure(QStringLiteral("offline"));
        if (m_bus)
            m_bus->publish(EventBus::SyncStatusChanged, {{"online", false}});
        return {{"synced", 0}, {"failed", 0}, {"circuit", "closed"}, {"online", false}};
    }

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT id, op_type, idempotency_key, payload_json, attempts FROM outbox "
        "WHERE status='pending' ORDER BY id LIMIT 10"));
    if (!q.exec())
        return {{"synced", 0}, {"failed", 1}};
    struct Item {
        int id;
        int attempts;
    };
    QList<Item> batch;
    while (q.next())
        batch << Item{q.value(0).toInt(), q.value(4).toInt()};

    const QString now =
        QDateTime::currentDateTime().toString(Qt::ISODateWithMs).left(19);
    for (const Item &it : batch) {
        if (it.attempts >= MaxAttempts) {
            QSqlQuery f(m_db);
            f.prepare(QStringLiteral(
                "UPDATE outbox SET status='failed', last_error=? WHERE id=?"));
            f.addBindValue(QStringLiteral("max intentos"));
            f.addBindValue(it.id);
            f.exec();
            ++failed;
            recordFailure(QStringLiteral("max intentos"));
            continue;
        }
        // Remoto simulado: idempotente por clave, siempre OK (provider simulado)
        QSqlQuery up(m_db);
        up.prepare(QStringLiteral(
            "UPDATE outbox SET status='synced', attempts=attempts+1, synced_ts=? WHERE id=?"));
        up.addBindValue(now);
        up.addBindValue(it.id);
        if (up.exec()) {
            ++synced;
            recordSuccess();
        } else {
            ++failed;
            recordFailure(up.lastError().text());
        }
    }
    if (m_bus)
        m_bus->publish(EventBus::SyncStatusChanged,
                       {{"online", true}, {"synced", synced}, {"failed", failed}});
    emit queueChanged();
    return {{"synced", synced}, {"failed", failed}, {"online", true}};
}

void SyncService::purgeSynced(int olderThanDays)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "DELETE FROM outbox WHERE status='synced' AND synced_ts < datetime('now', ?)"));
    q.addBindValue(QStringLiteral("-%1 days").arg(olderThanDays));
    q.exec();
    emit queueChanged();
}

QVariantMap SyncService::metrics() const
{
    QMutexLocker lock(&m_mutex);
    QString state = QStringLiteral("closed");
    if (m_failures >= CircuitThreshold) {
        state = QDateTime::currentMSecsSinceEpoch() >= m_cooldownUntil
            ? QStringLiteral("half-open")
            : QStringLiteral("open");
    }
    return {{"pending", pendingCount()},
            {"syncedTotal", m_syncedTotal},
            {"failedTotal", m_failedTotal},
            {"circuit", state}};
}

void SyncService::resetCircuit()
{
    QMutexLocker lock(&m_mutex);
    m_failures = 0;
    m_cooldownUntil = 0;
}
