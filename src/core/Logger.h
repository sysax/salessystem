#pragma once

#include <QMutex>
#include <QString>

// Fase 8: logging estructurado con niveles y rotación.
// Uso: Logger::init(dir) al arrancar (main.cpp), luego qInfo/qWarning
// habituales. El handler escribe a archivo + stderr con patrón:
//   <ts> [nivel] [sale:<id>] <categoría> <archivo:línea> — mensaje
// Correlación venta↔logs: Logger::setSaleContext(id) en el hilo que
// procesa la venta (o Logger::infoSale(id, msg) puntual).
class Logger
{
  public:
    // dir vacío = no hace nada (tests usan QTemporaryDir explícito).
    // maxBytes por archivo (default 1 MiB), keep archivos rotados (default 5).
    static void init(const QString &dir, qint64 maxBytes = 1024 * 1024, int keep = 5);
    static void shutdown();
    static bool isActive();
    static QString logDir();
    static QString logFile();

    // Contexto de venta para el hilo actual (thread-local).
    static void setSaleContext(const QString &saleId);
    static QString saleContext();
    static void clearSaleContext();

    // Atajo puntual sin tocar el contexto del hilo.
    static void infoSale(const QString &saleId, const QString &message);
    static void warnSale(const QString &saleId, const QString &message);

  private:
    static void handler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg);
    static void rotateIfNeeded();
    static QMutex &mutex();
};
