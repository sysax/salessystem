#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../core/Result.h"
#include "../domain/Entities.h"
#include "AuditRepository.h"

// Catálogo (products) — CRUD + búsqueda + kits + vencimientos.
// Validaciones idénticas a Repository.add/update_product (mensajes en ES).
class ProductRepository : public QObject
{
    Q_OBJECT

public:
    explicit ProductRepository(QSqlDatabase db, AuditRepository *audit = nullptr,
                               QObject *parent = nullptr);

    QList<Product> list() const;
    std::optional<Product> findById(int id) const;
    std::optional<Product> findBySku(const QString &sku) const;
    std::optional<Product> findByBarcode(const QString &barcode) const;
    QList<Product> search(const QString &text) const;

    Result<Product> add(const Product &p);
    // Reemplazo de campos editables (los vacíos/nulos no se tocan,
    // igual que update_product con updates dict).
    Result<Product> update(const QString &sku, const Product &p);
    StatusResult remove(const QString &sku);

    // Primitivas de stock para los servicios (no validan negocio)
    bool setStockById(int id, int stock);
    bool setStockBySku(const QString &sku, int stock);

    // Kits: stock virtual = mín(floor(stock/qty) componentes)
    QList<Product> kits() const;
    Result<Product> createKit(const QString &sku, const QString &name,
                              const QList<KitComponent> &components,
                              double priceOverride, const QString &user);
    QList<KitComponent> kitComponents(const QString &sku) const;
    int kitStock(const QList<KitComponent> &components) const;

    QList<Product> expiringWithin(int days) const;

    static QString generateBarcode(const QString &sku);
    static Product rowToProduct(const QSqlQuery &q);

private:
    QSqlDatabase m_db;
    AuditRepository *m_audit = nullptr;
};
