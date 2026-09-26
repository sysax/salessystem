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
    Q_PROPERTY(int totalCount READ totalCount NOTIFY clientsChanged)

  public:
    explicit ClientsController(ClientRepository *clients, ReceivablesService *cxc,
                               QObject *parent = nullptr);

    QVariantList clients() const
    {
        return m_clients;
    }
    int totalCount() const
    {
        return m_totalCount;
    }

    Q_INVOKABLE void search(const QString &text);
    // Fase 4: página servidor (page 0-based; pageSize <= 0 = todo).
    Q_INVOKABLE void searchPaged(const QString &text, int page, int pageSize);
    Q_INVOKABLE QVariantMap add(const QVariantMap &fields);
    Q_INVOKABLE QVariantMap update(int id, const QVariantMap &fields);
    Q_INVOKABLE QVariantMap remove(int id);
    Q_INVOKABLE QVariantList statement(const QString &name) const;
    Q_INVOKABLE QVariantMap pay(const QString &saleId, double amount, const QString &method,
                                const QString &user);

    static QVariantMap toMap(const Client &c);
    static Client fromMap(const QVariantMap &m, const Client &base = {});

  signals:
    void clientsChanged();

  private:
    // Fase 4: re-ejecuta la última consulta tras mutar.
    void reloadClients();

    ClientRepository *m_repos = nullptr;
    ReceivablesService *m_cxc = nullptr;
    QVariantList m_clients;
    int m_totalCount = 0;
    QString m_lastText;
    int m_lastPage = 0;
    int m_lastSize = 0;
    bool m_pagedActive = false;
};

// Proveedores (antes SuppliersScreen): CRUD.
class SuppliersController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList suppliers READ suppliers NOTIFY suppliersChanged)

  public:
    explicit SuppliersController(SupplierRepository *suppliers, QObject *parent = nullptr);

    QVariantList suppliers() const
    {
        return m_suppliers;
    }

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
