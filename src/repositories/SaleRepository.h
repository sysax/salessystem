#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../core/Result.h"
#include "../domain/Entities.h"
#include "AuditRepository.h"
#include "CajaRepository.h"
#include "ClientRepository.h"

// Ventas y documentos (sales + sale_items): folios, pagos mixtos/crédito,
// CUFE, estados, notas. Semántica de Repository.create_sale /
// create_document / advance_sale_status / create_credit|debit_note.
//
// Desviación documentada: el Python hacía
// `UPDATE sales SET reason=?, ref=?` (columnas inexistentes → crash);
// aquí el motivo/notas van a audit_log.
class SaleRepository : public QObject
{
    Q_OBJECT

public:
    static const QStringList EstadosVenta; // Cotización..Cerrada
    static const QStringList DocTypes;

    struct NewSale {
        QString clientName;
        QString vendedor = QStringLiteral("vendedor");
        double subtotal = 0.0;
        double discount = 0.0;
        double tax = 0.0;
        double total = 0.0;
        QString promoCode;
        // Pagos mixtos: {"efectivo": X, "credito": Y, ...}. Vacío + método
        // "Credito" ⇒ todo a crédito. Vacío + otro método ⇒ contado total.
        QMap<QString, double> payments;
        QString paymentMethod = QStringLiteral("Efectivo");
        QString docType; // vacío ⇒ default DIAN/offline
        QList<SaleItem> items; // productId, qty, subtotal por línea
        bool offline = false;
        // Fase 1: desglose por tasa (JSON de SalesService::bucketsToJson).
        QString taxBreakdownJson;
    };

    explicit SaleRepository(QSqlDatabase db, ClientRepository *clients = nullptr,
                            CajaRepository *caja = nullptr, AuditRepository *audit = nullptr,
                            QObject *parent = nullptr);

    QList<Sale> list() const;
    std::optional<Sale> find(const QString &id) const;
    QList<SaleItem> itemsFor(const QString &saleId) const;

    Result<Sale> create(const NewSale &s);
    Result<Sale> createDocument(const QString &docType, const QString &client, double total,
                                const QString &user);
    Result<Sale> advanceStatus(const QString &saleId, const QString &newStatus,
                               const QString &user);
    Result<Sale> createCreditNote(const QString &saleId, double amount, const QString &reason,
                                  const QString &user);
    Result<Sale> createDebitNote(const QString &saleId, double amount, const QString &reason,
                                 const QString &user);

    static Sale rowToSale(const QSqlQuery &q);

private:
    QSqlDatabase m_db;
    ClientRepository *m_clients = nullptr;
    CajaRepository *m_caja = nullptr;
    AuditRepository *m_audit = nullptr;
};
