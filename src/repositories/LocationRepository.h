#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QSqlDatabase>
#include <QString>

#include <optional>

#include "../core/Result.h"
#include "AuditRepository.h"

// Fase 6: almacenes y ledger de existencias por ubicación.
//
// Modelo: `stock_by_location(sku, location_id, qty)` es el libro;
// `products.stock` es el agregado (suma) y se mantiene en cada escritura.
// Toda escritura de stock físico DEBE pasar por aquí con su ubicación
// (por defecto "Principal", id 1); los traspasos mueven unidades sin
// tocar el agregado. Transacciones anidables (SAVEPOINT) vía Transaction.
class LocationRepository : public QObject
{
    Q_OBJECT

  public:
    static constexpr int kPrincipalId = 1;

    struct Location
    {
        int id = 0;
        QString name;
    };

    explicit LocationRepository(QSqlDatabase db, AuditRepository *audit = nullptr,
                                QObject *parent = nullptr);

    QList<Location> locations() const;
    std::optional<Location> findLocation(int id) const;
    std::optional<Location> findByName(const QString &name) const;
    Result<Location> ensureLocation(const QString &name); // crea si no existe

    // Existencias.
    double stockAt(const QString &sku, int locationId) const;
    QMap<int, double> stockBySku(const QString &sku) const; // locationId → qty
    double totalFor(const QString &sku) const;

    // Escrituras (actualizan ledger + agregado products.stock).
    bool addStock(const QString &sku, int locationId, double qty); // suma (qty>0)
    bool takeStock(const QString &sku, int locationId, double qty); // resta si hay suficiente
    bool setStock(const QString &sku, int locationId, double qty);  // fija (ajusta agregado)
    // Traspaso real: resta en origen (si alcanza) + suma en destino.
    bool transferStock(const QString &sku, int fromId, int toId, double qty);

    // Primitivas ledger-only (sin tocar el agregado ni abrir tx): para
    // repos que ya escriben products.stock en su propia transacción
    // (SaleRepository::create, ProductRepository::update).
    bool writeLedger(const QString &sku, int locationId, double qty); // absoluto
    bool takeLedger(const QString &sku, int locationId, double qty);  // resta si alcanza

  private:
    bool bumpAggregate(const QString &sku, double delta);
    QSqlDatabase m_db;
    AuditRepository *m_audit = nullptr;
};
