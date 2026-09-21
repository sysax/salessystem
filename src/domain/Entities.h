#pragma once

// Entidades del dominio — réplica 1:1 de las columnas SQLite (ver
// sql/schema.sql). Sustituyen a los dicts de data/repository.py.
// Dinero en double (COP; magnitudes exactas en binario hasta 2^53).
#include <QList>
#include <QMap>
#include <QString>

struct Product {
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
    int stock = 0;
    int stockMin = 5;
    int stockMax = 50;
    QString location;
    QString status = QStringLiteral("activo");
    QString image;
    QString lote;
    QString vencimiento;
    bool isKit = false;
    QString kitJson = QStringLiteral("[]");
};

struct KitComponent {
    QString sku;
    int qty = 0;
};

struct Client {
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

struct Supplier {
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

// Línea de venta (carrito y sale_items)
struct SaleItem {
    int productId = 0;
    int qty = 0;
    double subtotal = 0.0;
};

struct Sale {
    QString id;
    QString date;
    QString client;
    QString vendedor;
    double total = 0.0;
    double subtotal = 0.0;
    double tax = 0.0;
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
};

struct PurchaseItem {
    QString sku;
    int qty = 0;
    double priceBuy = 0.0;
};

struct Purchase {
    QString id;
    QString date;
    QString supplier;
    double total = 0.0;
    QString status;
    QList<PurchaseItem> items;
    QString notes;
};

struct Promo {
    int id = 0;
    QString name;
    QString type; // porcentaje|monto_fijo|2x1|3x2|volumen|cupon|happy_hour
    double value = 0.0;
    QString condition;
    QString code;
    bool active = true;
    QString desc;
};

struct CajaSale {
    QString id;
    double total = 0.0;
};

struct CajaStatus {
    bool open = false;
    double openingAmount = 0.0;
    QString openingTs;
    QString openingUser;
    QList<CajaSale> salesToday;
    double totalSales = 0.0;
    double expected = 0.0;
};

struct CajaCloseResult {
    double expected = 0.0;
    double counted = 0.0;
    double diff = 0.0;
    int salesCount = 0;
    double totalSales = 0.0;
};

struct InventoryMovement {
    int id = 0;
    QString ts;
    QString sku;
    QString product;
    QString type; // Entrada|Salida|Transferencia|Devolución
    int qty = 0;
    int before = 0;
    int after = 0;
    QString reason;
    QString user;
};

struct InventoryValue {
    double costValue = 0.0;
    double saleValue = 0.0;
    long long units = 0;
};

struct Payable {
    QString id;
    QString supplier;
    QString due;
    double amount = 0.0;
    double paid = 0.0;
    double balance = 0.0;
    double discountEarly = 0.0;
    QString status;
};

struct CxcPayment {
    int id = 0;
    QString saleId;
    QString date;
    double amount = 0.0;
    QString method;
    QString user;
};

struct AuditEntry {
    int id = 0;
    QString ts;
    QString user;
    QString action;
    QString detail;
};
