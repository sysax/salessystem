#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QVariantMap>

#include "SettingsService.h"

// Reportes de inventario / verticales — extraído de ReportService sin cambiar
// firmas ni consultas. Retorna QVariant nativo para QML directo.
//
// Multitienda: estos reportes son atribuibles a producto y se filtran por
// rubro ('' = legacy/mixto, visible en todos).
class ReportInventory : public QObject
{
    Q_OBJECT

  public:
    explicit ReportInventory(QSqlDatabase db, SettingsService *settings = nullptr,
                             QObject *parent = nullptr);

    // Fase 3: próximos a vencer (lista {sku,name,lote,vencimiento,stock}).
    Q_INVOKABLE QVariantList expiringProducts(int days = 30,
                                              const QString &businessType = {}) const;
    // Fase 4: seriales por estado + mermas valorizadas.
    Q_INVOKABLE QVariantMap serialsReport(const QString &businessType = {}) const;
    Q_INVOKABLE QVariantList wasteReport(const QString &businessType = {}) const;
    // Fase 5: reportes por vertical + genéricos (rotación, valorizado).
    Q_INVOKABLE QVariantList rotationByCategory(const QString &businessType = {}) const;
    Q_INVOKABLE QVariantMap inventoryValue(const QString &businessType = {}) const;
    Q_INVOKABLE QVariantList controlledSales(const QString &businessType = {}) const;
    Q_INVOKABLE QVariantList warrantyOpen(const QString &businessType = {}) const;
    Q_INVOKABLE QVariantList bulkPerformance(const QString &businessType = {}) const;

  private:
    QString effectiveBt(const QString &businessType) const;
    QSqlDatabase m_db;
    SettingsService *m_settings = nullptr;
};
