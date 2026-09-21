#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../core/Result.h"
#include "../domain/Entities.h"
#include "../repositories/CajaRepository.h"
#include "../repositories/ClientRepository.h"
#include "../repositories/InventoryRepository.h"
#include "../repositories/ProductRepository.h"
#include "../repositories/PromoRepository.h"
#include "../repositories/SaleRepository.h"

class EventBus;

// Orquesta creación/cancelación de ventas: valida stock, calcula totales
// (IVA por línea desde texto "IVA 19 %"), aplica promos, descuenta
// inventario con movimientos, publica eventos. Port de
// services/sales_service.py + Repository.create_sale.
//
// Desviación documentada: el Python hacía Decimal(product['tax']) donde
// tax es texto ("IVA 19%") → crash; aquí parseTaxRate() lo interpreta.
class SalesService : public QObject
{
    Q_OBJECT

public:
    struct ServiceItem {
        int productId = 0;
        int qty = 1;
        double priceOverride = 0.0; // 0 ⇒ precio lista
        double discountPct = 0.0;
    };
    struct LineTotal {
        int productId = 0;
        QString name;
        QString sku;
        int qty = 0;
        double unitPrice = 0.0;
        double subtotal = 0.0;
        double discount = 0.0;
        double tax = 0.0;
        double total = 0.0;
    };
    struct Totals {
        double subtotal = 0.0;
        double discount = 0.0;
        double tax = 0.0;
        double total = 0.0;
        int itemsCount = 0;
        QList<LineTotal> lines;
    };
    struct CreatedSale {
        QString id;
        double total = 0.0;
        QString cufe;
        QString status;
        QList<LineTotal> items;
    };

    explicit SalesService(QSqlDatabase db, ProductRepository *products, SaleRepository *sales,
                          InventoryRepository *inventory, ClientRepository *clients,
                          CajaRepository *caja, PromoRepository *promos, EventBus *bus = nullptr,
                          QObject *parent = nullptr);

    // Crea venta completa. payments: {"efectivo": X, "credito": Y} o vacío +
    // paymentMethod ("Efectivo"|"Credito"|...). promoCode opcional.
    Result<CreatedSale> create(const QList<ServiceItem> &items, const QString &clientName,
                               const QMap<QString, double> &payments,
                               const QString &paymentMethod, const QString &promoCode,
                               const QString &vendedor, bool offline = false);
    Result<CreatedSale> cancel(const QString &saleId, const QString &reason,
                               const QString &user);
    Totals calculateTotals(const QList<ServiceItem> &items) const;

    // Impuesto desde texto BD ("IVA 19%"→19, "19"→19, otro→0)
    static double parseTaxRate(const QString &taxText);

private:
    Result<Totals> buildTotals(const QList<ServiceItem> &items, QString &error) const;

    QSqlDatabase m_db;
    ProductRepository *m_products = nullptr;
    SaleRepository *m_sales = nullptr;
    InventoryRepository *m_inventory = nullptr;
    ClientRepository *m_clients = nullptr;
    CajaRepository *m_caja = nullptr;
    PromoRepository *m_promos = nullptr;
    EventBus *m_bus = nullptr;
};
