#include "DashboardController.h"

DashboardController::DashboardController(ReportService *reports,
                                         InventoryRepository *inventory, QObject *parent)
    : QObject(parent), m_reports(reports), m_inventory(inventory)
{
    refresh();
}

void DashboardController::refresh()
{
    QVariantMap d = m_reports->stats();
    d[QStringLiteral("topProducts")] = m_reports->topProducts(5);
    d[QStringLiteral("salesByDay")] = m_reports->salesByDay(7);
    d[QStringLiteral("summary")] = m_reports->salesSummary();
    d[QStringLiteral("kpis")] = m_reports->kpis();
    QVariantList low;
    for (const Product &p : m_inventory->belowMin()) {
        low << QVariantMap{{"sku", p.sku}, {"name", p.name}, {"stock", p.stock},
                           {"min", p.stockMin}};
    }
    d[QStringLiteral("lowStock")] = low;
    m_data = d;
    emit dataChanged();
}
