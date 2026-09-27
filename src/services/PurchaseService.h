#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../core/Result.h"
#include "../domain/Entities.h"
#include "../repositories/AuditRepository.h"
#include "../repositories/InventoryRepository.h"
#include "../repositories/CreditRepository.h"
#include "../repositories/LocationRepository.h"
#include "../repositories/ProductRepository.h"
#include "../repositories/PurchaseRepository.h"
#include "../repositories/ClientRepository.h" // Client + Supplier

class SettingsService;

// Órdenes de compra: crear → recibir (stock + CxP automática) → cancelar.
// Port de create/receive/cancel_purchase.
class PurchaseService : public QObject
{
    Q_OBJECT

  public:
    explicit PurchaseService(QSqlDatabase db, PurchaseRepository *purchases,
                             ProductRepository *products, SupplierRepository *suppliers,
                             InventoryRepository *inventory, PayablesRepository *payables,
                             AuditRepository *audit = nullptr, QObject *parent = nullptr,
                             SettingsService *settings = nullptr);

    Result<Purchase> create(const QString &supplierName, const QString &sku, double qty,
                             const QString &user);
    Result<Purchase> receive(const QString &folio, const QString &user,
                             int locationId = LocationRepository::kPrincipalId);
    // Fase 5: recepción parcial (sku → qty de ESTA entrega; se acumula en
    // received_json; N entregas hasta completar; estado Parcial/Recibida).
    // Fase 6: la entrega entra al almacén indicado (default Principal).
    Result<Purchase> receive(const QString &folio, const QMap<QString, double> &delivery,
                             const QString &user,
                             int locationId = LocationRepository::kPrincipalId);
    Result<Purchase> cancel(const QString &folio, const QString &user);

  private:
    QSqlDatabase m_db;
    PurchaseRepository *m_purchases = nullptr;
    ProductRepository *m_products = nullptr;
    SupplierRepository *m_suppliers = nullptr;
    InventoryRepository *m_inventory = nullptr;
    PayablesRepository *m_payables = nullptr;
    AuditRepository *m_audit = nullptr;
    // Fase 3: settings opcionales (días de pago a proveedores).
    SettingsService *m_settings = nullptr;
};
