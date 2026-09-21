#pragma once

#include <QObject>
#include <QSqlDatabase>

#include "../core/Result.h"
#include "../domain/Entities.h"
#include "AuditRepository.h"

// Caja (turno único id=1): apertura/cierre/estado + registro de ventas.
// Semántica idéntica a open/close/get_caja_status.
class CajaRepository : public QObject
{
    Q_OBJECT

public:
    explicit CajaRepository(QSqlDatabase db, AuditRepository *audit = nullptr,
                            QObject *parent = nullptr);

    CajaStatus status() const;
    Result<CajaStatus> open(double amount, const QString &user);
    Result<CajaCloseResult> close(double counted, const QString &user);

    // Agrega {id,total} a sales_today_json y recalcula expected (solo si abierta)
    bool recordSale(const QString &saleId, double total);

private:
    QSqlDatabase m_db;
    AuditRepository *m_audit = nullptr;
};
