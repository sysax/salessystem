#include "PromoRepository.h"

#include <QSqlError>
#include <QSqlQuery>

PromoRepository::PromoRepository(QSqlDatabase db, ProductRepository *products,
                                 AuditRepository *audit, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_products(products), m_audit(audit)
{
}

Promo PromoRepository::rowToPromo(const QSqlQuery &q)
{
    Promo p;
    p.id = q.value(QStringLiteral("id")).toInt();
    p.name = q.value(QStringLiteral("name")).toString();
    p.type = q.value(QStringLiteral("type")).toString();
    p.value = q.value(QStringLiteral("value")).toDouble();
    p.condition = q.value(QStringLiteral("condition")).toString();
    p.code = q.value(QStringLiteral("code")).toString();
    p.active = q.value(QStringLiteral("active")).toInt() != 0;
    p.desc = q.value(QStringLiteral("desc")).toString();
    return p;
}

QList<Promo> PromoRepository::list() const
{
    QList<Promo> out;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT * FROM promos ORDER BY id")))
        return out;
    while (q.next())
        out << rowToPromo(q);
    return out;
}

std::optional<Promo> PromoRepository::findById(int id) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM promos WHERE id=?"));
    q.addBindValue(id);
    if (q.exec() && q.next())
        return rowToPromo(q);
    return std::nullopt;
}

std::optional<Promo> PromoRepository::findActiveByCode(const QString &code) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM promos WHERE lower(code)=lower(?) AND active=1"));
    q.addBindValue(code.trimmed());
    if (q.exec() && q.next())
        return rowToPromo(q);
    return std::nullopt;
}

Result<Promo> PromoRepository::add(const Promo &pin)
{
    Promo p = pin;
    if (p.code.trimmed().isEmpty())
        return Result<Promo>::failure(QStringLiteral("Código requerido"));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "INSERT INTO promos (name, type, value, condition, code, active, desc) "
        "VALUES (?,?,?,?,?,?,?)"));
    q.addBindValue(p.name);
    q.addBindValue(p.type);
    q.addBindValue(p.value);
    q.addBindValue(p.condition);
    q.addBindValue(p.code.trimmed());
    q.addBindValue(p.active ? 1 : 0);
    q.addBindValue(p.desc);
    if (!q.exec())
        return Result<Promo>::failure(q.lastError().text());
    return Result<Promo>::success(*findActiveByCode(p.code));
}

Result<Promo> PromoRepository::update(int id, const Promo &p)
{
    if (!findById(id))
        return Result<Promo>::failure(QStringLiteral("Promo %1 no encontrada").arg(id));
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral(
        "UPDATE promos SET name=?, type=?, value=?, condition=?, code=?, active=?, desc=? "
        "WHERE id=?"));
    q.addBindValue(p.name);
    q.addBindValue(p.type);
    q.addBindValue(p.value);
    q.addBindValue(p.condition);
    q.addBindValue(p.code);
    q.addBindValue(p.active ? 1 : 0);
    q.addBindValue(p.desc);
    q.addBindValue(id);
    if (!q.exec())
        return Result<Promo>::failure(q.lastError().text());
    return Result<Promo>::success(*findById(id));
}

StatusResult PromoRepository::remove(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM promos WHERE id=?"));
    q.addBindValue(id);
    if (!q.exec() || q.numRowsAffected() == 0)
        return StatusResult::failure(QStringLiteral("Promo %1 no encontrada").arg(id));
    return StatusResult::success({});
}

static std::optional<Product> lookup(ProductRepository *repos, int productId)
{
    if (repos)
        return repos->findById(productId);
    return std::nullopt;
}

Result<PromoDiscount> PromoRepository::evaluate(const QList<CartLine> &cart,
                                                const QString &code) const
{
    PromoDiscount d;
    if (code.trimmed().isEmpty())
        return Result<PromoDiscount>::success(d);
    const auto promo = findActiveByCode(code);
    if (!promo)
        return Result<PromoDiscount>::failure(
            QStringLiteral("Promo %1 no existe o inactiva").arg(code.trimmed()));

    double subtotal = 0.0;
    int qtyTotal = 0;
    for (const CartLine &l : cart) {
        subtotal += l.subtotal;
        qtyTotal += l.qty;
    }
    double discount = 0.0;
    const QString cond = promo->condition;
    if (promo->type == QLatin1String("porcentaje")) {
        const QString c = cond.toLower();
        if (!c.isEmpty() && c != QLatin1String("min 5000") && c != QLatin1String("min 500000")
            && c != QLatin1String("qty>=10")) {
            // Condición = categoría
            double catTotal = 0.0;
            for (const CartLine &l : cart) {
                QString cat = l.cat;
                if (cat.isEmpty() && m_products) {
                    if (const auto p = lookup(m_products, l.productId))
                        cat = p->cat;
                }
                if (!cat.isEmpty() && cat.toLower() == c)
                    catTotal += l.subtotal;
            }
            discount = catTotal > 0 ? catTotal * promo->value / 100.0 : 0.0;
        } else {
            discount = subtotal * promo->value / 100.0;
        }
    } else if (promo->type == QLatin1String("monto_fijo")) {
        double minVal = 0.0;
        const QString lower = cond.toLower();
        if (lower.contains(QLatin1String("min"))) {
            bool ok = false;
            minVal = lower.split(QLatin1String("min")).last().trimmed().split(u' ').first()
                         .toDouble(&ok);
            if (!ok)
                minVal = 0.0;
        }
        if (subtotal >= minVal)
            discount = promo->value;
    } else if (promo->type == QLatin1String("2x1")) {
        const QString sku = cond.trimmed();
        for (const CartLine &l : cart) {
            if (m_products) {
                if (const auto p = lookup(m_products, l.productId)) {
                    if (p->sku == sku && l.qty >= 2)
                        discount += (l.qty / 2) * p->price;
                }
            }
        }
    } else if (promo->type == QLatin1String("3x2")) {
        const QString key = cond.trimmed().toLower();
        for (const CartLine &l : cart) {
            if (m_products) {
                if (const auto p = lookup(m_products, l.productId)) {
                    if (p->sku.toLower() == key || p->cat.toLower() == key) {
                        if (l.qty >= 3)
                            discount += (l.qty / 3) * p->price;
                    }
                }
            }
        }
    } else if (promo->type == QLatin1String("volumen")) {
        if (qtyTotal >= 10)
            discount = subtotal * promo->value / 100.0;
    } else if (promo->type == QLatin1String("cupon")
               || promo->type == QLatin1String("happy_hour")) {
        discount = promo->value ? subtotal * promo->value / 100.0 : 0.0;
    }
    d.discount = std::min(discount, subtotal);
    d.promoName = promo->name;
    d.promoCode = promo->code;
    return Result<PromoDiscount>::success(d);
}
