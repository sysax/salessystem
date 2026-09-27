#include "ReportService.h"

#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QPdfWriter>
#include <QPainter>
#include <QSqlQuery>
#include <QTextStream>

#include "ReportCommon.h"

ReportService::ReportService(QSqlDatabase db, SettingsService *settings, QObject *parent)
    : QObject(parent), m_db(std::move(db)), m_settings(settings), m_finance(m_db, m_settings, this),
      m_ops(m_db, m_settings, this), m_inventory(m_db, m_settings, this)
{
}

QVariantMap ReportService::stats(const QString &businessType) const
{
    return m_ops.stats(businessType);
}

QVariantList ReportService::topProducts(int n, const QString &businessType) const
{
    return m_ops.topProducts(n, businessType);
}

QVariantList ReportService::leastSold(int n, const QString &businessType) const
{
    return m_ops.leastSold(n, businessType);
}

QVariantList ReportService::topClients(int n, const QString &businessType) const
{
    return m_ops.topClients(n, businessType);
}

QVariantList ReportService::topSellers(int n, const QString &businessType) const
{
    return m_ops.topSellers(n, businessType);
}

QVariantList ReportService::marginPerProduct(const QString &businessType) const
{
    return m_ops.marginPerProduct(businessType);
}

double ReportService::averageTicket(const QString &businessType) const
{
    return m_finance.averageTicket(businessType);
}

QVariantMap ReportService::salesForPeriod(const QString &range, const QString &businessType) const
{
    return m_finance.salesForPeriod(range, businessType);
}

QVariantList ReportService::salesByDay(int days, const QString &businessType) const
{
    return m_finance.salesByDay(days, businessType);
}

QVariantMap ReportService::salesSummary(const QString &businessType) const
{
    return m_finance.salesSummary(businessType);
}

QVariantMap ReportService::incomeStatement(const QString &businessType) const
{
    return m_finance.incomeStatement(businessType);
}

QVariantMap ReportService::cashFlow(const QString &businessType) const
{
    return m_finance.cashFlow(businessType);
}

QVariantMap ReportService::taxes(const QString &businessType) const
{
    return m_finance.taxes(businessType);
}

QVariantMap ReportService::kpis(const QString &businessType) const
{
    return m_finance.kpis(businessType);
}

QVariantList ReportService::expiringProducts(int days, const QString &businessType) const
{
    return m_inventory.expiringProducts(days, businessType);
}

QVariantMap ReportService::serialsReport(const QString &businessType) const
{
    return m_inventory.serialsReport(businessType);
}

QVariantList ReportService::wasteReport(const QString &businessType) const
{
    return m_inventory.wasteReport(businessType);
}

QVariantList ReportService::rotationByCategory(const QString &businessType) const
{
    return m_inventory.rotationByCategory(businessType);
}

QVariantMap ReportService::inventoryValue(const QString &businessType) const
{
    return m_inventory.inventoryValue(businessType);
}

QVariantList ReportService::controlledSales(const QString &businessType) const
{
    return m_inventory.controlledSales(businessType);
}

QVariantList ReportService::warrantyOpen(const QString &businessType) const
{
    return m_inventory.warrantyOpen(businessType);
}

QVariantList ReportService::bulkPerformance(const QString &businessType) const
{
    return m_inventory.bulkPerformance(businessType);
}

QString ReportService::exportCsv(const QString &type, const QString &dir) const
{
    const QString path
        = dir
          + QStringLiteral("/reporte_%1_%2.csv")
                .arg(type,
                     QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    QTextStream out(&f);
    out << "Negocio," << setting(QStringLiteral("business_name"), QStringLiteral("Mi Negocio"))
        << "\n";
    const QString nit = setting(QStringLiteral("business_nit"));
    if (!nit.isEmpty())
        out << "NIT," << nit << "\n";
    if (type == QLatin1String("financiero")) {
        const QVariantMap e = incomeStatement(), fl = cashFlow(), tx = taxes();
        out << "Financiero,Valor\n";
        out << "Ingresos," << e["ingresos"].toDouble() << "\n";
        out << "Costo," << e["costo"].toDouble() << "\n";
        out << "Bruto," << e["bruto"].toDouble() << "\n";
        out << "IVA," << tx["iva_19"].toDouble() << "\n";
        // Fase 1: desglose por tasa.
        for (const QVariant &v : tx["breakdown"].toList()) {
            const QVariantMap m = v.toMap();
            out << "Impuesto " << m["name"].toString() << "," << m["tax"].toDouble() << "\n";
        }
        out << "Flujo neto," << fl["neto"].toDouble() << "\n";
    } else if (type == QLatin1String("vencimientos")) {
        // Fase 3: próximos a vencer (90 días).
        out << "SKU,Nombre,Lote,Vencimiento,Stock\n";
        for (const QVariant &v : expiringProducts(90)) {
            const QVariantMap m = v.toMap();
            out << m["sku"].toString() << "," << m["name"].toString() << "," << m["lote"].toString()
                << "," << m["vencimiento"].toString() << "," << m["stock"].toDouble() << "\n";
        }
    } else if (type == QLatin1String("seriales")) {
        // Fase 4: estado de seriales.
        const QVariantMap rep = serialsReport();
        out << "Serial,SKU,Producto,Estado,Venta\n";
        for (const QVariant &v : rep["items"].toList()) {
            const QVariantMap m = v.toMap();
            out << m["serial"].toString() << "," << m["sku"].toString() << ","
                << m["product"].toString() << "," << m["status"].toString() << ","
                << m["saleId"].toString() << "\n";
        }
    } else if (type == QLatin1String("mermas")) {
        // Fase 4: desperdicio valorizado.
        out << "SKU,Producto,Cantidad,Costo\n";
        for (const QVariant &v : wasteReport()) {
            const QVariantMap m = v.toMap();
            out << m["sku"].toString() << "," << m["product"].toString() << ","
                << m["qty"].toDouble() << "," << m["cost"].toDouble() << "\n";
        }
    } else if (type == QLatin1String("rotacion")) {
        // Fase 5: rotación por categoría.
        out << "Categoria,Productos,Vendidos,Ingreso,Stock,Rotacion\n";
        for (const QVariant &v : rotationByCategory()) {
            const QVariantMap m = v.toMap();
            out << m["category"].toString() << "," << m["products"].toInt() << ","
                << m["sold"].toDouble() << "," << m["revenue"].toDouble() << ","
                << m["stock"].toDouble() << "," << m["rotation"].toDouble() << "\n";
        }
    } else if (type == QLatin1String("inventario")) {
        // Fase 5: valorizado del inventario.
        const QVariantMap iv = inventoryValue();
        out << "Concepto,Valor\n";
        out << "Costo," << iv["cost"].toDouble() << "\n";
        out << "Venta," << iv["sale"].toDouble() << "\n";
        out << "Unidades," << iv["units"].toDouble() << "\n";
        out << "Items," << iv["items"].toInt() << "\n";
    } else if (type == QLatin1String("controlados")) {
        // Fase 5 (farmacia): controlados vendidos.
        out << "Venta,Fecha,SKU,Producto,Cantidad\n";
        for (const QVariant &v : controlledSales()) {
            const QVariantMap m = v.toMap();
            out << m["saleId"].toString() << "," << m["date"].toString() << ","
                << m["sku"].toString() << "," << m["product"].toString() << ","
                << m["qty"].toDouble() << "\n";
        }
    } else if (type == QLatin1String("garantias")) {
        // Fase 5 (celulares): seriales con garantía vigente.
        out << "Serial,SKU,Producto,Venta,FechaVenta,Vence\n";
        for (const QVariant &v : warrantyOpen()) {
            const QVariantMap m = v.toMap();
            out << m["serial"].toString() << "," << m["sku"].toString() << ","
                << m["product"].toString() << "," << m["saleId"].toString() << ","
                << m["saleDate"].toString() << "," << m["expiresAt"].toString() << "\n";
        }
    } else if (type == QLatin1String("granel")) {
        // Fase 5 (abarrotes): rendimiento por unidad de medida.
        out << "Unidad,Cantidad,Ingreso\n";
        for (const QVariant &v : bulkPerformance()) {
            const QVariantMap m = v.toMap();
            out << m["unit"].toString() << "," << m["qty"].toDouble() << ","
                << m["revenue"].toDouble() << "\n";
        }
    } else {
        const QVariantMap d = salesForPeriod(QStringLiteral("dia")),
                          w = salesForPeriod(QStringLiteral("semana"));
        out << "Operativo,Valor,Conteo\n";
        out << "Ventas dia," << d["total"].toDouble() << "," << d["count"].toInt() << "\n";
        out << "Ventas semana," << w["total"].toDouble() << "," << w["count"].toInt() << "\n";
        for (const QVariant &v : topProducts(5)) {
            const QVariantMap m = v.toMap();
            out << "Top," << m["name"].toString() << "," << m["sold"].toInt() << "\n";
        }
    }
    return path;
}

QString ReportService::exportPdf(const QString &type, const QString &dir) const
{
    const QString path
        = dir
          + QStringLiteral("/reporte_%1_%2.pdf")
                .arg(type,
                     QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setTitle(QStringLiteral("Reporte %1 — Sistema de Ventas").arg(type));
    QPainter painter(&writer);
    if (!painter.isActive())
        return {};
    QFont titleFont = painter.font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    int y = 400;
    painter.drawText(
        400, y,
        QStringLiteral("%1 — Reporte %2")
            .arg(setting(QStringLiteral("business_name"), QStringLiteral("Sistema de Ventas")),
                 type));
    y += 350;
    const QString nit = setting(QStringLiteral("business_nit"));
    if (!nit.isEmpty()) {
        painter.drawText(400, y, QStringLiteral("NIT: %1").arg(nit));
        y += 350;
    }
    y += 150;
    QFont bodyFont = painter.font();
    bodyFont.setPointSize(10);
    bodyFont.setBold(false);
    painter.setFont(bodyFont);
    QStringList rows;
    if (type == QLatin1String("financiero")) {
        const QVariantMap e = incomeStatement(), fl = cashFlow(), tx = taxes();
        rows << QStringLiteral("Ingresos: $%1").arg(e["ingresos"].toDouble(), 0, 'f', 0)
             << QStringLiteral("Costo: $%1").arg(e["costo"].toDouble(), 0, 'f', 0)
             << QStringLiteral("Bruto: $%1").arg(e["bruto"].toDouble(), 0, 'f', 0)
             << QStringLiteral("IVA: $%1").arg(tx["iva_19"].toDouble(), 0, 'f', 0)
             << QStringLiteral("Flujo neto: $%1").arg(fl["neto"].toDouble(), 0, 'f', 0);
    } else if (type == QLatin1String("vencimientos")) {
        // Fase 3/5: próximos a vencer (90 días).
        for (const QVariant &v : expiringProducts(90)) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("%1 · lote %2 · vence %3 · stock %4")
                        .arg(m["name"].toString(), m["lote"].toString(),
                             m["vencimiento"].toString())
                        .arg(m["stock"].toDouble());
        }
    } else if (type == QLatin1String("seriales")) {
        // Fase 4/5: estado de seriales.
        for (const QVariant &v : serialsReport()["items"].toList()) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("%1 · %2 · %3")
                        .arg(m["serial"].toString(), m["product"].toString(),
                             m["status"].toString());
        }
    } else if (type == QLatin1String("mermas")) {
        // Fase 4/5: desperdicio valorizado.
        for (const QVariant &v : wasteReport()) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("%1: %2 uds · costo $%3")
                        .arg(m["product"].toString())
                        .arg(m["qty"].toDouble())
                        .arg(m["cost"].toDouble(), 0, 'f', 0);
        }
    } else if (type == QLatin1String("rotacion")) {
        // Fase 5: rotación por categoría.
        for (const QVariant &v : rotationByCategory()) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("%1: %2 uds · rotación %3")
                        .arg(m["category"].toString())
                        .arg(m["sold"].toDouble())
                        .arg(m["rotation"].toDouble(), 0, 'f', 2);
        }
    } else if (type == QLatin1String("inventario")) {
        // Fase 5: valorizado del inventario.
        const QVariantMap iv = inventoryValue();
        rows << QStringLiteral("Costo: $%1").arg(iv["cost"].toDouble(), 0, 'f', 0)
             << QStringLiteral("Venta: $%1").arg(iv["sale"].toDouble(), 0, 'f', 0)
             << QStringLiteral("Unidades: %1").arg(iv["units"].toDouble());
    } else if (type == QLatin1String("controlados")) {
        // Fase 5 (farmacia): controlados vendidos.
        for (const QVariant &v : controlledSales()) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("%1 · %2 x %3 (%4)")
                        .arg(m["saleId"].toString(), m["product"].toString())
                        .arg(m["qty"].toDouble())
                        .arg(m["date"].toString());
        }
    } else if (type == QLatin1String("garantias")) {
        // Fase 5 (celulares): garantías vigentes.
        for (const QVariant &v : warrantyOpen()) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("%1 · vence %2")
                        .arg(m["serial"].toString(), m["expiresAt"].toString());
        }
    } else if (type == QLatin1String("granel")) {
        // Fase 5 (abarrotes): rendimiento por unidad.
        for (const QVariant &v : bulkPerformance()) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("%1: %2 · $%3")
                        .arg(m["unit"].toString())
                        .arg(m["qty"].toDouble())
                        .arg(m["revenue"].toDouble(), 0, 'f', 0);
        }
    } else {
        const QVariantMap d = salesForPeriod(QStringLiteral("dia")),
                          w = salesForPeriod(QStringLiteral("semana"));
        rows << QStringLiteral("Ventas día: $%1 (%2 docs)")
                    .arg(d["total"].toDouble(), 0, 'f', 0)
                    .arg(d["count"].toInt())
             << QStringLiteral("Ventas semana: $%1 (%2 docs)")
                    .arg(w["total"].toDouble(), 0, 'f', 0)
                    .arg(w["count"].toInt());
        for (const QVariant &v : topProducts(5)) {
            const QVariantMap m = v.toMap();
            rows << QStringLiteral("Top: %1 (%2 uds)")
                        .arg(m["name"].toString())
                        .arg(m["sold"].toInt());
        }
    }
    for (const QString &row : rows) {
        y += 350;
        if (y > writer.height() - 400) {
            writer.newPage();
            y = 400;
        }
        painter.drawText(400, y, row);
    }
    painter.end();
    return QFile::exists(path) ? path : QString();
}

QString ReportService::setting(const QString &key, const QString &fallback) const
{
    return ReportCommon::setting(m_db, key, fallback);
}
