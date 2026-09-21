#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../core/Result.h"
#include "../domain/Entities.h"
#include "AuditRepository.h"

// Clientes CRM (NIT, crédito, descuentos) — validaciones de add/update_client.
class ClientRepository : public QObject
{
    Q_OBJECT

public:
    explicit ClientRepository(QSqlDatabase db, AuditRepository *audit = nullptr,
                              QObject *parent = nullptr);

    QList<Client> list() const;
    std::optional<Client> findById(int id) const;
    std::optional<Client> findByName(const QString &name) const; // case-insensitive
    QList<Client> search(const QString &text) const;

    Result<Client> add(const Client &c);
    Result<Client> update(int id, const Client &c); // discount validado 0-100
    StatusResult remove(int id);

    // Crédito comercial (ventas a crédito y abonos CxC)
    bool addCredit(const QString &name, double amount); // suma a credit+balance
    bool payCredit(const QString &name, double amount); // resta con piso 0

    static Client rowToClient(const QSqlQuery &q);

private:
    QSqlDatabase m_db;
    AuditRepository *m_audit = nullptr;
};

// Proveedores — validaciones de add/update_supplier.
class SupplierRepository : public QObject
{
    Q_OBJECT

public:
    explicit SupplierRepository(QSqlDatabase db, AuditRepository *audit = nullptr,
                                QObject *parent = nullptr);

    QList<Supplier> list() const;
    std::optional<Supplier> findById(int id) const;
    std::optional<Supplier> findByName(const QString &name) const; // case-insensitive
    QList<Supplier> search(const QString &text) const;

    Result<Supplier> add(const Supplier &s);
    Result<Supplier> update(int id, const Supplier &s);
    StatusResult remove(int id);

    static Supplier rowToSupplier(const QSqlQuery &q);

private:
    QSqlDatabase m_db;
    AuditRepository *m_audit = nullptr;
};
