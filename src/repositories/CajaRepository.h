#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../core/Result.h"
#include "../domain/Entities.h"
#include "AuditRepository.h"

// Caja (turno único id=1): apertura/cierre/estado + registro de ventas.
// Semántica idéntica a open/close/get_caja_status.
class CajaRepository : public QObject
{
    Q_OBJECT

  public:
    explicit CajaRepository(QSqlDatabase db, AuditRepository *audit = nullptr,
                            QObject *parent = nullptr);

    CajaStatus status() const;
    Result<CajaStatus> open(double amount, const QString &user);
    // Fase 5: con diferencia (sobra/falta) el motivo es obligatorio.
    Result<CajaCloseResult> close(double counted, const QString &user, const QString &reason = {});

    // Agrega {id,total} a sales_today_json y recalcula expected (solo si abierta)
    bool recordSale(const QString &saleId, double total);
    // Fase 1: retira {saleId} del JSON y recalcula expected (cancelación).
    // Si la caja está cerrada o no contiene la venta, no-op (true).
    bool reverseSale(const QString &saleId);

    // Fase 3: movimiento normalizado (tipo: apertura|venta|devolucion|cierre).
    // Sin transacción propia: se suma a la del llamador (atómico con la
    // operación que lo genera). ts/turno automáticos del turno actual.
    struct Movement
    {
        int id = 0;
        QString ts;
        QString turno;
        QString type;
        double amount = 0.0;
        QString method;
        QString saleId;
        QString user;
    };
    bool logMovement(const QString &type, double amount, const QString &method,
                     const QString &saleId, const QString &user);
    // Movimientos del turno actual (turno vacío = todos). El JSON de caja
    // queda como caché de presentación.
    QList<Movement> movements(const QString &turno = {}) const;
    QString currentTurno() const;
    // Esperado calculado desde movimientos (cuadra por construcción con
    // status().expected; el test lo verifica).
    double expectedFromMovements() const;

  private:
    QSqlDatabase m_db;
    AuditRepository *m_audit = nullptr;
};
