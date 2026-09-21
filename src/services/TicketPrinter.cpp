#include "TicketPrinter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

TicketPrinter::TicketPrinter(const QString &ticketsDir, QObject *parent)
    : QObject(parent)
{
    m_dir = ticketsDir;
    if (m_dir.isEmpty()) {
        m_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/tickets");
    }
    QDir().mkpath(m_dir);
}

static QString money(double v)
{
    // "$1.850.000" sin decimales (formato ticket DIAN)
    long long cop = llround(v);
    QString digits = QString::number(qAbs(cop));
    QString grouped;
    while (digits.size() > 3) {
        grouped.prepend(u'.' + digits.right(3));
        digits.chop(3);
    }
    grouped.prepend(digits);
    return (cop < 0 ? QStringLiteral("-$") : QStringLiteral("$")) + grouped;
}

QString TicketPrinter::buildText(const Ticket &t)
{
    const QString now =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    QStringList lines;
    lines << QString(42, u'=');
    lines << QStringLiteral("   SISTEMA DE VENTAS — COLOMBIA (DIAN)").leftJustified(42);
    lines << QStringLiteral("   Comercio genérico: abarrotes/electrónica").leftJustified(42);
    lines << QString(42, u'=');
    lines << QStringLiteral("Fecha: %1  Factura: %2").arg(now, t.saleId);
    lines << QStringLiteral("Cliente: %1  NIT: %2").arg(t.clientName, t.clientNit);
    lines << QStringLiteral("CUFE DIAN: %1").arg(t.cufe);
    lines << QStringLiteral("Doc: %1").arg(t.docType);
    lines << QString(42, u'-');
    lines << QStringLiteral("%1 %2 %3 %4")
                  .arg(QStringLiteral("Producto"), -16)
                  .arg(QStringLiteral("Cant"), 4)
                  .arg(QStringLiteral("Precio"), 10)
                  .arg(QStringLiteral("Subtotal"), 10);
    lines << QString(42, u'-');
    double subtotal = 0.0;
    for (const Ticket::Line &l : t.lines) {
        subtotal += l.subtotal;
        lines << QStringLiteral("%1 %2 %3 %4")
                     .arg(l.name.left(16), -16)
                     .arg(l.qty, 4)
                     .arg(money(l.price), 10)
                     .arg(money(l.subtotal), 10);
    }
    lines << QString(42, u'-');
    lines << QStringLiteral("Subtotal: %1 COP").arg(money(subtotal));
    if (t.discount > 0)
        lines << QStringLiteral("Descuento (%1): -%2 COP").arg(t.promoCode, money(t.discount));
    lines << QStringLiteral("IVA 19% DIAN: %1 COP").arg(money(t.tax));
    lines << QStringLiteral("TOTAL: %1 COP").arg(money(t.total));
    lines << QString(42, u'-');
    lines << QStringLiteral("Pagos:");
    for (auto it = t.payments.begin(); it != t.payments.end(); ++it) {
        if (it.value() != 0)
            lines << QStringLiteral("  %1 %2 COP")
                          .arg(it.key().first(1).toUpper() + it.key().mid(1), -12)
                          .arg(money(it.value()));
    }
    lines << QStringLiteral("Cambio: %1 COP").arg(money(t.change));
    lines << QString(42, u'-');
    lines << QStringLiteral("Gracias por su compra! — Resolución DIAN");
    lines << QStringLiteral("Modo offline: ticket guardado, se sincroniza al reconectar");
    lines << QString(42, u'=');
    lines << QStringLiteral("Software: Sistema Ventas Qt6/QML — COP — Colombia");
    return lines.join(u'\n');
}

Result<TicketPrinter::PrintResult> TicketPrinter::print(const Ticket &t) const
{
    const QString path =
        m_dir + QStringLiteral("/ticket_%1.txt").arg(t.saleId);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return Result<PrintResult>::failure(
            QStringLiteral("No se pudo guardar ticket: %1").arg(path));
    f.write(buildText(t).toUtf8());
    f.close();

    PrintResult r;
    r.path = path;
    // Intentar impresora del sistema (silencioso si no hay)
    QProcess lp;
    lp.start(QStringLiteral("lp"), {path});
    if (lp.waitForFinished(2000) && lp.exitCode() == 0)
        r.printed = true;
    return Result<PrintResult>::success(r);
}

bool TicketPrinter::openDrawer() const
{
    // ESC p 0 25 250 — pulso cajón (igual que printer.py)
    const QByteArray esc("\x1b\x70\x00\x19\xfa", 5);
    const QStringList candidates = {
        QStringLiteral("/dev/usb/lp0"), QStringLiteral("/dev/usb/lp1"),
        QStringLiteral("/dev/lp0"), m_dir + QStringLiteral("/cajon_signal"),
    };
    for (const QString &dev : candidates) {
        QFile f(dev);
        if (dev.startsWith(QLatin1String("/tmp")) || dev.startsWith(m_dir)
            || QFile::exists(dev)) {
            if (f.open(QIODevice::WriteOnly)) {
                f.write(dev.endsWith(QLatin1String("cajon_signal")) ? "OPEN" : esc);
                return true;
            }
        }
    }
    return false;
}
