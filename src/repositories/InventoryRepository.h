#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../domain/Entities.h"
#include "../core/Result.h"
#include "AuditRepository.h"

#include <optional>

// Movimientos y consultas de inventario. Solo primitives de escritura
// (record/setStock vía ProductRepository); las reglas (justificación,
// no-negativo, costos) viven en InventoryService.
class InventoryRepository : public QObject
{
    Q_OBJECT

  public:
    explicit InventoryRepository(QSqlDatabase db, AuditRepository *audit = nullptr,
                                 QObject *parent = nullptr);

    // Registra movimiento con before/after explícitos (no toca stock).
    // Fase 2: cantidades decimales. Fase 6: origen/destino del traspaso.
    bool record(const QString &sku, const QString &productName, const QString &type, double qty,
                double before, double after, const QString &reason, const QString &user,
                const QString &fromLocation = {}, const QString &toLocation = {});

    QList<InventoryMovement> movements(int limit = 20) const;
    QList<InventoryMovement> movementsBySku(const QString &sku) const;

    InventoryValue value() const;
    QList<Product> lowStock(int threshold) const;
    // Multitienda: bt vacío o 'miscelanea' = todos los rubros.
    QList<Product> belowMin(const QString &businessType = {}) const;
    QList<Product> aboveMax() const;
    QList<Product> outOfStock() const;

    // Fase 5: lotes PEPS (un SKU, varios lotes con costo/vencimiento).
    Result<Lot> addLot(const QString &sku, const QString &lote, const QString &vencimiento,
                       double qty, Money cost);
    QList<Lot> lotsBySku(const QString &sku) const; // qty>0, PEPS: vencimiento ASC ('' al final)
    bool reduceLot(int lotId, double qty);
    Money lotsValue(const QString &sku = {}) const; // valuación PEPS (suma lotes)
    QList<Lot> expiringLots(int days) const;

    // Fase 5: conteos cíclicos (conteo → diferencia → ajuste justificado).
    Result<InventoryCount> startCount(const QString &sku, double expected, double counted,
                                     const QString &reason, const QString &user);
    QList<InventoryCount> listCounts(const QString &status = {}) const;
    std::optional<InventoryCount> findCount(int id) const;
    bool markCountApplied(int id);

    static InventoryMovement rowToMovement(const QSqlQuery &q);
    static Lot rowToLot(const QSqlQuery &q);
    static InventoryCount rowToCount(const QSqlQuery &q);

  private:
    QSqlDatabase m_db;
    AuditRepository *m_audit = nullptr;
};
