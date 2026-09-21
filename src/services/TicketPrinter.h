#pragma once

#include <QObject>
#include <QString>

#include <QList>
#include <QMap>

#include "../core/Result.h"

// Ticket de venta en texto (DIAN Colombia, COP) + señal de cajón.
// Port de components/printer.py: siempre guarda .txt (sin hardware real);
// intenta `lp` y pulso ESC/POS en /dev/usb/lp0 (silencioso si no hay).
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
        struct Line {
            QString name;
            int qty = 0;
            double price = 0.0;
            double subtotal = 0.0;
        };
        QList<Line> lines;
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
    Result<PrintResult> print(const Ticket &t) const;
    // Pulso de apertura; true si se pudo escribir en algún dispositivo
    Q_INVOKABLE bool openDrawer() const;
    QString ticketsDir() const { return m_dir; }

private:
    QString m_dir;
};
