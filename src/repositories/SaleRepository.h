#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../core/Result.h"
#include "../domain/Entities.h"
#include "AuditRepository.h"
#include "CajaRepository.h"
#include "ClientRepository.h"
#include "LocationRepository.h"

class SettingsService;

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

    struct NewSale
    {
        QString clientName;
        QString vendedor = QStringLiteral("vendedor");
        Money subtotal;
        Money discount;
        Money tax;
        Money total;
        QString promoCode;
        // Pagos mixtos: {"efectivo": Money, "credito": Money, ...}. Vacío + método
        // "Credito" ⇒ todo a crédito. Vacío + otro método ⇒ contado total.
        QMap<QString, Money> payments;
        QString paymentMethod = QStringLiteral("Efectivo");
        QString docType;       // vacío ⇒ default DIAN/offline
        QList<SaleItem> items; // productId, qty, subtotal por línea
        bool offline = false;
        // Fase 1: desglose por tasa (JSON de SalesService::bucketsToJson).
        QString taxBreakdownJson;
        // Multitienda: rubro de la venta ('' = mixta/legacy).
        QString businessType;
        // Fase 3: días de crédito para el vencimiento (0 = default 15).
        int creditDays = 0;
        // Fase 6: almacén origen de la venta (default Principal).
        int locationId = LocationRepository::kPrincipalId;
    };

    explicit SaleRepository(QSqlDatabase db, ClientRepository *clients = nullptr,
                            CajaRepository *caja = nullptr, AuditRepository *audit = nullptr,
                            QObject *parent = nullptr);

    // Fase 3: configuración externalizada (series de folios). Sin settings
    // se usan los defaults históricos (COT/PED/REM/FE/NC/ND).
    void setSettings(SettingsService *s);

    QList<Sale> list() const;
    // Fase 4: paginación servidor (limit < 0 = sin límite) + total.
    QList<Sale> listPaged(int limit, int offset) const;
    int count() const;
    // Fase 4: filtro texto + orden servidor (whitelist) + página + total.
    QList<Sale> searchPaged(const QString &text, const QString &sortKey, bool sortAsc, int limit,
                            int offset) const;
    int countSearch(const QString &text) const;
    std::optional<Sale> find(const QString &id) const;
    QList<SaleItem> itemsFor(const QString &saleId) const;

    Result<Sale> create(const NewSale &s);
    Result<Sale> createDocument(const QString &docType, const QString &client, Money total,
                                const QString &user, const QString &parentId = {},
                                const QString &reason = {});
    // Fase 5: máquina de estados documental (origen → destino permitido).
    static bool transitionAllowed(const QString &from, const QString &to);
    // Fase 5: conversión con trazabilidad (clona cabecera + líneas al nuevo
    // folio, parent_id = origen; el origen queda Cerrado como consumido).
    Result<Sale> convertDocument(const QString &originFolio, const QString &targetDocType,
                                 const QString &user);
    // Fase 5: cancelación con reversión — único camino a Cancelada. Lo usa
    // SalesService::cancel (que ya revirtió stock/seriales/crédito/caja);
    // advanceStatus rechaza Cancelada para cerrar el bypass sin reversión.
    Result<Sale> markCancelled(const QString &saleId, const QString &reason, const QString &user);
    // Fase 5: total de notas crédito ligadas a un folio (evita NC múltiple > total).
    Money creditNotesTotal(const QString &parentId) const;
    Result<Sale> advanceStatus(const QString &saleId, const QString &newStatus,
                               const QString &user);
    Result<Sale> createCreditNote(const QString &saleId, Money amount, const QString &reason,
                                  const QString &user);
    Result<Sale> createDebitNote(const QString &saleId, Money amount, const QString &reason,
                                 const QString &user);

    static Sale rowToSale(const QSqlQuery &q);

  private:
    QSqlDatabase m_db;
    ClientRepository *m_clients = nullptr;
    CajaRepository *m_caja = nullptr;
    AuditRepository *m_audit = nullptr;
    // Fase 6: ledger propio (misma conexión; las ventas descuentan Principal).
    LocationRepository *m_locations = nullptr;
    // Fase 3: settings opcionales (solo series de folios; nullptr = defaults).
    SettingsService *m_settings = nullptr;
};
