#include "PurchaseRepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>

PurchaseRepository::PurchaseRepository(QSqlDatabase db, AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_audit(audit)
{
}

QList<PurchaseItem> PurchaseRepository::parseItems(const QString &json)
{
    QList<PurchaseItem> out;
    const QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();
    for (const auto &v : arr) {
        const QJsonObject o = v.toObject();
        PurchaseItem it;
        it.sku = o.value(QStringLiteral("sku")).toString();
        it.qty = o.value(QStringLiteral("qty")).toInt();
        it.priceBuy = o.value(QStringLiteral("price_buy")).toDouble();
        out << it;
    }
    return out;
}

QString PurchaseRepository::itemsToJson(const QList<PurchaseItem> &items)
{
    QJsonArray arr;
    for (const PurchaseItem &it : items) {
        QJsonObject o;
        o[QStringLiteral("sku")] = it.sku;
        o[QStringLiteral("qty")] = it.qty;
        o[QStringLiteral("price_buy")] = it.priceBuy;
        arr << o;
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

Purchase PurchaseRepository::rowToPurchase(const QSqlQuery &q)
{
    Purchase p;
    p.id = q.value(QStringLiteral("id")).toString();
    p.date = q.value(QStringLiteral("date")).toString();
    p.supplier = q.value(QStringLiteral("supplier")).toString();
    p.total = q.value(QStringLiteral("total")).toDouble();
    p.status = q.value(QStringLiteral("status")).toString();
    p.items = parseItems(q.value(QStringLiteral("items_json")).toString());
    p.notes = q.value(QStringLiteral("notes")).toString();
    return p;
}

QList<Purchase> PurchaseRepository::list() const
{
    QList<Purchase> out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT * FROM purchases ORDER BY id DESC")))
        return out;
    while (q.next())
        out << rowToPurchase(q);
    return out;
}

std::optional<Purchase> PurchaseRepository::find(const QString &folio) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM purchases WHERE id=?"));
    q.addBindValue(folio);
    if (q.exec() && q.next())
        return rowToPurchase(q);
    return std::nullopt;
}

Result<Purchase> PurchaseRepository::insert(const Purchase &p)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO purchases (id, date, supplier, total, status, items_json, notes) "
        "VALUES (?,?,?,?,?,?,?)"));
    q.addBindValue(p.id);
    q.addBindValue(p.date);
    q.addBindValue(p.supplier);
    q.addBindValue(p.total);
    q.addBindValue(p.status);
    q.addBindValue(itemsToJson(p.items));
    q.addBindValue(p.notes);
    if (!q.exec())
        return Result<Purchase>::failure(q.lastError().text());
    return Result<Purchase>::success(*find(p.id));
}

bool PurchaseRepository::setStatus(const QString &folio, const QString &status)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("UPDATE purchases SET status=? WHERE id=?"));
    q.addBindValue(status);
    q.addBindValue(folio);
    return q.exec() && q.numRowsAffected() > 0;
}
