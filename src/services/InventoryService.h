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
    struct StockResult
    {
        QString sku;
        double newStock = 0.0;
        double newCost = 0.0;
    };
    struct Valuation
    {
        double totalValue = 0.0;
        int productsCount = 0;
    };

    explicit InventoryService(QSqlDatabase db, ProductRepository *products,
                              InventoryRepository *inventory, EventBus *bus = nullptr,
                              QObject *parent = nullptr);

    // Entrada por compra: costo promedio ponderado + movimiento "Entrada"
    // (Fase 5: también abre lote PEPS con el costo de la entrada).
    Result<StockResult> registerPurchase(int productId, double qty, double cost,
                                         const QString &supplier, const QString &invoice,
                                         const QString &user, const QString &lote = {},
                                         const QString &vencimiento = {});
    // Ajuste ±: motivo obligatorio, nunca stock negativo
    Result<StockResult> registerAdjustment(const QString &sku, double delta, const QString &reason,
                                           const QString &user);
    // Fase 4: merma (salida tipo "Merma" para el reporte de desperdicio).
    Result<StockResult> registerWaste(const QString &sku, double qty, const QString &reason,
                                      const QString &user);
    // Cambio de ubicación (sin mover unidades)
    StatusResult transfer(const QString &sku, double qty, const QString &toLocation,
                          const QString &reason, const QString &user);
    // Fase 3: apartar/liberar stock (no tocan el físico; el POS vende
    // contra disponible = físico − reservado).
    struct ReserveResult
    {
        QString sku;
        double reserved = 0.0;
        double available = 0.0;
    };
    Result<ReserveResult> reserveStock(const QString &sku, double qty, const QString &reason,
                                       const QString &user);
    Result<ReserveResult> releaseStock(const QString &sku, double qty, const QString &user);

    QList<Product> lowStock(double multiplier = 1.0) const;
    Valuation valuation() const;
    // Fase 5: valuación por método ("promedio" o "peps" por lotes).
    Valuation valuation(const QString &method) const;
    QList<InventoryMovement> movementsBySku(const QString &sku) const;
    // Fase 5: salida PEPS (consume lotes por vencimiento; devuelve costo
    // de lo consumido). No toca products.stock (lo hace el llamador).
    Result<double> consumeFifo(const QString &sku, double qty);
    // Fase 5: conteos cíclicos (conteo → diferencia → ajuste justificado).
    Result<InventoryCount> startCount(const QString &sku, double counted, const QString &reason,
                                     const QString &user);
    Result<InventoryCount> applyCount(int countId, const QString &user);
    QList<InventoryCount> listCounts(const QString &status = {}) const;
    QList<Lot> expiringLots(int days) const;

  private:
    QSqlDatabase m_db;
    ProductRepository *m_products = nullptr;
    InventoryRepository *m_inventory = nullptr;
    EventBus *m_bus = nullptr;
};
