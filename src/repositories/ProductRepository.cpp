#include "ProductRepository.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

#include <limits>

#include "../domain/Attrs.h"

ProductRepository::ProductRepository(QSqlDatabase db, AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_audit(audit)
{
}

const QStringList ProductRepository::Units = {
    QStringLiteral("unidad"), QStringLiteral("g"),  QStringLiteral("kg"),
    QStringLiteral("ml"),     QStringLiteral("l"),  QStringLiteral("caja"),
    QStringLiteral("paquete"), QStringLiteral("metro"),
};

bool ProductRepository::isWeighable(const QString &unit)
{
    const QString u = unit.trimmed().toLower();
    return u == QLatin1String("g") || u == QLatin1String("kg") || u == QLatin1String("ml")
        || u == QLatin1String("l");
}

QString ProductRepository::normalizeUnit(const QString &unit)
{
    const QString u = unit.trimmed().toLower();
    return Units.contains(u) ? u : QString();
}

bool ProductRepository::isValidEan13(const QString &barcode)
{
    const QString b = barcode.trimmed();
    if (b.size() != 13)
        return false;
    for (const QChar c : b) {
        if (!c.isDigit())
            return false;
    }
    int sum = 0;
    for (int i = 0; i < 12; ++i) {
        const int d = b[i].digitValue();
        sum += (i % 2 == 0) ? d : d * 3;
    }
    const int check = (10 - (sum % 10)) % 10;
    return check == b[12].digitValue();
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
    p.stock = q.value(QStringLiteral("stock")).toDouble();
    p.stockMin = q.value(QStringLiteral("stock_min")).toDouble();
    p.stockMax = q.value(QStringLiteral("stock_max")).toDouble();
    p.location = q.value(QStringLiteral("location")).toString();
    p.status = q.value(QStringLiteral("status")).toString();
    p.image = q.value(QStringLiteral("image")).toString();
    p.lote = q.value(QStringLiteral("lote")).toString();
    p.vencimiento = q.value(QStringLiteral("vencimiento")).toString();
    p.isKit = q.value(QStringLiteral("is_kit")).toInt() != 0;
    p.kitJson = q.value(QStringLiteral("kit_json")).toString();
    // Columna aditiva Fase 3: en BDs legadas aún no existe → '{}'.
    p.attrsJson = q.value(QStringLiteral("attrs_json")).toString();
    if (p.attrsJson.trimmed().isEmpty())
        p.attrsJson = QStringLiteral("{}");
    return p;
}

QString ProductRepository::generateBarcode(const QString &sku)
{
    // EAN-13 con prefijo Colombia 770 (antes: hash() de Python).
    // Fase 3: el 13.er dígito es el verificador calculado (antes era azar).
    const quint64 h = qHash(sku);
    // QString::number evita la sobrecarga ambigua de QString::arg numérico (GCC + Qt 6.4)
    QString base = QStringLiteral("770")
        + QString::number(h % 1000000000ULL).rightJustified(9, u'0');
    int sum = 0;
    for (int i = 0; i < 12; ++i) {
        const int d = base[i].digitValue();
        sum += (i % 2 == 0) ? d : d * 3;
    }
    return base + QString::number((10 - (sum % 10)) % 10);
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
    // Fase 3: vencimiento con formato válido si se informa; attrs JSON objeto.
    if (!p.vencimiento.trimmed().isEmpty()
        && !QDate::fromString(p.vencimiento.trimmed(), Qt::ISODate).isValid())
        return Result<Product>::failure(
            QStringLiteral("Vencimiento inválido (use AAAA-MM-DD)"));
    if (!Attrs::isObject(p.attrsJson))
        return Result<Product>::failure(QStringLiteral("Atributos inválidos (JSON objeto)"));
    // Fase 3: barcode de 13 dígitos debe ser EAN-13 válido (UPC-12 e
    // internos de otra longitud se aceptan para no romper legacy).
    if (p.barcode.trimmed().size() == 13 && !isValidEan13(p.barcode))
        return Result<Product>::failure(
            QStringLiteral("EAN-13 inválido (dígito verificador)"));
    // Fase 2: unidad canónica. En alta se exige; en edición se preservan
    // valores legacy (p. ej. "pieza") salvo que se cambien a otro inválido.
    if (!normalizeUnit(p.unit).isEmpty()) {
        p.unit = normalizeUnit(p.unit);
    } else if (!p.unit.trimmed().isEmpty()) {
        return Result<Product>::failure(
            QStringLiteral("Unidad inválida (válidas: %1)").arg(Units.join(u", ")));
    } else {
        p.unit = QStringLiteral("unidad");
    }
    if (p.priceBuy <= 0)
        p.priceBuy = p.price * 0.7;
    if (p.priceWholesale <= 0)
        p.priceWholesale = p.price * 0.9;
    // Fase 3: sin auto-vencimientos (el +180d anterior inventaba fechas y
    // sabotea require_expiry; el vencimiento lo informa el usuario).
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
        "status, image, lote, vencimiento, is_kit, kit_json, attrs_json) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
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
    q.addBindValue(p.attrsJson.trimmed().isEmpty() ? QStringLiteral("{}") : p.attrsJson);
    if (!q.exec())
        return Result<Product>::failure(q.lastError().text());
    return Result<Product>::success(*findBySku(p.sku));
}

Result<Product> ProductRepository::update(const QString &sku, const Product &p)
{
    const auto cur = findBySku(sku);
    if (!cur)
        return Result<Product>::failure(
            QStringLiteral("Producto SKU %1 no encontrado").arg(sku));
    if (p.price <= 0 || p.priceBuy <= 0 || p.stock < 0 || p.stockMin < 0 || p.stockMax < 0)
        return Result<Product>::failure(QStringLiteral("Precio >0 y stocks >=0"));
    if (!p.vencimiento.trimmed().isEmpty()
        && !QDate::fromString(p.vencimiento.trimmed(), Qt::ISODate).isValid())
        return Result<Product>::failure(
            QStringLiteral("Vencimiento inválido (use AAAA-MM-DD)"));
    if (!Attrs::isObject(p.attrsJson))
        return Result<Product>::failure(QStringLiteral("Atributos inválidos (JSON objeto)"));
    // Unidad legacy preservada salvo cambio explícito a valor inválido.
    QString unit = p.unit;
    if (unit != cur->unit) {
        unit = normalizeUnit(unit);
        if (unit.isEmpty())
            return Result<Product>::failure(
                QStringLiteral("Unidad inválida (válidas: %1)").arg(Units.join(u", ")));
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE products SET name=?, cat=?, subcat=?, brand=?, supplier=?, description=?, price=?, "
        "price_buy=?, price_wholesale=?, tax=?, unit=?, stock=?, stock_min=?, stock_max=?, "
        "location=?, status=?, image=?, lote=?, vencimiento=?, barcode=?, attrs_json=? WHERE sku=?"));
    q.addBindValue(p.name);
    q.addBindValue(p.cat);
    q.addBindValue(p.subcat);
    q.addBindValue(p.brand);
    q.addBindValue(p.supplier);
    q.addBindValue(p.description);
    q.addBindValue(p.price);
    q.addBindValue(p.priceBuy);
    q.addBindValue(p.priceWholesale);
    q.addBindValue(p.tax);
    q.addBindValue(unit);
    q.addBindValue(p.stock);
    q.addBindValue(p.stockMin);
    q.addBindValue(p.stockMax);
    q.addBindValue(p.location);
    q.addBindValue(p.status);
    q.addBindValue(p.image.isEmpty() ? QVariant() : p.image);
    q.addBindValue(p.lote.isEmpty() ? QVariant() : p.lote);
    q.addBindValue(p.vencimiento.isEmpty() ? QVariant() : p.vencimiento);
    q.addBindValue(p.barcode);
    q.addBindValue(p.attrsJson.trimmed().isEmpty() ? QStringLiteral("{}") : p.attrsJson);
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

bool ProductRepository::setStockById(int id, double stock)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE products SET stock=? WHERE id=?"));
    q.addBindValue(stock);
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() > 0;
}

bool ProductRepository::setStockBySku(const QString &sku, double stock)
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
        k.qty = c.toObject().value(QStringLiteral("qty")).toDouble();
        out << k;
    }
    return out;
}

double ProductRepository::kitStock(const QList<KitComponent> &components) const
{
    double minStock = std::numeric_limits<double>::max();
    for (const KitComponent &c : components) {
        const auto p = findBySku(c.sku);
        if (!p || c.qty <= 0)
            return 0.0;
        minStock = std::min(minStock, std::floor(p->stock / c.qty));
    }
    return minStock == std::numeric_limits<double>::max() ? 0.0 : minStock;
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
