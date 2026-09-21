#include "ProductRepository.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

#include <limits>

ProductRepository::ProductRepository(QSqlDatabase db, AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_audit(audit)
{
}

Product ProductRepository::rowToProduct(const QSqlQuery &q)
{
    Product p;
    p.id = q.value(QStringLiteral("id")).toInt();
    p.sku = q.value(QStringLiteral("sku")).toString();
    p.barcode = q.value(QStringLiteral("barcode")).toString();
    p.name = q.value(QStringLiteral("name")).toString();
    p.description = q.value(QStringLiteral("description")).toString();
    p.cat = q.value(QStringLiteral("cat")).toString();
    p.subcat = q.value(QStringLiteral("subcat")).toString();
    p.brand = q.value(QStringLiteral("brand")).toString();
    p.supplier = q.value(QStringLiteral("supplier")).toString();
    p.price = q.value(QStringLiteral("price")).toDouble();
    p.priceBuy = q.value(QStringLiteral("price_buy")).toDouble();
    p.priceWholesale = q.value(QStringLiteral("price_wholesale")).toDouble();
    p.tax = q.value(QStringLiteral("tax")).toString();
    p.unit = q.value(QStringLiteral("unit")).toString();
    p.stock = q.value(QStringLiteral("stock")).toInt();
    p.stockMin = q.value(QStringLiteral("stock_min")).toInt();
    p.stockMax = q.value(QStringLiteral("stock_max")).toInt();
    p.location = q.value(QStringLiteral("location")).toString();
    p.status = q.value(QStringLiteral("status")).toString();
    p.image = q.value(QStringLiteral("image")).toString();
    p.lote = q.value(QStringLiteral("lote")).toString();
    p.vencimiento = q.value(QStringLiteral("vencimiento")).toString();
    p.isKit = q.value(QStringLiteral("is_kit")).toInt() != 0;
    p.kitJson = q.value(QStringLiteral("kit_json")).toString();
    return p;
}

QString ProductRepository::generateBarcode(const QString &sku)
{
    // EAN-13 con prefijo Colombia 770 (antes: hash() de Python)
    const quint64 h = qHash(sku);
    return QStringLiteral("770%1").arg(h % 10000000000ULL, 10, 10, u'0');
}

QList<Product> ProductRepository::list() const
{
    QList<Product> out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT * FROM products ORDER BY id")))
        return out;
    while (q.next())
        out << rowToProduct(q);
    return out;
}

std::optional<Product> ProductRepository::findById(int id) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM products WHERE id=?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return rowToProduct(q);
    return std::nullopt;
}

std::optional<Product> ProductRepository::findBySku(const QString &sku) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM products WHERE sku=?"));
    q.addBindValue(sku.trimmed());
    if (q.exec() && q.next())
        return rowToProduct(q);
    return std::nullopt;
}

std::optional<Product> ProductRepository::findByBarcode(const QString &barcode) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM products WHERE barcode=?"));
    q.addBindValue(barcode.trimmed());
    if (q.exec() && q.next())
        return rowToProduct(q);
    return std::nullopt;
}

QList<Product> ProductRepository::search(const QString &text) const
{
    const QString t = text.trimmed().toLower();
    QList<Product> out;
    QSqlQuery q(m_db);
    if (t.isEmpty()) {
        if (q.exec(QStringLiteral("SELECT * FROM products ORDER BY id")))
            while (q.next())
                out << rowToProduct(q);
        return out;
    }
    q.prepare(QStringLiteral(
        "SELECT * FROM products WHERE lower(name) LIKE ? OR lower(sku) LIKE ? "
        "OR lower(barcode) LIKE ? OR lower(cat) LIKE ? ORDER BY id"));
    const QString like = u'%' + t + u'%';
    q.addBindValue(like);
    q.addBindValue(like);
    q.addBindValue(like);
    q.addBindValue(like);
    if (!q.exec())
        return out;
    while (q.next())
        out << rowToProduct(q);
    return out;
}

Result<Product> ProductRepository::add(const Product &pin)
{
    Product p = pin;
    p.sku = p.sku.trimmed();
    if (p.sku.isEmpty())
        return Result<Product>::failure(QStringLiteral("SKU requerido"));
    if (findBySku(p.sku))
        return Result<Product>::failure(QStringLiteral("SKU %1 ya existe").arg(p.sku));
    p.name = p.name.trimmed();
    if (p.name.size() < 2)
        return Result<Product>::failure(QStringLiteral("Nombre mínimo 2 caracteres"));
    if (p.price <= 0 || p.stock < 0)
        return Result<Product>::failure(QStringLiteral("Precio >0 y stock >=0"));
    if (p.priceBuy <= 0)
        p.priceBuy = p.price * 0.7;
    if (p.priceWholesale <= 0)
        p.priceWholesale = p.price * 0.9;
    if (p.vencimiento.isEmpty() && p.cat == QLatin1String("Abarrotes"))
        p.vencimiento = QDate::currentDate().addDays(180).toString(Qt::ISODate);
    if (p.barcode.size() < 8)
        p.barcode = generateBarcode(p.sku);
    if (p.isKit) {
        QJsonArray comps = QJsonDocument::fromJson(p.kitJson.toUtf8()).array();
        if (comps.isEmpty())
            return Result<Product>::failure(QStringLiteral("Kit debe tener componentes"));
        for (const auto &c : comps) {
            if (!findBySku(c.toObject().value(QStringLiteral("sku")).toString()))
                return Result<Product>::failure(
                    QStringLiteral("Componente %1 no existe")
                        .arg(c.toObject().value(QStringLiteral("sku")).toString()));
        }
    }
    if (p.stockMax <= 0)
        p.stockMax = p.stock + 50;

    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, "
        "price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, "
        "status, image, lote, vencimiento, is_kit, kit_json) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
    q.addBindValue(p.sku);
    q.addBindValue(p.barcode);
    q.addBindValue(p.name);
    q.addBindValue(p.description);
    q.addBindValue(p.cat.isEmpty() ? QStringLiteral("General") : p.cat);
    q.addBindValue(p.subcat);
    q.addBindValue(p.brand);
    q.addBindValue(p.supplier);
    q.addBindValue(p.price);
    q.addBindValue(p.priceBuy);
    q.addBindValue(p.priceWholesale);
    q.addBindValue(p.tax.isEmpty() ? QStringLiteral("IVA 19%") : p.tax);
    q.addBindValue(p.unit.isEmpty() ? QStringLiteral("unidad") : p.unit);
    q.addBindValue(p.stock);
    q.addBindValue(p.stockMin);
    q.addBindValue(p.stockMax);
    q.addBindValue(p.location);
    q.addBindValue(p.status.isEmpty() ? QStringLiteral("activo") : p.status);
    q.addBindValue(p.image.isEmpty() ? QVariant() : p.image);
    q.addBindValue(p.lote.isEmpty() ? QVariant() : p.lote);
    q.addBindValue(p.vencimiento.isEmpty() ? QVariant() : p.vencimiento);
    q.addBindValue(p.isKit ? 1 : 0);
    q.addBindValue(p.kitJson);
    if (!q.exec())
        return Result<Product>::failure(q.lastError().text());
    return Result<Product>::success(*findBySku(p.sku));
}

Result<Product> ProductRepository::update(const QString &sku, const Product &p)
{
    if (!findBySku(sku))
        return Result<Product>::failure(
            QStringLiteral("Producto SKU %1 no encontrado").arg(sku));
    if (p.price <= 0 || p.priceBuy <= 0 || p.stock < 0 || p.stockMin < 0 || p.stockMax < 0)
        return Result<Product>::failure(QStringLiteral("Precio >0 y stocks >=0"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE products SET name=?, cat=?, brand=?, supplier=?, description=?, price=?, "
        "price_buy=?, price_wholesale=?, tax=?, unit=?, stock=?, stock_min=?, stock_max=?, "
        "location=?, status=?, image=?, lote=?, vencimiento=?, barcode=? WHERE sku=?"));
    q.addBindValue(p.name);
    q.addBindValue(p.cat);
    q.addBindValue(p.brand);
    q.addBindValue(p.supplier);
    q.addBindValue(p.description);
    q.addBindValue(p.price);
    q.addBindValue(p.priceBuy);
    q.addBindValue(p.priceWholesale);
    q.addBindValue(p.tax);
    q.addBindValue(p.unit);
    q.addBindValue(p.stock);
    q.addBindValue(p.stockMin);
    q.addBindValue(p.stockMax);
    q.addBindValue(p.location);
    q.addBindValue(p.status);
    q.addBindValue(p.image.isEmpty() ? QVariant() : p.image);
    q.addBindValue(p.lote.isEmpty() ? QVariant() : p.lote);
    q.addBindValue(p.vencimiento.isEmpty() ? QVariant() : p.vencimiento);
    q.addBindValue(p.barcode);
    q.addBindValue(sku);
    if (!q.exec())
        return Result<Product>::failure(q.lastError().text());
    return Result<Product>::success(*findBySku(sku));
}

StatusResult ProductRepository::remove(const QString &sku)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM products WHERE sku=?"));
    q.addBindValue(sku);
    if (!q.exec() || q.numRowsAffected() == 0)
        return StatusResult::failure(
            QStringLiteral("Producto SKU %1 no encontrado").arg(sku));
    return StatusResult::success({});
}

bool ProductRepository::setStockById(int id, int stock)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE products SET stock=? WHERE id=?"));
    q.addBindValue(stock);
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}

bool ProductRepository::setStockBySku(const QString &sku, int stock)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE products SET stock=? WHERE sku=?"));
    q.addBindValue(stock);
    q.addBindValue(sku);
    return q.exec() && q.numRowsAffected() > 0;
}

QList<Product> ProductRepository::kits() const
{
    QList<Product> out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT * FROM products WHERE is_kit=1 ORDER BY id")))
        return out;
    while (q.next())
        out << rowToProduct(q);
    return out;
}

QList<KitComponent> ProductRepository::kitComponents(const QString &sku) const
{
    QList<KitComponent> out;
    const auto p = findBySku(sku);
    if (!p || !p->isKit)
        return out;
    const QJsonArray arr = QJsonDocument::fromJson(p->kitJson.toUtf8()).array();
    for (const auto &c : arr) {
        KitComponent k;
        k.sku = c.toObject().value(QStringLiteral("sku")).toString();
        k.qty = c.toObject().value(QStringLiteral("qty")).toInt();
        out << k;
    }
    return out;
}

int ProductRepository::kitStock(const QList<KitComponent> &components) const
{
    int minStock = std::numeric_limits<int>::max();
    for (const KitComponent &c : components) {
        const auto p = findBySku(c.sku);
        if (!p || c.qty <= 0)
            return 0;
        minStock = std::min(minStock, p->stock / c.qty);
    }
    return minStock == std::numeric_limits<int>::max() ? 0 : minStock;
}

Result<Product> ProductRepository::createKit(const QString &sku, const QString &name,
                                             const QList<KitComponent> &components,
                                             double priceOverride, const QString &user)
{
    if (findBySku(sku))
        return Result<Product>::failure(QStringLiteral("SKU %1 ya existe").arg(sku));
    if (components.isEmpty())
        return Result<Product>::failure(QStringLiteral("Componentes lista requerida"));
    double totalCost = 0.0, totalSale = 0.0;
    for (const KitComponent &c : components) {
        const auto prod = findBySku(c.sku);
        if (!prod)
            return Result<Product>::failure(
                QStringLiteral("Componente %1 no existe").arg(c.sku));
        if (c.qty <= 0)
            return Result<Product>::failure(QStringLiteral("Qty >0"));
        totalCost += (prod->priceBuy > 0 ? prod->priceBuy : prod->price * 0.7) * c.qty;
        totalSale += prod->price * c.qty;
    }
    QJsonArray arr;
    for (const KitComponent &c : components) {
        QJsonObject o;
        o[QStringLiteral("sku")] = c.sku;
        o[QStringLiteral("qty")] = c.qty;
        arr << o;
    }
    Product p;
    p.sku = sku;
    p.name = name;
    p.cat = QStringLiteral("Kits");
    p.description = QStringLiteral("Kit %1 productos").arg(components.size());
    p.price = priceOverride > 0 ? priceOverride : totalSale * 0.95;
    p.priceBuy = totalCost;
    p.stock = kitStock(components);
    p.isKit = true;
    p.kitJson = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    p.barcode = generateBarcode(sku);
    auto r = add(p);
    if (r.ok() && m_audit)
        m_audit->log(user, QStringLiteral("kit_creado"), sku);
    return r;
}

QList<Product> ProductRepository::expiringWithin(int days) const
{
    QList<Product> out;
    const QString limit = QDate::currentDate().addDays(days).toString(Qt::ISODate);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT * FROM products WHERE vencimiento IS NOT NULL AND vencimiento != '' "
        "AND vencimiento <= ? ORDER BY vencimiento"));
    q.addBindValue(limit);
    if (!q.exec())
        return out;
    while (q.next())
        out << rowToProduct(q);
    return out;
}
