#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../core/Result.h"
#include "../domain/Entities.h"
#include "../repositories/AuditRepository.h"
#include "../repositories/InventoryRepository.h"
#include "../repositories/CreditRepository.h"
#include "../repositories/ProductRepository.h"
#include "../repositories/PurchaseRepository.h"
#include "../repositories/ClientRepository.h" // Client + Supplier

// Órdenes de compra: crear → recibir (stock + CxP automática) → cancelar.
// Port de create/receive/cancel_purchase.
class PurchaseService : public QObject
{
    Q_OBJECT

public:
    explicit PurchaseService(QSqlDatabase db, PurchaseRepository *purchases,
                             ProductRepository *products, SupplierRepository *suppliers,
                             InventoryRepository *inventory, PayablesRepository *payables,
                             AuditRepository *audit = nullptr, QObject *parent = nullptr);

    Result<Purchase> create(const QString &supplierName, const QString &sku, double qty,
                            const QString &user);
    Result<Purchase> receive(const QString &folio, const QString &user);
    Result<Purchase> cancel(const QString &folio, const QString &user);

private:
    QSqlDatabase m_db;
    PurchaseRepository *m_purchases = nullptr;
    ProductRepository *m_products = nullptr;
    SupplierRepository *m_suppliers = nullptr;
    InventoryRepository *m_inventory = nullptr;
    PayablesRepository *m_payables = nullptr;
    AuditRepository *m_audit = nullptr;
};
