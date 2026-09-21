#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "../services/ReportService.h"
#include "../repositories/InventoryRepository.h"

// Tablero estilo GesNet: tarjetas + comparativa 7 días (antes DashboardScreen).
class DashboardController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap data READ data NOTIFY dataChanged)

public:
    explicit DashboardController(ReportService *reports, InventoryRepository *inventory,
                                 QObject *parent = nullptr);

    QVariantMap data() const { return m_data; }
    Q_INVOKABLE void refresh();

signals:
    void dataChanged();

private:
    ReportService *m_reports = nullptr;
    InventoryRepository *m_inventory = nullptr;
    QVariantMap m_data;
};
