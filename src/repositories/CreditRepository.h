#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../core/Result.h"
#include "../domain/Entities.h"
#include "AuditRepository.h"
#include "SaleRepository.h"

// Cuentas por cobrar: saldos, abonos, mora 2 % mensual, estado de cuenta.
class ReceivablesRepository : public QObject
{
    Q_OBJECT

public:
    static double MoraRate; // 2 % mensual

    explicit ReceivablesRepository(QSqlDatabase db, SaleRepository *sales = nullptr,
                                   AuditRepository *audit = nullptr, QObject *parent = nullptr);

    QList<Sale> pending() const; // balance>0 y no Cancelada/Cotización/Pedido
    QList<Sale> statement(const QString &clientName) const;
    QList<CxcPayment> paymentsFor(const QString &saleId) const;

    // Abono validado; liquida a Pagada si saldo <= 0.01 y descuenta crédito.
    Result<Sale> addPayment(const QString &saleId, double amount,
                            const QString &method, const QString &user);

    // Interés moratorio sobre saldo vencido (0 si no vencida o sin saldo)
    static double mora(const Sale &s, double rate = MoraRate);

private:
    QSqlDatabase m_db;
    SaleRepository *m_sales = nullptr;
    AuditRepository *m_audit = nullptr;
};

// Cuentas por pagar: saldos, pagos parciales, pronto pago.
class PayablesRepository : public QObject
{
    Q_OBJECT

public:
    explicit PayablesRepository(QSqlDatabase db, AuditRepository *audit = nullptr,
                                QObject *parent = nullptr);

    QList<Payable> pending() const; // balance>0
    std::optional<Payable> find(const QString &id) const;

    struct PaymentResult {
        Payable payable;
        double earlyDiscount = 0.0; // descuento pronto pago aplicado
    };
    Result<PaymentResult> addPayment(const QString &payableId, double amount,
                                     const QString &method, const QString &user);

    // Alta de CxP (usada por PurchaseService al recibir OC)
    StatusResult create(const Payable &p);

    static Payable rowToPayable(const QSqlQuery &q);

private:
    QSqlDatabase m_db;
    AuditRepository *m_audit = nullptr;
};
