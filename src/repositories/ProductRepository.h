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
    QList<Product> list(const QString &businessType) const;
    std::optional<Product> findById(int id) const;
    std::optional<Product> findBySku(const QString &sku) const;
    std::optional<Product> findByBarcode(const QString &barcode) const;
    QList<Product> search(const QString &text) const;
    // Multitienda (filtrar sin borrar): bt vacío o 'miscelanea' = todo;
    // si no, (business_type='' OR business_type=bt). '' = legacy visible en todos.
    QList<Product> search(const QString &text, const QString &businessType) const;
    // Fase 4: paginación servidor (limit < 0 = sin límite) + total.
    // sortKey con whitelist (id|sku|name|price|stock) para orden servidor.
    QList<Product> searchPaged(const QString &text, const QString &businessType, int limit,
                               int offset) const;
    QList<Product> searchPaged(const QString &text, const QString &businessType, int limit,
                               int offset, const QString &sortKey, bool sortAsc) const;
    int countSearch(const QString &text, const QString &businessType = {}) const;

    Result<Product> add(const Product &p);
    // Reemplazo de campos editables (los vacíos/nulos no se tocan,
    // igual que update_product con updates dict).
    Result<Product> update(const QString &sku, const Product &p);
    StatusResult remove(const QString &sku);

    // Primitivas de stock para los servicios (no validan negocio).
    // Fase 2: double (granel).
    bool setStockById(int id, double stock);
    bool setStockBySku(const QString &sku, double stock);
    // Fase 3: apartados (no tocan el físico). reserve falla sin disponible;
    // release topa en 0. Deben llamarse dentro de una Transaction.
    bool reserveAtomic(int id, double qty);
    bool releaseAtomic(int id, double qty);

    // Kits: stock virtual = mín(floor(stock/qty) componentes)
    QList<Product> kits() const;
    Result<Product> createKit(const QString &sku, const QString &name,
                              const QList<KitComponent> &components, double priceOverride,
                              const QString &user);
    QList<KitComponent> kitComponents(const QString &sku) const;
    double kitStock(const QList<KitComponent> &components) const;

    QList<Product> expiringWithin(int days) const;
    // Multitienda: bt vacío o 'miscelanea' = todos los rubros.
    QList<Product> expiringWithin(int days, const QString &businessType) const;

    // Fase 2: unidades canónicas. Vacío se normaliza a "unidad" en add/update.
    static const QStringList Units;
    static bool isWeighable(const QString &unit);
    static QString normalizeUnit(const QString &unit);
    // Fase 3: EAN-13 con dígito verificador válido.
    static bool isValidEan13(const QString &barcode);

    static QString generateBarcode(const QString &sku);
    static Product rowToProduct(const QSqlQuery &q);

  private:
    QSqlDatabase m_db;
    AuditRepository *m_audit = nullptr;
};
