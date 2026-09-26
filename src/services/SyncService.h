#pragma once

#include <QMutex>
#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QVariantMap>

#include "../core/Result.h"

class EventBus;

// Cola persistente offline-first (outbox): encola operaciones idempotentes
// y las sincroniza al reconectar. Port esencial de data/offline.py: cola +
// backoff + circuit breaker + métricas. Sin backend real, el "remoto" es
// simulado (marca SINCRONIZADO, como el provider simulado DIAN).
class SyncService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int pendingCount READ pendingCount NOTIFY queueChanged)

  public:
    struct Metrics
    {
        int pending = 0;
        int syncedTotal = 0;
        int failedTotal = 0;
        QString circuitState = QStringLiteral("closed");
    };

    explicit SyncService(QSqlDatabase db, EventBus *bus = nullptr, QObject *parent = nullptr);

    // Encola op (idempotente por clave); retorna pendientes.
    Q_INVOKABLE int queueOperation(const QString &opType, const QVariantMap &data,
                                   const QString &idempotencyKey = {});
    Q_INVOKABLE int pendingCount() const;
    Q_INVOKABLE QVariantList pendingList(int limit = 50) const;

    // Sincroniza lote: si online (o force), marca synced; si no, reintento
    // con backoff. Retorna {synced, failed}.
    Q_INVOKABLE QVariantMap syncNow(bool force = false);

    Q_INVOKABLE void purgeSynced(int olderThanDays = 7);
    Q_INVOKABLE QVariantMap metrics() const;
    Q_INVOKABLE bool isOnline() const;
    Q_INVOKABLE void resetCircuit();

    // Fase 6: identidad estable del dispositivo (persistida en settings;
    // QUuid la primera vez). Los eventos llevan device_id + seq (= rowid,
    // secuencia por dispositivo de un solo escritor).
    QString deviceId();
    // Fase 6: estrategia de resolución por tipo de op (el servidor la
    // aplicará; sin backend real se documenta y testea el mapeo).
    // "lww" = last-writer-wins (catálogo); "additive" = solo-agrega,
    // nunca se sobrescribe (movimientos: ventas, pagos, inventario).
    static QString conflictStrategy(const QString &opType);

  signals:
    void queueChanged();

  private:
    bool circuitAllows() const;
    void recordSuccess();
    void recordFailure(const QString &error);
    void setSetting(const QString &key, const QString &value);
    QString setting(const QString &key) const;

    QSqlDatabase m_db;
    EventBus *m_bus = nullptr;
    mutable QMutex m_mutex;
    int m_failures = 0;
    qint64 m_cooldownUntil = 0; // ms epoch
    int m_syncedTotal = 0;
    int m_failedTotal = 0;
};
