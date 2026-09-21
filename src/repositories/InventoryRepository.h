#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../domain/Entities.h"
#include "AuditRepository.h"

// Movimientos y consultas de inventario. Solo primitives de escritura
// (record/setStock vía ProductRepository); las reglas (justificación,
// no-negativo, costos) viven en InventoryService.
class InventoryRepository : public QObject
{
    Q_OBJECT

public:
    explicit InventoryRepository(QSqlDatabase db, AuditRepository *audit = nullptr,
                                 QObject *parent = nullptr);

    // Registra movimiento con before/after explícitos (no toca stock)
    bool record(const QString &sku, const QString &productName, const QString &type, int qty,
                int before, int after, const QString &reason, const QString &user);

    QList<InventoryMovement> movements(int limit = 20) const;
    QList<InventoryMovement> movementsBySku(const QString &sku) const;

    InventoryValue value() const;
    QList<Product> lowStock(int threshold) const;
    QList<Product> belowMin() const;
    QList<Product> aboveMax() const;
    QList<Product> outOfStock() const;

    static InventoryMovement rowToMovement(const QSqlQuery &q);

private:
    QSqlDatabase m_db;
    AuditRepository *m_audit = nullptr;
};
