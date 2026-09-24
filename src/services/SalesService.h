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
#include "../repositories/SerialRepository.h"
#include "../repositories/AuditRepository.h"
#include "../services/SettingsService.h"

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
        double qty = 1.0;
        double priceOverride = 0.0; // 0 ⇒ precio lista
        double discountPct = 0.0;
        QString serial; // Fase 3: IMEI/serial (productos tracked)
        QString receta; // Fase 3: Nº receta (requires_prescription)
    };
    struct LineTotal {
        int productId = 0;
        QString name;
        QString sku;
        double qty = 0.0;
        double unitPrice = 0.0;
        double subtotal = 0.0;
        double discount = 0.0;
        double tax = 0.0;
        double total = 0.0;
        // Fase 1: tasa efectivamente aplicada (validada contra settings).
        double taxRate = 0.0;
        QString taxName;
        // Fase 3: serial/receta de la línea.
        QString serial;
        QString receta;
    };
    struct TaxBucket {
        QString name;
        double rate = 0.0;
        double base = 0.0; // Σ(subtotal − descuento) de sus líneas
        double tax = 0.0;  // Σ impuesto de sus líneas (agregado, no recalculado)
    };
    struct Totals {
        double subtotal = 0.0;
        double discount = 0.0;
        double tax = 0.0;
        double total = 0.0;
        int itemsCount = 0;
        QList<LineTotal> lines;
        QList<TaxBucket> buckets; // desglose por tasa
    };
    struct CreatedSale {
        QString id;
        double total = 0.0;
        QString cufe;
        QString status;
        QList<LineTotal> items;
        QList<TaxBucket> buckets;
    };

    explicit SalesService(QSqlDatabase db, ProductRepository *products, SaleRepository *sales,
                          InventoryRepository *inventory, ClientRepository *clients,
                          CajaRepository *caja, PromoRepository *promos, EventBus *bus = nullptr,
                          SettingsService *settings = nullptr,
                          AuditRepository *audit = nullptr,
                          SerialRepository *serials = nullptr, QObject *parent = nullptr);

    // Crea venta completa. payments: {"efectivo": X, "credito": Y} o vacío +
    // paymentMethod ("Efectivo"|"Credito"|...). promoCode opcional.
    // role: rol del vendedor (productos controlled exigen Administrador).
    Result<CreatedSale> create(const QList<ServiceItem> &items, const QString &clientName,
                               const QMap<QString, double> &payments,
                               const QString &paymentMethod, const QString &promoCode,
                               const QString &vendedor, bool offline = false,
                               const QString &role = {});
    Result<CreatedSale> cancel(const QString &saleId, const QString &reason,
                               const QString &user);
    Totals calculateTotals(const QList<ServiceItem> &items) const;

    // Impuesto desde texto BD ("IVA 19%"→19, "19"→19, otro→0). Legacy: se
    // conserva para compatibilidad; el cálculo usa resolveTaxRate().
    static double parseTaxRate(const QString &taxText);
    // Fase 1: valida contra tax_rates_json; si no está configurada, usa
    // default_tax_rate. Sin settings (tests viejos) = parseTaxRate().
    double resolveTaxRate(const QString &taxText) const;
    QString resolveTaxName(const QString &taxText) const;
    // Serializa buckets a JSON para sales.tax_breakdown.
    static QString bucketsToJson(const QList<TaxBucket> &buckets);
    static QList<TaxBucket> bucketsFromJson(const QString &json);

private:
    Result<Totals> buildTotals(const QList<ServiceItem> &items, QString &error,
                               const QString &clientName = {}) const;
    // Fase 4: precio según lista del cliente (mayorista → price_wholesale).
    double priceFor(const Product &p, double priceOverride,
                    const QString &clientName) const;
    // Fase 3: producto con seguimiento de serial (flag attrs o seriales registrados).
    bool isTracked(const Product &p) const;

    QSqlDatabase m_db;
    ProductRepository *m_products = nullptr;
    SaleRepository *m_sales = nullptr;
    InventoryRepository *m_inventory = nullptr;
    ClientRepository *m_clients = nullptr;
    CajaRepository *m_caja = nullptr;
    PromoRepository *m_promos = nullptr;
    EventBus *m_bus = nullptr;
    SettingsService *m_settings = nullptr;
    AuditRepository *m_audit = nullptr;
    SerialRepository *m_serials = nullptr;
};
