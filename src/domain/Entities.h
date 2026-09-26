#pragma once

// Entidades del dominio — réplica 1:1 de las columnas SQLite (ver
// sql/schema.sql). Sustituyen a los dicts de data/repository.py.
// Dinero en double (COP; magnitudes exactas en binario hasta 2^53).
#include <QList>
#include <QMap>
#include <QString>

struct Product
{
    int id = 0;
    QString sku;
    QString barcode;
    QString name;
    QString description;
    QString cat = QStringLiteral("General");
    QString subcat;
    QString brand;
    QString supplier;
    double price = 0.0;
    double priceBuy = 0.0;
    double priceWholesale = 0.0;
    QString tax = QStringLiteral("IVA 19%");
    QString unit = QStringLiteral("unidad");
    // Fase 2: cantidades decimales (granel). SQLite guarda REAL sin ALTER.
    double stock = 0.0;
    // Fase 3: apartados (reservado no disponible para vender).
    double reserved = 0.0;
    double stockMin = 5.0;
    double stockMax = 50.0;
    QString location;
    QString status = QStringLiteral("activo");
    QString image;
    QString lote;
    QString vencimiento;
    // Multitienda: rubro dueño del producto ('' = legacy/mixto, visible en todos).
    // Filtrar sin borrar: cambiar business_type oculta, nunca elimina.
    QString businessType;
    // Fase 3: metadatos por vertical (JSON objeto).
    QString attrsJson = QStringLiteral("{}");
    bool isKit = false;
    QString kitJson = QStringLiteral("[]");
    // Fase 3: disponible para vender (físico menos apartados).
    double available() const
    {
        return stock - reserved;
    }
};

struct KitComponent
{
    QString sku;
    double qty = 0.0;
};

struct Client
{
    int id = 0;
    QString name;
    QString nit;
    QString razon;
    QString regimen = QStringLiteral("No responsable IVA");
    QString responsabilidad;
    QString email;
    QString phone;
    QString address;
    QString city;
    double credit = 0.0;
    double creditLimit = 5000000.0;
    int discount = 0;
    double balance = 0.0;
    QString priceList = QStringLiteral("detal");
    QString status = QStringLiteral("activo");
};

struct Supplier
{
    int id = 0;
    QString name;
    QString nit;
    QString contact;
    QString phone;
    QString email;
    QString city;
    QString address;
    QString catalog;
    QString leadTime;
    QString paymentTerms;
    double balance = 0.0;
};

// Línea de venta (carrito y sale_items). qty decimal desde Fase 2 (granel).
struct SaleItem
{
    int productId = 0;
    double qty = 0.0;
    double subtotal = 0.0;
    // Fase 3: metadatos de línea (receta) + serial vendido.
    QString attrsJson = QStringLiteral("{}");
    QString serial;
};

struct Sale
{
    QString id;
    QString date;
    QString client;
    QString vendedor;
    double total = 0.0;
    double subtotal = 0.0;
    double tax = 0.0;
    // Fase 1: desglose por tasa (JSON); vacío en ventas históricas.
    QString taxBreakdown;
    // Multitienda: rubro de la venta ('' = mixta o legacy, visible en todos).
    QString businessType;
    double discount = 0.0;
    QString promo;
    QString status;
    QString docType;
    QString payment;
    QMap<QString, double> payments;
    double paid = 0.0;
    double balance = 0.0;
    QString due;
    QString estado;
    QString dianCufe;
    QString dianStatus;
    // Fase 5: trazabilidad documental (folio origen) + motivo (NC/ND/cancelación).
    QString parentId;
    QString reason;
};

struct PurchaseItem
{
    QString sku;
    double qty = 0.0;
    double priceBuy = 0.0;
};

struct Purchase
{
    QString id;
    QString date;
    QString supplier;
    double total = 0.0;
    QString status;
    QList<PurchaseItem> items;
    QString notes;
    // Fase 5: recepción parcial acumulada {sku: qty_recibida}.
    QMap<QString, double> received;
};

struct Promo
{
    int id = 0;
    QString name;
    QString type; // porcentaje|monto_fijo|2x1|3x2|volumen|cupon|happy_hour
    double value = 0.0;
    QString condition;
    QString code;
    bool active = true;
    QString desc;
    // Multitienda: rubro dueño ('' = todas las verticales).
    QString businessType;
    // Fase 3: vigencia AAAA-MM-DD ('' = sin límite).
    QString validFrom;
    QString validTo;
    // Fase 3: prioridad (mayor primero) y límite de usos (0 = ilimitada).
    int priority = 0;
    int maxUses = 0;
    int uses = 0;
};

struct CajaSale
{
    QString id;
    double total = 0.0;
};

struct CajaStatus
{
    bool open = false;
    double openingAmount = 0.0;
    QString openingTs;
    QString openingUser;
    QList<CajaSale> salesToday;
    double totalSales = 0.0;
    double expected = 0.0;
};

struct CajaCloseResult
{
    double expected = 0.0;
    double counted = 0.0;
    double diff = 0.0;
    int salesCount = 0;
    double totalSales = 0.0;
};

struct InventoryMovement
{
    int id = 0;
    QString ts;
    QString sku;
    QString product;
    QString type; // Entrada|Salida|Transferencia|Devolución
    double qty = 0.0;
    double before = 0.0;
    double after = 0.0;
    QString reason;
    QString user;
};

struct InventoryValue
{
    double costValue = 0.0;
    double saleValue = 0.0;
    double units = 0.0;
};

struct Payable
{
    QString id;
    QString supplier;
    QString due;
    double amount = 0.0;
    double paid = 0.0;
    double balance = 0.0;
    double discountEarly = 0.0;
    QString status;
};

struct CxcPayment
{
    int id = 0;
    QString saleId;
    QString date;
    double amount = 0.0;
    QString method;
    QString user;
};

// Fase 5: lote para valuación PEPS (un SKU, varios lotes con costo/vencimiento).
struct Lot
{
    int id = 0;
    QString sku;
    QString lote;
    QString vencimiento; // AAAA-MM-DD ('' = sin vencimiento, sale al final)
    double qty = 0.0;
    double cost = 0.0;
    QString createdTs;
};

// Fase 5: conteo cíclico (conteo → diferencia → ajuste justificado).
struct InventoryCount
{
    int id = 0;
    QString ts;
    QString sku;
    double expected = 0.0;
    double counted = 0.0;
    double diff = 0.0;
    QString reason;
    QString user;
    QString status; // Pendiente|Aplicado
};

struct AuditEntry
{
    int id = 0;
    QString ts;
    QString user;
    QString action;
    QString detail;
    // Fase 5: entidad afectada + antes/después (JSON) para responder
    // "¿quién cambió este precio y cuándo?".
    QString entity;
    QString entityId;
    QString beforeJson = QStringLiteral("{}");
    QString afterJson = QStringLiteral("{}");
};
