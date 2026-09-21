#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../core/Result.h"
#include "../domain/Entities.h"
#include "AuditRepository.h"

// Órdenes de compra (purchases): persistencia plana. La orquestación
// (stock, movimientos, CxP automática) vive en PurchaseService.
class PurchaseRepository : public QObject
{
    Q_OBJECT

public:
    explicit PurchaseRepository(QSqlDatabase db, AuditRepository *audit = nullptr,
                                QObject *parent = nullptr);

    QList<Purchase> list() const;
    std::optional<Purchase> find(const QString &folio) const;

    Result<Purchase> insert(const Purchase &p);
    bool setStatus(const QString &folio, const QString &status);

    static Purchase rowToPurchase(const QSqlQuery &q);
    static QList<PurchaseItem> parseItems(const QString &json);
    static QString itemsToJson(const QList<PurchaseItem> &items);

private:
    QSqlDatabase m_db;
    AuditRepository *m_audit = nullptr;
};
