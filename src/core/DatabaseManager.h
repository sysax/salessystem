#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>

// Capa Repositories (arquitectura.txt): acceso a datos.
// DatabaseManager abre/crea el SQLite, aplica sql/schema.sql por
// sentencias y ejecuta las migraciones de columnas heredadas de
// data/db.py (_migrate_users/_migrate_products). No contiene SQL de
// negocio: eso vive en cada *Repository (fase 3).
class DatabaseManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
    Q_PROPERTY(QString dbPath READ dbPath CONSTANT)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY openChanged)

public:
    explicit DatabaseManager(QObject *parent = nullptr);

    // Abre (o crea + inicializa) la base. customPath vacío =>
    // QStandardPaths::AppDataLocation/sistema_ventas.db
    Q_INVOKABLE bool initialize(const QString &customPath = QString());
    Q_INVOKABLE void close();

    bool isOpen() const;
    QString dbPath() const { return m_dbPath; }
    QString statusMessage() const { return m_status; }
    QSqlDatabase database() const { return m_db; }

    // Utilidad fase 1: conteo de filas por tabla para verificar el seed.
    Q_INVOKABLE int tableRowCount(const QString &table) const;
    // Fase 2: aplica sql/seeds/<name>.sql (INSERT OR IGNORE, idempotente).
    // Solo añade: no borra usuarios, ventas ni productos existentes.
    Q_INVOKABLE bool applySeedFile(const QString &name);

signals:
    void openChanged();

private:
    bool applySqlFile(const QString &resourcePath, const QString &diskFallback);
    bool migrateLegacyColumns();
    bool ensureSeeded(); // aplica sql/seed.sql solo si users está vacía
    bool ensureColumn(const QString &table, const QString &column,
                      const QString &definition);

    QSqlDatabase m_db;
    QString m_dbPath;
    QString m_status;
};
