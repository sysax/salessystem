#include "CrashHandler.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include <csignal>
#include <cstdio>

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <cxxabi.h>
#include <execinfo.h>
#endif

namespace
{
QString g_logsDir;
QString g_version;

QString signalName(int sig)
{
    switch (sig) {
    case SIGSEGV:
        return QStringLiteral("SIGSEGV");
    case SIGABRT:
        return QStringLiteral("SIGABRT");
    case SIGILL:
        return QStringLiteral("SIGILL");
    case SIGFPE:
        return QStringLiteral("SIGFPE");
#ifdef SIGBUS
    case SIGBUS:
        return QStringLiteral("SIGBUS");
#endif
    default:
        return QStringLiteral("SIG%1").arg(sig);
    }
}

QString captureStack()
{
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    void *frames[64];
    const int n = ::backtrace(frames, 64);
    char **syms = ::backtrace_symbols(frames, n);
    if (!syms)
        return QStringLiteral("(backtrace_symbols falló)\n");
    QString out;
    for (int i = 0; i < n; ++i) {
        QString line = QString::fromLatin1(syms[i]);
        const int p1 = line.indexOf(u'(');
        const int p2 = line.indexOf(u'+', p1);
        if (p1 >= 0 && p2 > p1) {
            const QByteArray mangled = line.mid(p1 + 1, p2 - p1 - 1).toLatin1();
            int status = 0;
            char *dem = abi::__cxa_demangle(mangled.constData(), nullptr, nullptr, &status);
            if (status == 0 && dem) {
                line.replace(QString::fromLatin1(mangled), QString::fromLatin1(dem));
                ::free(dem);
            }
        }
        out += QStringLiteral("#%1 %2\n").arg(i).arg(line);
    }
    ::free(syms);
    return out;
#else
    return QStringLiteral("(stack no disponible en esta plataforma)\n");
#endif
}
} // namespace

QString CrashHandler::crashDir(const QString &logsDir)
{
    return logsDir.trimmed() + QStringLiteral("/crashes");
}

QString CrashHandler::writeCrashReport(int signalNumber, const QString &logsDir,
                                       const QString &appVersion, const QString &stackTrace)
{
    const QString dir = crashDir(logsDir);
    if (logsDir.trimmed().isEmpty() || !QDir().mkpath(dir))
        return {};
    const QString path
        = dir + QStringLiteral("/crash_")
          + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"))
          + QStringLiteral("_") + QString::number(QCoreApplication::applicationPid())
          + QStringLiteral(".log");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return {};
    QTextStream out(&f);
    out << "app=QtSalesSystem\n";
    out << "version=" << appVersion << "\n";
    out << "qt=" << QString::fromLatin1(qVersion()) << "\n";
    out << "signal=" << signalName(signalNumber) << "\n";
    out << "ts=" << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << "\n";
    out << "outbox=preservada (SQLite/WAL persistente; reintento al reiniciar)\n";
    out << "--- stack ---\n";
    out << (stackTrace.isEmpty() ? captureStack() : stackTrace);
    return path;
}

void CrashHandler::onSignal(int sig)
{
    // No tocar la BD desde aquí: el outbox ya está en disco por WAL.
    const QString path = writeCrashReport(sig, g_logsDir, g_version);
    fprintf(stderr, "QtSalesSystem crash reporte: %s\n", qPrintable(path));
    fflush(stderr);
    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

void CrashHandler::install(const QString &logsDir, const QString &appVersion)
{
    g_logsDir = logsDir.trimmed();
    g_version = appVersion;
    if (g_logsDir.isEmpty())
        return;
    QDir().mkpath(crashDir(g_logsDir));
    ::signal(SIGSEGV, CrashHandler::onSignal);
    ::signal(SIGABRT, CrashHandler::onSignal);
    ::signal(SIGILL, CrashHandler::onSignal);
    ::signal(SIGFPE, CrashHandler::onSignal);
#ifdef SIGBUS
    ::signal(SIGBUS, CrashHandler::onSignal);
#endif
}
