#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "../repositories/ClientRepository.h"
#include "../services/CreditService.h"

// CRM clientes (antes ClientsScreen): CRUD + estado de cuenta + abonos.
class ClientsController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList clients READ clients NOTIFY clientsChanged)

public:
    explicit ClientsController(ClientRepository *clients, ReceivablesService *cxc,
                               QObject *parent = nullptr);

    QVariantList clients() const { return m_clients; }

    Q_INVOKABLE void search(const QString &text);
    Q_INVOKABLE QVariantMap add(const QVariantMap &fields);
    Q_INVOKABLE QVariantMap update(int id, const QVariantMap &fields);
    Q_INVOKABLE QVariantMap remove(int id);
    Q_INVOKABLE QVariantList statement(const QString &name) const;
    Q_INVOKABLE QVariantMap pay(const QString &saleId, double amount,
                                const QString &method, const QString &user);

    static QVariantMap toMap(const Client &c);
    static Client fromMap(const QVariantMap &m, const Client &base = {});

signals:
    void clientsChanged();

private:
    ClientRepository *m_repos = nullptr;
    ReceivablesService *m_cxc = nullptr;
    QVariantList m_clients;
};

// Proveedores (antes SuppliersScreen): CRUD.
class SuppliersController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList suppliers READ suppliers NOTIFY suppliersChanged)

public:
    explicit SuppliersController(SupplierRepository *suppliers, QObject *parent = nullptr);

    QVariantList suppliers() const { return m_suppliers; }

    Q_INVOKABLE void search(const QString &text);
    Q_INVOKABLE QVariantMap add(const QVariantMap &fields);
    Q_INVOKABLE QVariantMap update(int id, const QVariantMap &fields);
    Q_INVOKABLE QVariantMap remove(int id);

    static QVariantMap toMap(const Supplier &s);
    static Supplier fromMap(const QVariantMap &m, const Supplier &base = {});

signals:
    void suppliersChanged();

private:
    SupplierRepository *m_repos = nullptr;
    QVariantList m_suppliers;
};
