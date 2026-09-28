#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>

// Fase 8: métricas operativas mínimas persistidas en SQLite.
// Tabla app_metrics(ts, kind, ms, ok, sale_id, detail):
//   kind = sale|sync|caja — ms solo aplica a sale (tiempo de venta).
// El resumen semanal alimenta el reporte semanal (exportCsv) y el
// Dashboard. Todo best-effort: si el INSERT falla, se registra en
// qWarning y la operación de negocio sigue (nunca bloquear ventas).
class AppMetrics : public QObject
{
    Q_OBJECT
  public:
    explicit AppMetrics(QSqlDatabase db, QObject *parent = nullptr);

    static bool ensureTable(QSqlDatabase db);

    bool recordSale(qint64 elapsedMs, bool ok, const QString &saleId = {},
                    const QString &detail = {});
    bool recordSync(bool ok, const QString &detail = {});
    bool recordCaja(bool ok, const QString &detail = {});

    // Agregado últimos 7 días: {saleCount, saleAvgMs, saleP95Ms,
    // saleFail, syncFail, cajaFail, totalEvents, since}.
    Q_INVOKABLE QVariantMap weeklySummary() const;
    // Reporte semanal CSV en dir (retorna ruta o "" en error).
    Q_INVOKABLE QString exportCsv(const QString &dir) const;

  private:
    bool record(const QString &kind, qint64 ms, bool ok, const QString &saleId,
                const QString &detail);
    QSqlDatabase m_db;
};
