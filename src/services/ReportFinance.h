#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QVariantMap>

#include "SettingsService.h"

// Reportes financieros — extraído de ReportService sin cambiar firmas ni
// consultas. Retorna QVariant nativo para QML directo.
//
// Multitienda: los agregados de dinero (ventas, caja, impuestos, KPIs) son
// globales porque `sales` no lleva rubro y una venta mixta no es atribuible.
class ReportFinance : public QObject
{
    Q_OBJECT

  public:
    explicit ReportFinance(QSqlDatabase db, SettingsService *settings = nullptr,
                           QObject *parent = nullptr);

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

  private:
    QString effectiveBt(const QString &businessType) const;
    QSqlDatabase m_db;
    SettingsService *m_settings = nullptr;
};
