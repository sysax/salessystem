#include "SerialRepository.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>

SerialRepository::SerialRepository(QSqlDatabase db, QObject *parent)
    : QObject(parent), m_db(std::move(db))
{
}

SerialInfo SerialRepository::rowToSerial(const QSqlQuery &q)
{
    SerialInfo s;
    s.id = q.value(QStringLiteral("id")).toInt();
    s.productId = q.value(QStringLiteral("product_id")).toInt();
    s.sku = q.value(QStringLiteral("sku")).toString();
    s.serial = q.value(QStringLiteral("serial")).toString();
    s.status = q.value(QStringLiteral("status")).toString();
    s.saleId = q.value(QStringLiteral("sale_id")).toString();
    s.imei2 = q.value(QStringLiteral("imei2")).toString();
    s.notes = q.value(QStringLiteral("notes")).toString();
    return s;
}

QList<SerialInfo> SerialRepository::inStock(const QString &sku) const
{
    QList<SerialInfo> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT * FROM serials WHERE sku=? AND status='in_stock' ORDER BY id"));
    q.addBindValue(sku);
    if (!q.exec())
        return out;
    while (q.next())
        out << rowToSerial(q);
    return out;
}

int SerialRepository::inStockCount(const QString &sku) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM serials WHERE sku=? AND status='in_stock'"));
    q.addBindValue(sku);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

QList<SerialInfo> SerialRepository::byStatus(const QString &status, int limit) const
{
    QList<SerialInfo> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM serials WHERE status=? ORDER BY id DESC LIMIT ?"));
    q.addBindValue(status);
    q.addBindValue(limit > 0 ? limit : 200);
    if (!q.exec())
        return out;
    while (q.next())
        out << rowToSerial(q);
    return out;
}

bool SerialRepository::hasSerials(const QString &sku) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM serials WHERE sku=?"));
    q.addBindValue(sku);
    return q.exec() && q.next() && q.value(0).toInt() > 0;
}

std::optional<SerialInfo> SerialRepository::find(const QString &serial) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM serials WHERE serial=?"));
    q.addBindValue(serial.trimmed());
    if (q.exec() && q.next())
        return rowToSerial(q);
    return std::nullopt;
}

StatusResult SerialRepository::add(int productId, const QString &sku, const QString &serial,
                                   const QString &imei2, const QString &notes)
{
    const QString clean = serial.trimmed();
    if (clean.size() < 4)
        return StatusResult::failure(QStringLiteral("Serial mínimo 4 caracteres"));
    if (find(clean))
        return StatusResult::failure(
            QStringLiteral("Serial %1 ya registrado").arg(clean));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO serials (product_id, sku, serial, status, imei2, notes) "
        "VALUES (?,?,?,'in_stock',?,?)"));
    q.addBindValue(productId);
    q.addBindValue(sku);
    q.addBindValue(clean);
    q.addBindValue(imei2.trimmed().isEmpty() ? QVariant() : imei2.trimmed());
    q.addBindValue(notes);
    if (!q.exec())
        return StatusResult::failure(q.lastError().text());
    return StatusResult::success({});
}

StatusResult SerialRepository::sell(const QString &serial, const QString &saleId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE serials SET status='sold', sale_id=? WHERE serial=? AND status='in_stock'"));
    q.addBindValue(saleId);
    q.addBindValue(serial.trimmed());
    if (!q.exec())
        return StatusResult::failure(q.lastError().text());
    if (q.numRowsAffected() == 0)
        return StatusResult::failure(
            QStringLiteral("Serial %1 no disponible").arg(serial.trimmed()));
    return StatusResult::success({});
}

int SerialRepository::revertSale(const QString &saleId)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE serials SET status='in_stock', sale_id=NULL WHERE sale_id=? AND status='sold'"));
    q.addBindValue(saleId);
    if (!q.exec())
        return -1;
    return q.numRowsAffected();
}

StatusResult SerialRepository::setStatus(const QString &serial, const QString &status,
                                         const QString &notes)
{
    static const QStringList kOk = {QStringLiteral("in_stock"), QStringLiteral("sold"),
                                    QStringLiteral("rma"), QStringLiteral("repaired")};
    if (!kOk.contains(status))
        return StatusResult::failure(QStringLiteral("Estado inválido: %1").arg(status));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE serials SET status=?, notes=? WHERE serial=?"));
    q.addBindValue(status);
    q.addBindValue(notes);
    q.addBindValue(serial.trimmed());
    if (!q.exec() || q.numRowsAffected() == 0)
        return StatusResult::failure(
            QStringLiteral("Serial %1 no encontrado").arg(serial.trimmed()));
    return StatusResult::success({});
}

QVariantMap SerialRepository::warrantyStatus(const QString &serial, int warrantyMonths) const
{
    const auto s = find(serial);
    if (!s)
        return {{"ok", false}, {"error", QStringLiteral("Serial no registrado")}};
    if (s->status != QLatin1String("sold") || s->saleId.isEmpty())
        return {{"ok", true},
                {"serial", s->serial},
                {"status", s->status},
                {"inWarranty", false},
                {"detail", QStringLiteral("Sin venta asociada")}};
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT date FROM sales WHERE id=?"));
    q.addBindValue(s->saleId);
    QString saleDate;
    if (q.exec() && q.next())
        saleDate = q.value(0).toString();
    const QDate sold = QDate::fromString(saleDate, Qt::ISODate);
    if (!sold.isValid())
        return {{"ok", true},
                {"serial", s->serial},
                {"status", s->status},
                {"inWarranty", false},
                {"detail", QStringLiteral("Venta sin fecha")}};
    const QDate expires = sold.addMonths(warrantyMonths > 0 ? warrantyMonths : 12);
    const bool active = QDate::currentDate() <= expires;
    return {{"ok", true},
            {"serial", s->serial},
            {"status", s->status},
            {"saleId", s->saleId},
            {"saleDate", saleDate},
            {"warrantyMonths", warrantyMonths > 0 ? warrantyMonths : 12},
            {"expiresAt", expires.toString(Qt::ISODate)},
            {"inWarranty", active},
            {"detail", active ? QStringLiteral("En garantía hasta %1").arg(expires.toString(Qt::ISODate))
                              : QStringLiteral("Garantía vencida el %1").arg(expires.toString(Qt::ISODate))}};
}
