#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../core/Result.h"
#include "../domain/Entities.h"
#include "AuditRepository.h"
#include "ProductRepository.h"

// Promociones — CRUD + evaluación de descuento sobre carrito.
// Tipos: porcentaje|monto_fijo|2x1|3x2|volumen|cupon|happy_hour
// (semántica idéntica a Repository.apply_promo).
struct CartLine {
    int productId = 0;
    QString sku;
    QString cat;
    double price = 0.0;
    int qty = 0;
    double subtotal = 0.0;
};

struct PromoDiscount {
    double discount = 0.0;
    QString promoName;
    QString promoCode;
};

class PromoRepository : public QObject
{
    Q_OBJECT

public:
    explicit PromoRepository(QSqlDatabase db, ProductRepository *products = nullptr,
                             AuditRepository *audit = nullptr, QObject *parent = nullptr);

    QList<Promo> list() const;
    std::optional<Promo> findById(int id) const;
    std::optional<Promo> findActiveByCode(const QString &code) const;

    Result<Promo> add(const Promo &p);
    Result<Promo> update(int id, const Promo &p);
    StatusResult remove(int id);

    // Descuento en COP para un carrito; código vacío → 0 sin error.
    Result<PromoDiscount> evaluate(const QList<CartLine> &cart, const QString &code) const;

    static Promo rowToPromo(const QSqlQuery &q);

private:
    QSqlDatabase m_db;
    ProductRepository *m_products = nullptr;
    AuditRepository *m_audit = nullptr;
};
