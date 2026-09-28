#include "Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

namespace
{
QString g_dir;
QFile *g_file = nullptr;
qint64 g_maxBytes = 1024 * 1024;
int g_keep = 5;
thread_local QString t_sale;

QString levelName(QtMsgType t)
{
    switch (t) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }
    return QStringLiteral("INFO");
}
} // namespace

QMutex &Logger::mutex()
{
    static QMutex m;
    return m;
}

void Logger::init(const QString &dir, qint64 maxBytes, int keep)
{
    QMutexLocker lock(&mutex());
    if (g_file) // ya inicializado
        return;
    g_dir = dir.trimmed();
    g_maxBytes = maxBytes > 0 ? maxBytes : 1024 * 1024;
    g_keep = keep > 0 ? keep : 5;
    if (g_dir.isEmpty())
        return;
    QDir().mkpath(g_dir);
    const QString path = g_dir + QStringLiteral("/qtsales.log");
    // Rotación al arrancar si el archivo supera el tope: qtsales.log ->
    // qtsales.1.log ... (se descarta el más viejo).
    QFileInfo fi(path);
    if (fi.exists() && fi.size() >= g_maxBytes) {
        QFile::remove(g_dir + QStringLiteral("/qtsales.%1.log").arg(g_keep));
        for (int i = g_keep - 1; i >= 1; --i) {
            const QString src = g_dir + QStringLiteral("/qtsales.%1.log").arg(i);
            const QString dst = g_dir + QStringLiteral("/qtsales.%1.log").arg(i + 1);
            if (QFile::exists(src))
                QFile::rename(src, dst);
        }
        QFile::rename(path, g_dir + QStringLiteral("/qtsales.1.log"));
    }
    g_file = new QFile(path);
    if (!g_file->open(QIODevice::Append | QIODevice::Text)) {
        delete g_file;
        g_file = nullptr;
        g_dir.clear();
        return;
    }
    // Patrón también en consola para sesiones interactivas.
    qSetMessagePattern(QStringLiteral("%{time yyyy-MM-ddTHH:mm:ss.zzz} [%{type}] %{message}"));
    qInstallMessageHandler(Logger::handler);
}

void Logger::shutdown()
{
    QMutexLocker lock(&mutex());
    if (g_file) {
        g_file->flush();
        g_file->close();
        delete g_file;
        g_file = nullptr;
    }
    // No restauramos el handler anterior: la app termina o re-inicializa.
}

bool Logger::isActive()
{
    QMutexLocker lock(&mutex());
    return g_file != nullptr;
}

QString Logger::logDir()
{
    QMutexLocker lock(&mutex());
    return g_dir;
}

QString Logger::logFile()
{
    QMutexLocker lock(&mutex());
    return g_dir.isEmpty() ? QString() : g_dir + QStringLiteral("/qtsales.log");
}

void Logger::setSaleContext(const QString &saleId)
{
    t_sale = saleId.trimmed();
}

QString Logger::saleContext()
{
    return t_sale;
}

void Logger::clearSaleContext()
{
    t_sale.clear();
}

void Logger::infoSale(const QString &saleId, const QString &message)
{
    const QString prev = t_sale;
    t_sale = saleId.trimmed();
    qInfo().noquote() << message;
    t_sale = prev;
}

void Logger::warnSale(const QString &saleId, const QString &message)
{
    const QString prev = t_sale;
    t_sale = saleId.trimmed();
    qWarning().noquote() << message;
    t_sale = prev;
}

void Logger::rotateIfNeeded()
{
    // Llamar con mutex tomado.
    if (!g_file)
        return;
    if (g_file->size() < g_maxBytes)
        return;
    g_file->flush();
    g_file->close();
    QFile::remove(g_dir + QStringLiteral("/qtsales.%1.log").arg(g_keep));
    for (int i = g_keep - 1; i >= 1; --i) {
        const QString src = g_dir + QStringLiteral("/qtsales.%1.log").arg(i);
        const QString dst = g_dir + QStringLiteral("/qtsales.%1.log").arg(i + 1);
        if (QFile::exists(src))
            QFile::rename(src, dst);
    }
    QFile::rename(g_dir + QStringLiteral("/qtsales.log"), g_dir + QStringLiteral("/qtsales.1.log"));
    delete g_file;
    g_file = new QFile(g_dir + QStringLiteral("/qtsales.log"));
    g_file->open(QIODevice::Append | QIODevice::Text);
}

void Logger::handler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    QMutexLocker lock(&mutex());
    const QString ts
        = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzz"));
    QString line = ts + QStringLiteral(" [") + levelName(type) + QStringLiteral("]");
    if (!t_sale.isEmpty())
        line += QStringLiteral(" [sale:") + t_sale + QStringLiteral("]");
    if (ctx.category) {
        const QString cat = QString::fromLatin1(ctx.category);
        if (!cat.isEmpty() && cat != QStringLiteral("default"))
            line += QStringLiteral(" [") + cat + QStringLiteral("]");
    }
    if (ctx.file)
        line += QStringLiteral(" ") + QString::fromLatin1(ctx.file) + QStringLiteral(":")
                + QString::number(ctx.line);
    line += QStringLiteral(" — ") + msg + QStringLiteral("\n");
    if (g_file) {
        rotateIfNeeded();
        if (g_file && g_file->isOpen()) {
            g_file->write(line.toUtf8());
            g_file->flush();
        }
    }
    // Espejo a stderr para no perder visibilidad en terminal/CI.
    fprintf(stderr, "%s", line.toUtf8().constData());
    fflush(stderr);
    if (type == QtFatalMsg)
        abort();
}
