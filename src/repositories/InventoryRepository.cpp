#include "InventoryRepository.h"
#include "ProductRepository.h"

#include <QDate>
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

#include <optional>

InventoryRepository::InventoryRepository(QSqlDatabase db, AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_audit(audit)
{
}

bool InventoryRepository::record(const QString &sku, const QString &productName,
                                 const QString &type, double qty, double before, double after,
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
    m.qty = q.value(QStringLiteral("qty")).toDouble();
    m.before = q.value(QStringLiteral("before_qty")).toDouble();
    m.after = q.value(QStringLiteral("after_qty")).toDouble();
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
    if (!q.exec(
            QStringLiteral("SELECT COALESCE(SUM(price_buy*stock),0), COALESCE(SUM(price*stock),0), "
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

QList<Product> InventoryRepository::belowMin(const QString &businessType) const
{
    QList<Product> out;
    QSqlQuery q(m_db);
    const QString bt = businessType.trimmed();
    if (bt.isEmpty() || bt == QLatin1String("miscelanea")) {
        if (!q.exec(QStringLiteral("SELECT * FROM products WHERE stock < stock_min")))
            return out;
    } else {
        q.prepare(QStringLiteral("SELECT * FROM products WHERE stock < stock_min AND "
                                 "(business_type IS NULL OR business_type='' OR business_type=?)"));
        q.addBindValue(bt);
        if (!q.exec())
            return out;
    }
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

// ── Fase 5: lotes PEPS ──────────────────────────────────────────────

Lot InventoryRepository::rowToLot(const QSqlQuery &q)
{
    Lot l;
    l.id = q.value(QStringLiteral("id")).toInt();
    l.sku = q.value(QStringLiteral("sku")).toString();
    l.lote = q.value(QStringLiteral("lote")).toString();
    l.vencimiento = q.value(QStringLiteral("vencimiento")).toString();
    l.qty = q.value(QStringLiteral("qty")).toDouble();
    l.cost = q.value(QStringLiteral("cost")).toDouble();
    l.createdTs = q.value(QStringLiteral("created_ts")).toString();
    return l;
}

Result<Lot> InventoryRepository::addLot(const QString &sku, const QString &lote,
                                        const QString &vencimiento, double qty, double cost)
{
    if (sku.trimmed().isEmpty())
        return Result<Lot>::failure(QStringLiteral("SKU requerido"));
    if (qty <= 1e-9)
        return Result<Lot>::failure(QStringLiteral("Cantidad >0"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO lots (sku, lote, vencimiento, qty, cost, created_ts) "
                             "VALUES (?,?,?,?,?,?)"));
    q.addBindValue(sku.trimmed());
    q.addBindValue(lote.trimmed());
    q.addBindValue(vencimiento.trimmed());
    q.addBindValue(qty);
    q.addBindValue(cost);
    q.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    if (!q.exec())
        return Result<Lot>::failure(q.lastError().text());
    QSqlQuery get(m_db);
    get.prepare(QStringLiteral("SELECT * FROM lots WHERE id=?"));
    get.addBindValue(q.lastInsertId().toLongLong());
    if (!get.exec() || !get.next())
        return Result<Lot>::failure(QStringLiteral("Lote no encontrado tras crear"));
    return Result<Lot>::success(rowToLot(get));
}

QList<Lot> InventoryRepository::lotsBySku(const QString &sku) const
{
    // PEPS: primero los que vencen antes; sin vencimiento al final; luego antigüedad.
    QList<Lot> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT * FROM lots WHERE sku=? AND qty>1e-9 ORDER BY "
        "CASE WHEN vencimiento IS NULL OR vencimiento='' THEN 1 ELSE 0 END, vencimiento ASC, id ASC"));
    q.addBindValue(sku);
    if (!q.exec())
        return out;
    while (q.next())
        out << rowToLot(q);
    return out;
}

bool InventoryRepository::reduceLot(int lotId, double qty)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE lots SET qty = qty - ? WHERE id=? AND qty >= ?"));
    q.addBindValue(qty);
    q.addBindValue(lotId);
    q.addBindValue(qty - 1e-9);
    return q.exec() && q.numRowsAffected() == 1;
}

double InventoryRepository::lotsValue(const QString &sku) const
{
    QSqlQuery q(m_db);
    if (sku.trimmed().isEmpty()) {
        if (!q.exec(QStringLiteral("SELECT COALESCE(SUM(qty*cost),0) FROM lots WHERE qty>0"))
            || !q.next())
            return 0.0;
    } else {
        q.prepare(QStringLiteral("SELECT COALESCE(SUM(qty*cost),0) FROM lots WHERE sku=? AND qty>0"));
        q.addBindValue(sku.trimmed());
        if (!q.exec() || !q.next())
            return 0.0;
    }
    return q.value(0).toDouble();
}

QList<Lot> InventoryRepository::expiringLots(int days) const
{
    QList<Lot> out;
    const QString limit = QDate::currentDate().addDays(days).toString(Qt::ISODate);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM lots WHERE qty>1e-9 AND vencimiento IS NOT NULL AND "
                             "vencimiento!='' AND vencimiento<=? ORDER BY vencimiento ASC"));
    q.addBindValue(limit);
    if (!q.exec())
        return out;
    while (q.next())
        out << rowToLot(q);
    return out;
}

// ── Fase 5: conteos cíclicos ────────────────────────────────────────

InventoryCount InventoryRepository::rowToCount(const QSqlQuery &q)
{
    InventoryCount c;
    c.id = q.value(QStringLiteral("id")).toInt();
    c.ts = q.value(QStringLiteral("ts")).toString();
    c.sku = q.value(QStringLiteral("sku")).toString();
    c.expected = q.value(QStringLiteral("expected")).toDouble();
    c.counted = q.value(QStringLiteral("counted")).toDouble();
    c.diff = q.value(QStringLiteral("diff")).toDouble();
    c.reason = q.value(QStringLiteral("reason")).toString();
    c.user = q.value(QStringLiteral("user")).toString();
    c.status = q.value(QStringLiteral("status")).toString();
    return c;
}

Result<InventoryCount> InventoryRepository::startCount(const QString &sku, double expected,
                                                      double counted, const QString &reason,
                                                      const QString &user)
{
    if (sku.trimmed().isEmpty())
        return Result<InventoryCount>::failure(QStringLiteral("SKU requerido"));
    if (reason.trimmed().isEmpty())
        return Result<InventoryCount>::failure(QStringLiteral("Motivo del conteo requerido"));
    if (counted < -1e-9)
        return Result<InventoryCount>::failure(QStringLiteral("Conteo no puede ser negativo"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO inventory_counts (ts, sku, expected, counted, diff, "
                             "reason, user, status) VALUES (?,?,?,?,?,?,?,?)"));
    q.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    q.addBindValue(sku.trimmed());
    q.addBindValue(expected);
    q.addBindValue(counted);
    q.addBindValue(counted - expected);
    q.addBindValue(reason.trimmed().left(280));
    q.addBindValue(user);
    q.addBindValue(QStringLiteral("Pendiente"));
    if (!q.exec())
        return Result<InventoryCount>::failure(q.lastError().text());
    return findCount(q.lastInsertId().toInt())
        ? Result<InventoryCount>::success(*findCount(q.lastInsertId().toInt()))
        : Result<InventoryCount>::failure(QStringLiteral("Conteo no encontrado tras crear"));
}

QList<InventoryCount> InventoryRepository::listCounts(const QString &status) const
{
    QList<InventoryCount> out;
    QSqlQuery q(m_db);
    if (status.trimmed().isEmpty()) {
        if (!q.exec(QStringLiteral("SELECT * FROM inventory_counts ORDER BY id DESC")))
            return out;
    } else {
        q.prepare(QStringLiteral("SELECT * FROM inventory_counts WHERE status=? ORDER BY id DESC"));
        q.addBindValue(status.trimmed());
        if (!q.exec())
            return out;
    }
    while (q.next())
        out << rowToCount(q);
    return out;
}

std::optional<InventoryCount> InventoryRepository::findCount(int id) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM inventory_counts WHERE id=?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return rowToCount(q);
    return std::nullopt;
}

bool InventoryRepository::markCountApplied(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE inventory_counts SET status='Aplicado' WHERE id=? AND "
                             "status='Pendiente'"));
    q.addBindValue(id);
    return q.exec() && q.numRowsAffected() == 1;
}
