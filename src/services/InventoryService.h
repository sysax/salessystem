#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../core/Result.h"
#include "../domain/Entities.h"
#include "../repositories/InventoryRepository.h"
#include "../repositories/ProductRepository.h"

class EventBus;

// Compras de inventario (costo promedio ponderado), ajustes justificados,
// transferencias, alertas y valorización. Port de
// services/inventory_service.py + adjust/transfer_stock.
class InventoryService : public QObject
{
    Q_OBJECT

public:
    struct StockResult {
        QString sku;
        int newStock = 0;
        double newCost = 0.0;
    };
    struct Valuation {
        double totalValue = 0.0;
        int productsCount = 0;
    };

    explicit InventoryService(QSqlDatabase db, ProductRepository *products,
                              InventoryRepository *inventory, EventBus *bus = nullptr,
                              QObject *parent = nullptr);

    // Entrada por compra: costo promedio ponderado + movimiento "Entrada"
    Result<StockResult> registerPurchase(int productId, int qty, double cost,
                                         const QString &supplier, const QString &invoice,
                                         const QString &user);
    // Ajuste ±: motivo obligatorio, nunca stock negativo
    Result<StockResult> registerAdjustment(const QString &sku, int delta,
                                           const QString &reason, const QString &user);
    // Cambio de ubicación (sin mover unidades)
    StatusResult transfer(const QString &sku, int qty, const QString &toLocation,
                          const QString &reason, const QString &user);

    QList<Product> lowStock(double multiplier = 1.0) const;
    Valuation valuation() const;
    QList<InventoryMovement> movementsBySku(const QString &sku) const;

private:
    QSqlDatabase m_db;
    ProductRepository *m_products = nullptr;
    InventoryRepository *m_inventory = nullptr;
    EventBus *m_bus = nullptr;
};
