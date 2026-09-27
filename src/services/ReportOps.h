#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QVariantMap>

#include "SettingsService.h"

// Reportes operativos — extraído de ReportService sin cambiar firmas ni
// consultas. Retorna QVariant nativo para QML directo.
class ReportOps : public QObject
{
    Q_OBJECT

  public:
    explicit ReportOps(QSqlDatabase db, SettingsService *settings = nullptr,
                       QObject *parent = nullptr);

    Q_INVOKABLE QVariantMap stats(const QString &businessType = {}) const;
    Q_INVOKABLE QVariantList topProducts(int n = 5, const QString &businessType = {}) const;
    Q_INVOKABLE QVariantList leastSold(int n = 5, const QString &businessType = {}) const;
    Q_INVOKABLE QVariantList topClients(int n = 5, const QString &businessType = {}) const;
    Q_INVOKABLE QVariantList topSellers(int n = 5, const QString &businessType = {}) const;
    Q_INVOKABLE QVariantList marginPerProduct(const QString &businessType = {}) const;

  private:
    QString effectiveBt(const QString &businessType) const;
    QSqlDatabase m_db;
    SettingsService *m_settings = nullptr;
};
