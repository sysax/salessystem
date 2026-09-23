#pragma once

#include <QObject>
#include <QString>

#include <QList>
#include <QMap>

#include "../core/Result.h"

// Ticket de venta en texto (.txt) + señal de cajón.
// Fase 0 multinegocio: la cabecera (nombre, NIT, dirección, teléfono) y el
// símbolo de moneda viajan en el Ticket (los rellena PosController desde
// SettingsService). Sin hardware real: siempre guarda .txt; intenta `lp`
// y pulso ESC/POS en /dev/usb/lp0 (silencioso si no hay).
class TicketPrinter : public QObject
{
    Q_OBJECT

public:
    struct Ticket {
        QString saleId;
        QString clientName;
        QString clientNit = QStringLiteral("NIT");
        QString docType;
        QString cufe;
        // Fase 0: datos del negocio (SettingsService). Defaults conservan el
        // aspecto anterior para tickets construidos en tests viejos.
        QString businessName;
        QString businessNit;
        QString businessAddress;
        QString businessPhone;
        QString currencySymbol = QStringLiteral("$");
        QString taxLabel = QStringLiteral("IVA");
        struct Line {
            QString name;
            double qty = 0.0; // Fase 2: decimal (granel)
            double price = 0.0;
            double subtotal = 0.0;
            QString serial; // Fase 3: IMEI/serial (solo líneas tracked)
        };
        // Fase 1: desglose por tasa (los rellena PosController desde
        // SalesService::Totals::buckets). Vacío ⇒ formato legacy de una línea.
        struct TaxLine {
            QString label;
            double base = 0.0;
            double tax = 0.0;
        };
        QList<Line> lines;
        QList<TaxLine> taxLines;
        double discount = 0.0;
        QString promoCode;
        double tax = 0.0;
        double total = 0.0;
        QMap<QString, double> payments;
        double change = 0.0;
    };
    struct PrintResult {
        QString path;
        bool printed = false;
        QString error;
    };

    // ticketsDir vacío ⇒ QStandardPaths::AppDataLocation/tickets
    explicit TicketPrinter(const QString &ticketsDir = {}, QObject *parent = nullptr);

    static QString buildText(const Ticket &t);
    // Fase 2: cantidad con hasta 3 decimales recortando ceros (2, 0.35, 0.350).
    static QString formatQty(double qty);
    Result<PrintResult> print(const Ticket &t) const;
    // Pulso de apertura; true si se pudo escribir en algún dispositivo
    Q_INVOKABLE bool openDrawer() const;
    QString ticketsDir() const { return m_dir; }

private:
    QString m_dir;
};
