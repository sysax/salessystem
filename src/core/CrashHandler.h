#pragma once

#include <QString>

// Fase 8: crash handler global. Deja rastro (módulo + versión) y
// preserva la cola de sync: el outbox vive en SQLite/WAL persistente,
// así que no hay "flush" que hacer en el handler — basta con no
// corromper la BD (no escribir en ella desde el handler) y vaciar
// los logs. El reporte cae en <logsDir>/crashes/crash_*.log.
class CrashHandler
{
  public:
    static void install(const QString &logsDir, const QString &appVersion);
    // Escribe el reporte directamente (testeable sin estrellarse).
    // Retorna la ruta del archivo o "" en error.
    static QString writeCrashReport(int signalNumber, const QString &logsDir,
                                    const QString &appVersion, const QString &stackTrace = {});
    static QString crashDir(const QString &logsDir);

  private:
    static void onSignal(int sig);
};
