#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "../services/ReportService.h"
#include "../repositories/InventoryRepository.h"
#include "../repositories/ProductRepository.h"
#include "../repositories/SerialRepository.h"

// Tablero estilo GesNet: tarjetas + comparativa 7 días (antes DashboardScreen).
class DashboardController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap data READ data NOTIFY dataChanged)

public:
    explicit DashboardController(ReportService *reports, InventoryRepository *inventory,
                                 ProductRepository *products = nullptr,
                                 SerialRepository *serials = nullptr,
                                 QObject *parent = nullptr);

    QVariantMap data() const { return m_data; }
    Q_INVOKABLE void refresh();

signals:
    void dataChanged();

private:
    static QVariantList toExpiring(const QList<Product> &ps);

    ReportService *m_reports = nullptr;
    InventoryRepository *m_inventory = nullptr;
    ProductRepository *m_products = nullptr;
    SerialRepository *m_serials = nullptr;
    QVariantMap m_data;
};
