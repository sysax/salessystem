#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QVariantMap>

#include "ReportFinance.h"
#include "ReportInventory.h"
#include "ReportOps.h"
#include "SettingsService.h"

// Fachada de reportes operativos/financieros/KPIs — conserva TODOS los
// Q_INVOKABLEs públicos con las mismas firmas y delega en instancias
// internas (ReportFinance/ReportOps/ReportInventory). exportCsv/exportPdf
// se quedan aquí (usan de todo). Nada de QML ni tests cambia.
//
// Multitienda: los reportes atribuibles a producto (top, margen, rotación,
// valorizado, granel, vencimientos, seriales, mermas, controlados,
// garantías) se filtran por rubro; los agregados de dinero/cliente
// (ventas, caja, impuestos, KPIs) son globales porque `sales` no lleva
// rubro y una venta mixta no es atribuible.
class ReportService : public QObject
{
    Q_OBJECT

  public:
    explicit ReportService(QSqlDatabase db, SettingsService *settings = nullptr,
                           QObject *parent = nullptr);

    Q_INVOKABLE QVariantMap stats(const QString &businessType = {}) const;
    Q_INVOKABLE QVariantList topProducts(int n = 5, const QString &businessType = {}) const;
    Q_INVOKABLE QVariantList leastSold(int n = 5, const QString &businessType = {}) const;
    Q_INVOKABLE QVariantList topClients(int n = 5, const QString &businessType = {}) const;
    Q_INVOKABLE QVariantList topSellers(int n = 5, const QString &businessType = {}) const;
    Q_INVOKABLE QVariantList marginPerProduct(const QString &businessType = {}) const;
    Q_INVOKABLE double averageTicket(const QString &businessType = {}) const;
    Q_INVOKABLE QVariantMap salesForPeriod(const QString &range, const QString &businessType
                                                                 = {}) const; // dia|semana|mes|año
    Q_INVOKABLE QVariantList salesByDay(int days = 7, const QString &businessType = {}) const;
    Q_INVOKABLE QVariantMap salesSummary(const QString &businessType
                                         = {}) const; // conteo por estado
    Q_INVOKABLE QVariantMap incomeStatement(const QString &businessType = {}) const; // resultados
    Q_INVOKABLE QVariantMap cashFlow(const QString &businessType = {}) const;
    Q_INVOKABLE QVariantMap taxes(const QString &businessType = {}) const;
    Q_INVOKABLE QVariantMap kpis(const QString &businessType = {}) const;

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

    // Exporta CSV operativo/financiero; retorna ruta o "" en error.
    // Cabecera con business_name/NIT de `settings` (Fase 0 multinegocio).
    Q_INVOKABLE QString exportCsv(const QString &type, const QString &dir) const;
    // Exporta PDF (QPdfWriter); retorna ruta o "" en error.
    Q_INVOKABLE QString exportPdf(const QString &type, const QString &dir) const;

  private:
    QString setting(const QString &key, const QString &fallback = {}) const;
    QSqlDatabase m_db;
    SettingsService *m_settings = nullptr;
    ReportFinance m_finance;
    ReportOps m_ops;
    ReportInventory m_inventory;
};
