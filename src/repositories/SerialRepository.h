#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>

#include <optional>

#include "../core/Result.h"

// Fase 3: seriales/IMEI por producto (tabla serials).
// status: in_stock | sold | rma | repaired.
struct SerialInfo {
    int id = 0;
    int productId = 0;
    QString sku;
    QString serial;
    QString status = QStringLiteral("in_stock");
    QString saleId;
    QString imei2;
    QString notes;
};

class SerialRepository : public QObject
{
    Q_OBJECT

public:
    explicit SerialRepository(QSqlDatabase db, QObject *parent = nullptr);

    QList<SerialInfo> inStock(const QString &sku) const;
    int inStockCount(const QString &sku) const;
    bool hasSerials(const QString &sku) const; // ¿producto con seriales registrados?
    std::optional<SerialInfo> find(const QString &serial) const;

    StatusResult add(int productId, const QString &sku, const QString &serial,
                     const QString &imei2 = {}, const QString &notes = {});
    // Vende: solo desde in_stock. Atómico (UPDATE condicional).
    StatusResult sell(const QString &serial, const QString &saleId);
    // Revierte una venta (cancelación): sold → in_stock de esa venta.
    int revertSale(const QString &saleId);
    StatusResult setStatus(const QString &serial, const QString &status,
                           const QString &notes = {});

    // Garantía: fecha venta + warranty_months (attrs del producto).
    QVariantMap warrantyStatus(const QString &serial, int warrantyMonths) const;

    static SerialInfo rowToSerial(const QSqlQuery &q);

private:
    QSqlDatabase m_db;
};
