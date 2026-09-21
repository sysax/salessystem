#include "InventoryRepository.h"
#include "ProductRepository.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

InventoryRepository::InventoryRepository(QSqlDatabase db, AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_audit(audit)
{
}

bool InventoryRepository::record(const QString &sku, const QString &productName,
                                 const QString &type, int qty, int before, int after,
                                 const QString &reason, const QString &user)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO inventory_movements (ts, sku, product, type, qty, before_qty, after_qty, "
        "reason, user) VALUES (?,?,?,?,?,?,?,?,?)"));
    q.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    q.addBindValue(sku);
    q.addBindValue(productName.left(18));
    q.addBindValue(type);
    q.addBindValue(qty);
    q.addBindValue(before);
    q.addBindValue(after);
    q.addBindValue(reason.left(40));
    q.addBindValue(user);
    return q.exec();
}

InventoryMovement InventoryRepository::rowToMovement(const QSqlQuery &q)
{
    InventoryMovement m;
    m.id = q.value(QStringLiteral("id")).toInt();
    m.ts = q.value(QStringLiteral("ts")).toString();
    m.sku = q.value(QStringLiteral("sku")).toString();
    m.product = q.value(QStringLiteral("product")).toString();
    m.type = q.value(QStringLiteral("type")).toString();
    m.qty = q.value(QStringLiteral("qty")).toInt();
    m.before = q.value(QStringLiteral("before_qty")).toInt();
    m.after = q.value(QStringLiteral("after_qty")).toInt();
    m.reason = q.value(QStringLiteral("reason")).toString();
    m.user = q.value(QStringLiteral("user")).toString();
    return m;
}

QList<InventoryMovement> InventoryRepository::movements(int limit) const
{
    QList<InventoryMovement> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM inventory_movements ORDER BY id DESC LIMIT ?"));
    q.addBindValue(limit);
    if (!q.exec())
        return out;
    while (q.next())
        out << rowToMovement(q);
    std::reverse(out.begin(), out.end()); // cronológico como en Python
    return out;
}

QList<InventoryMovement> InventoryRepository::movementsBySku(const QString &sku) const
{
    QList<InventoryMovement> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM inventory_movements WHERE sku=? ORDER BY id"));
    q.addBindValue(sku);
    if (!q.exec())
        return out;
    while (q.next())
        out << rowToMovement(q);
    return out;
}

InventoryValue InventoryRepository::value() const
{
    InventoryValue v;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral(
            "SELECT COALESCE(SUM(price_buy*stock),0), COALESCE(SUM(price*stock),0), "
            "COALESCE(SUM(stock),0) FROM products")))
        return v;
    if (q.next()) {
        v.costValue = q.value(0).toDouble();
        v.saleValue = q.value(1).toDouble();
        v.units = q.value(2).toLongLong();
    }
    return v;
}

QList<Product> InventoryRepository::lowStock(int threshold) const
{
    QList<Product> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM products WHERE stock < ?"));
    q.addBindValue(threshold);
    if (!q.exec())
        return out;
    while (q.next())
        out << ProductRepository::rowToProduct(q);
    return out;
}

QList<Product> InventoryRepository::belowMin() const
{
    QList<Product> out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT * FROM products WHERE stock < stock_min")))
        return out;
    while (q.next())
        out << ProductRepository::rowToProduct(q);
    return out;
}

QList<Product> InventoryRepository::aboveMax() const
{
    QList<Product> out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT * FROM products WHERE stock > stock_max")))
        return out;
    while (q.next())
        out << ProductRepository::rowToProduct(q);
    return out;
}

QList<Product> InventoryRepository::outOfStock() const
{
    QList<Product> out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT * FROM products WHERE stock=0")))
        return out;
    while (q.next())
        out << ProductRepository::rowToProduct(q);
    return out;
}
