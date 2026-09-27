#include "LocationRepository.h"

#include <QSqlError>
#include <QSqlQuery>

#include "../core/Transaction.h"

LocationRepository::LocationRepository(QSqlDatabase db, AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_audit(audit)
{
}

QList<LocationRepository::Location> LocationRepository::locations() const
{
    QList<Location> out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT id, name FROM locations ORDER BY id")))
        return out;
    while (q.next())
        out << Location{q.value(0).toInt(), q.value(1).toString()};
    return out;
}

std::optional<LocationRepository::Location> LocationRepository::findLocation(int id) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id, name FROM locations WHERE id=?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return Location{q.value(0).toInt(), q.value(1).toString()};
    return std::nullopt;
}

std::optional<LocationRepository::Location> LocationRepository::findByName(const QString &name) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT id, name FROM locations WHERE name=?"));
    q.addBindValue(name.trimmed());
    if (q.exec() && q.next())
        return Location{q.value(0).toInt(), q.value(1).toString()};
    return std::nullopt;
}

Result<LocationRepository::Location> LocationRepository::ensureLocation(const QString &name)
{
    const QString clean = name.trimmed();
    if (clean.isEmpty())
        return Result<Location>::failure(QStringLiteral("Nombre de almacén requerido"));
    if (const auto existing = findByName(clean))
        return Result<Location>::success(*existing);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO locations (name) VALUES (?)"));
    q.addBindValue(clean);
    if (!q.exec())
        return Result<Location>::failure(q.lastError().text());
    return Result<Location>::success(*findByName(clean));
}

double LocationRepository::stockAt(const QString &sku, int locationId) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT qty FROM stock_by_location WHERE sku=? AND location_id=?"));
    q.addBindValue(sku);
    q.addBindValue(locationId);
    if (!q.exec() || !q.next())
        return 0.0;
    return q.value(0).toDouble();
}

QMap<int, double> LocationRepository::stockBySku(const QString &sku) const
{
    QMap<int, double> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT location_id, qty FROM stock_by_location WHERE sku=?"));
    q.addBindValue(sku);
    if (!q.exec())
        return out;
    while (q.next())
        out[q.value(0).toInt()] = q.value(1).toDouble();
    return out;
}

double LocationRepository::totalFor(const QString &sku) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COALESCE(SUM(qty),0) FROM stock_by_location WHERE sku=?"));
    q.addBindValue(sku);
    if (!q.exec() || !q.next())
        return 0.0;
    return q.value(0).toDouble();
}

bool LocationRepository::bumpAggregate(const QString &sku, double delta)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE products SET stock = stock + ? WHERE sku=?"));
    q.addBindValue(delta);
    q.addBindValue(sku);
    return q.exec() && q.numRowsAffected() == 1;
}

bool LocationRepository::addStock(const QString &sku, int locationId, double qty)
{
    if (qty <= 1e-9)
        return false;
    Transaction tx(m_db);
    if (!tx.isValid())
        return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO stock_by_location (sku, location_id, qty) VALUES (?,?,?) "
                             "ON CONFLICT(sku, location_id) DO UPDATE SET qty = qty + ?"));
    q.addBindValue(sku);
    q.addBindValue(locationId);
    q.addBindValue(qty);
    q.addBindValue(qty);
    if (!q.exec() || !bumpAggregate(sku, qty))
        return false;
    return tx.commit();
}

bool LocationRepository::takeStock(const QString &sku, int locationId, double qty)
{
    if (qty <= 1e-9)
        return false;
    Transaction tx(m_db);
    if (!tx.isValid())
        return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE stock_by_location SET qty = qty - ? WHERE sku=? AND "
                             "location_id=? AND qty >= ?"));
    q.addBindValue(qty);
    q.addBindValue(sku);
    q.addBindValue(locationId);
    q.addBindValue(qty - 1e-9);
    if (!q.exec() || q.numRowsAffected() != 1 || !bumpAggregate(sku, -qty))
        return false;
    return tx.commit();
}

bool LocationRepository::setStock(const QString &sku, int locationId, double qty)
{
    if (qty < -1e-9)
        return false;
    Transaction tx(m_db);
    if (!tx.isValid())
        return false;
    const double before = stockAt(sku, locationId);
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO stock_by_location (sku, location_id, qty) VALUES (?,?,?) "
                             "ON CONFLICT(sku, location_id) DO UPDATE SET qty=?"));
    q.addBindValue(sku);
    q.addBindValue(locationId);
    q.addBindValue(qty);
    q.addBindValue(qty);
    if (!q.exec() || !bumpAggregate(sku, qty - before))
        return false;
    return tx.commit();
}

bool LocationRepository::transferStock(const QString &sku, int fromId, int toId, double qty)
{
    if (fromId == toId || qty <= 1e-9)
        return false;
    Transaction tx(m_db);
    if (!tx.isValid())
        return false;
    QSqlQuery out(m_db);
    out.prepare(QStringLiteral("UPDATE stock_by_location SET qty = qty - ? WHERE sku=? AND "
                               "location_id=? AND qty >= ?"));
    out.addBindValue(qty);
    out.addBindValue(sku);
    out.addBindValue(fromId);
    out.addBindValue(qty - 1e-9);
    if (!out.exec() || out.numRowsAffected() != 1)
        return false;
    QSqlQuery in(m_db);
    in.prepare(QStringLiteral("INSERT INTO stock_by_location (sku, location_id, qty) VALUES "
                              "(?,?,?) ON CONFLICT(sku, location_id) DO UPDATE SET qty = qty + ?"));
    in.addBindValue(sku);
    in.addBindValue(toId);
    in.addBindValue(qty);
    in.addBindValue(qty);
    if (!in.exec())
        return false;
    // El agregado global no cambia (sale de un almacén y entra a otro).
    return tx.commit();
}

bool LocationRepository::writeLedger(const QString &sku, int locationId, double qty)
{
    if (qty < -1e-9)
        return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT INTO stock_by_location (sku, location_id, qty) VALUES (?,?,?) "
                             "ON CONFLICT(sku, location_id) DO UPDATE SET qty=?"));
    q.addBindValue(sku);
    q.addBindValue(locationId);
    q.addBindValue(qty);
    q.addBindValue(qty);
    return q.exec() && q.numRowsAffected() >= 0;
}

bool LocationRepository::takeLedger(const QString &sku, int locationId, double qty)
{
    if (qty <= 1e-9)
        return false;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE stock_by_location SET qty = qty - ? WHERE sku=? AND "
                             "location_id=? AND qty >= ?"));
    q.addBindValue(qty);
    q.addBindValue(sku);
    q.addBindValue(locationId);
    q.addBindValue(qty - 1e-9);
    return q.exec() && q.numRowsAffected() == 1;
}
