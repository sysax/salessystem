#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QVariantList>
#include <QVariantMap>

// Reportes operativos/financieros/KPIs — port de los get_* de
// data/repository.py. Retorna QVariant nativo para QML directo.
class ReportService : public QObject
{
    Q_OBJECT

public:
    explicit ReportService(QSqlDatabase db, QObject *parent = nullptr);

    Q_INVOKABLE QVariantMap stats() const;
    Q_INVOKABLE QVariantList topProducts(int n = 5) const;
    Q_INVOKABLE QVariantList leastSold(int n = 5) const;
    Q_INVOKABLE QVariantList topClients(int n = 5) const;
    Q_INVOKABLE QVariantList topSellers(int n = 5) const;
    Q_INVOKABLE QVariantList marginPerProduct() const;
    Q_INVOKABLE double averageTicket() const;
    Q_INVOKABLE QVariantMap salesForPeriod(const QString &range) const; // dia|semana|mes|año
    Q_INVOKABLE QVariantList salesByDay(int days = 7) const;
    Q_INVOKABLE QVariantMap salesSummary() const; // conteo por estado
    Q_INVOKABLE QVariantMap incomeStatement() const; // estado de resultados
    Q_INVOKABLE QVariantMap cashFlow() const;
    Q_INVOKABLE QVariantMap taxes() const;
    Q_INVOKABLE QVariantMap kpis() const;

    // Exporta CSV operativo/financiero; retorna ruta o "" en error.
    Q_INVOKABLE QString exportCsv(const QString &type, const QString &dir) const;
    // Exporta PDF (QPdfWriter); retorna ruta o "" en error.
    Q_INVOKABLE QString exportPdf(const QString &type, const QString &dir) const;

private:
    QSqlDatabase m_db;
};
