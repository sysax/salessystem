#include "DashboardController.h"

DashboardController::DashboardController(ReportService *reports,
                                         InventoryRepository *inventory,
                                         ProductRepository *products, QObject *parent)
    : QObject(parent), m_reports(reports), m_inventory(inventory), m_products(products)
{
    refresh();
}

QVariantList DashboardController::toExpiring(const QList<Product> &ps)
{
    QVariantList out;
    for (const Product &p : ps) {
        out << QVariantMap{{"sku", p.sku},
                           {"name", p.name},
                           {"lote", p.lote},
                           {"vencimiento", p.vencimiento},
                           {"stock", p.stock}};
    }
    return out;
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
    // Fase 3: próximos a vencer (solo si hay repo de productos).
    if (m_products) {
        d[QStringLiteral("expiring30")] = toExpiring(m_products->expiringWithin(30));
        d[QStringLiteral("expiring60")] = toExpiring(m_products->expiringWithin(60));
        d[QStringLiteral("expiring90")] = toExpiring(m_products->expiringWithin(90));
    }
    m_data = d;
    emit dataChanged();
}
