#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "../repositories/InventoryRepository.h"
#include "../repositories/ProductRepository.h"
#include "../repositories/PromoRepository.h"
#include "../repositories/PurchaseRepository.h"
#include "../repositories/ClientRepository.h" // + SupplierRepository
#include "../services/AuthService.h"
#include "../services/CreditService.h"
#include "../services/InventoryService.h"
#include "../services/PurchaseService.h"

// Inventario: movimientos, ajustes, transferencias, alertas (antes InventoryScreen).
class InventoryController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList movements READ movements NOTIFY movementsChanged)
    Q_PROPERTY(QVariantMap alerts READ alerts NOTIFY movementsChanged)

public:
    explicit InventoryController(InventoryService *service, InventoryRepository *inventory,
                                 ProductRepository *products, QObject *parent = nullptr);

    QVariantList movements() const { return m_movements; }
    QVariantMap alerts() const { return m_alerts; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap adjust(const QString &sku, int delta, const QString &reason,
                                   const QString &user);
    Q_INVOKABLE QVariantMap transfer(const QString &sku, int qty, const QString &to,
                                     const QString &reason, const QString &user);
    Q_INVOKABLE QVariantMap valuation() const;

signals:
    void movementsChanged();

private:
    InventoryService *m_service = nullptr;
    InventoryRepository *m_inventory = nullptr;
    ProductRepository *m_products = nullptr;
    QVariantList m_movements;
    QVariantMap m_alerts;
};

// Compras OC → recepción → CxP (antes PurchasesScreen).
class PurchasesController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList orders READ orders NOTIFY ordersChanged)

public:
    explicit PurchasesController(PurchaseService *service, PurchaseRepository *repos,
                                 QObject *parent = nullptr);

    QVariantList orders() const { return m_orders; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap create(const QString &supplier, const QString &sku, int qty,
                                   const QString &user);
    Q_INVOKABLE QVariantMap receive(const QString &folio, const QString &user);
    Q_INVOKABLE QVariantMap cancel(const QString &folio, const QString &user);

signals:
    void ordersChanged();

private:
    static QVariantMap toMap(const Purchase &p);

    PurchaseService *m_service = nullptr;
    PurchaseRepository *m_repos = nullptr;
    QVariantList m_orders;
};

// CxC (antes ReceivablesScreen) y CxP (antes PayablesScreen).
class ReceivablesController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList pending READ pending NOTIFY pendingChanged)

public:
    explicit ReceivablesController(ReceivablesService *service, QObject *parent = nullptr);

    QVariantList pending() const { return m_pending; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantList statement(const QString &client) const;
    Q_INVOKABLE QVariantMap pay(const QString &saleId, double amount,
                                const QString &method, const QString &user);

signals:
    void pendingChanged();

private:
    ReceivablesService *m_service = nullptr;
    QVariantList m_pending;
};

class PayablesController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList pending READ pending NOTIFY pendingChanged)

public:
    explicit PayablesController(PayablesService *service, QObject *parent = nullptr);

    QVariantList pending() const { return m_pending; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap pay(const QString &id, double amount, const QString &method,
                                const QString &user);

signals:
    void pendingChanged();

private:
    PayablesService *m_service = nullptr;
    QVariantList m_pending;
};

// Promociones ABM (antes PromosScreen).
class PromosController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList promos READ promos NOTIFY promosChanged)

public:
    explicit PromosController(PromoRepository *promos, QObject *parent = nullptr);

    QVariantList promos() const { return m_promos; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap add(const QVariantMap &fields);
    Q_INVOKABLE QVariantMap setActive(int id, bool active);
    Q_INVOKABLE QVariantMap remove(int id);

signals:
    void promosChanged();

private:
    static QVariantMap toMap(const Promo &p);

    PromoRepository *m_repos = nullptr;
    QVariantList m_promos;
};

// Usuarios y 2FA admin (antes UsersScreen).
class UsersController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList users READ users NOTIFY usersChanged)

public:
    explicit UsersController(AuthService *auth, QObject *parent = nullptr);

    QVariantList users() const { return m_users; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap add(const QString &username, const QString &password,
                                const QString &role);
    Q_INVOKABLE QVariantMap setActive(const QString &username, bool active);
    Q_INVOKABLE QVariantMap resetPassword(const QString &username, const QString &password);
    Q_INVOKABLE QVariantMap remove(const QString &username);
    Q_INVOKABLE QVariantMap enable2fa(const QString &username);
    Q_INVOKABLE QVariantMap confirm2fa(const QString &username, const QString &code);
    Q_INVOKABLE QVariantMap disable2fa(const QString &username);

signals:
    void usersChanged();

private:
    AuthService *m_auth = nullptr;
    QVariantList m_users;
};
