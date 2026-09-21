#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "../repositories/CajaRepository.h"
#include "../services/SalesService.h"
#include "../services/SyncService.h"
#include "../services/TicketPrinter.h"

// Punto de venta: carrito, pagos mixtos, promos, ticket, caja y offline
// (antes POSScreen). El carrito vive en el controller; el checkout delega
// en SalesService y guarda el ticket vía TicketPrinter.
class PosController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList cart READ cart NOTIFY cartChanged)
    Q_PROPERTY(QVariantMap totals READ totals NOTIFY cartChanged)
    Q_PROPERTY(QVariantMap caja READ caja NOTIFY cajaChanged)
    Q_PROPERTY(int pendingSync READ pendingSync NOTIFY syncChanged)

public:
    explicit PosController(SalesService *sales, ProductRepository *products,
                           PromoRepository *promos, CajaRepository *caja,
                           TicketPrinter *printer, SyncService *sync, QObject *parent = nullptr);

    QVariantList cart() const { return m_cart; }
    QVariantMap totals() const { return m_totals; }
    QVariantMap caja() const { return m_cajaStatus; }
    int pendingSync() const;

    Q_INVOKABLE QVariantMap addToCart(int productId, int qty);
    Q_INVOKABLE void setQty(int index, int qty);
    Q_INVOKABLE void removeLine(int index);
    Q_INVOKABLE void clearCart();
    Q_INVOKABLE QVariantMap applyPromo(const QString &code);
    // payments: {"efectivo": X, ...}; method: "Efectivo"|"Credito"|"Mixto"...
    Q_INVOKABLE QVariantMap checkout(const QString &client, const QVariantMap &payments,
                                     const QString &method, const QString &user);

    Q_INVOKABLE QVariantMap openCaja(double amount, const QString &user);
    Q_INVOKABLE QVariantMap closeCaja(double counted, const QString &user);
    Q_INVOKABLE void refreshCaja();

signals:
    void cartChanged();
    void cajaChanged();
    void syncChanged();

private:
    void recompute();

    SalesService *m_sales = nullptr;
    ProductRepository *m_products = nullptr;
    PromoRepository *m_promos = nullptr;
    CajaRepository *m_caja = nullptr;
    TicketPrinter *m_printer = nullptr;
    SyncService *m_sync = nullptr;
    QVariantList m_cart; // {productId, name, price, qty, subtotal}
    QVariantMap m_totals = {{"subtotal", 0.0}, {"discount", 0.0}, {"tax", 0.0}, {"total", 0.0}};
    QString m_promoCode;
    QVariantMap m_cajaStatus;
};
