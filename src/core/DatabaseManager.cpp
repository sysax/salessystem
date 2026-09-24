#include "DatabaseManager.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
{
}

bool DatabaseManager::initialize(const QString &customPath)
{
    if (m_db.isOpen())
        return true;

    m_dbPath = customPath;
    if (m_dbPath.isEmpty()) {
        const QString dir =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        m_dbPath = dir + QStringLiteral("/sistema_ventas.db");
    }

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), "sales");
    m_db.setDatabaseName(m_dbPath);
    // Timeout 10 s como en data/db.py::get_conn (timeout=10)
    m_db.setConnectOptions(QStringLiteral("QSQLITE_BUSY_TIMEOUT=10000"));

    if (!m_db.open()) {
        m_status = QStringLiteral("No se pudo abrir: ") + m_db.lastError().text();
        emit openChanged();
        return false;
    }

    if (!applySqlFile(QStringLiteral(":/sql/schema.sql"), QStringLiteral("sql/schema.sql"))
        || !migrateLegacyColumns() || !ensureSeeded()) {
        emit openChanged();
        return false;
    }

    m_status = QStringLiteral("OK: ") + m_dbPath;
    emit openChanged();
    return true;
}

void DatabaseManager::close()
{
    if (m_db.isOpen())
        m_db.close();
    m_status = QStringLiteral("Cerrada");
    emit openChanged();
}

bool DatabaseManager::isOpen() const
{
    return m_db.isValid() && m_db.isOpen();
}

int DatabaseManager::tableRowCount(const QString &table) const
{
    static const QStringList kAllowed = {
        QStringLiteral("users"),        QStringLiteral("products"),
        QStringLiteral("clients"),      QStringLiteral("suppliers"),
        QStringLiteral("sales"),        QStringLiteral("sale_items"),
        QStringLiteral("purchases"),    QStringLiteral("inventory_movements"),
        QStringLiteral("payables"),     QStringLiteral("payments_cxc"),
        QStringLiteral("payments_cxp"),         QStringLiteral("promos"),
        QStringLiteral("audit_log"),    QStringLiteral("caja"),
        QStringLiteral("counters"),     QStringLiteral("outbox"),
        QStringLiteral("settings"),     QStringLiteral("recovery_tokens"),
        QStringLiteral("categories"),   QStringLiteral("serials"),
    };
    if (!kAllowed.contains(table))
        return -1;
    QSqlQuery q(m_db);
    q.exec(QStringLiteral("SELECT COUNT(*) FROM \"%1\"").arg(table));
    return q.next() ? q.value(0).toInt() : -1;
}

bool DatabaseManager::applySqlFile(const QString &resourcePath, const QString &diskFallback)
{
    // El .sql va embebido en el .qrc; fallback al archivo en disco para
    // desarrollo sin recompilar recursos.
    QString sql;
    QFile res(resourcePath);
    if (res.open(QIODevice::ReadOnly | QIODevice::Text)) {
        sql = QString::fromUtf8(res.readAll());
    } else {
        QFile disk(diskFallback);
        if (disk.open(QIODevice::ReadOnly | QIODevice::Text))
            sql = QString::fromUtf8(disk.readAll());
    }
    if (sql.isEmpty()) {
        m_status = diskFallback + QStringLiteral(" no encontrado");
        return false;
    }

    // QSqlQuery no ejecuta multi-sentencia: quitar comentarios de línea
    // primero y luego partir por ';' (un bloque de comentarios inicial no
    // debe fusionarse con la primera sentencia).
    QStringList codeLines;
    for (const QString &ln : sql.split(QLatin1Char('\n'))) {
        if (!ln.trimmed().startsWith(QLatin1String("--")))
            codeLines << ln;
    }
    QSqlQuery q(m_db);
    for (const QString &raw : codeLines.join(QLatin1Char('\n')).split(QLatin1Char(';'))) {
        const QString stmt = raw.trimmed();
        if (stmt.isEmpty())
            continue;
        if (!q.exec(stmt)) {
            m_status = QStringLiteral("sql: ") + q.lastError().text();
            return false;
        }
    }
    return true;
}

bool DatabaseManager::applySeedFile(const QString &name)
{
    // Nombres válidos: letras minúsculas (coincide con business_type).
    if (name.trimmed().isEmpty()
        || !QRegularExpression(QStringLiteral("^[a-z]+$")).match(name.trimmed()).hasMatch()) {
        m_status = QStringLiteral("seed inválido: ") + name;
        emit openChanged();
        return false;
    }
    const QString clean = name.trimmed();
    const bool ok = applySqlFile(QStringLiteral(":/sql/seeds/%1.sql").arg(clean),
                                 QStringLiteral("sql/seeds/%1.sql").arg(clean));
    emit openChanged();
    return ok;
}

bool DatabaseManager::ensureSeeded()
{
    // Réplica de _seed_if_empty (data/db.py): solo siembra con BD virgen.
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM users")) || !q.next())
        return false;
    if (q.value(0).toInt() > 0)
        return true;
    // Base limpia: solo admin (seed.sql y seed_min.sql siembran lo mismo).
    if (qgetenv("QTSALES_SIN_DEMO") == QByteArrayLiteral("1"))
        return applySqlFile(QStringLiteral(":/sql/seed_min.sql"), QStringLiteral("sql/seed_min.sql"));
    return applySqlFile(QStringLiteral(":/sql/seed.sql"), QStringLiteral("sql/seed.sql"));
}

bool DatabaseManager::migrateLegacyColumns()
{
    // Réplica de _migrate_users / _migrate_products (data/db.py):
    // ALTER TABLE ADD COLUMN solo si falta. No hashea aquí: el hash
    // PBKDF2 vive en AuthService (fase 2).
    struct Column {
        const char *table;
        const char *name;
        const char *definition; // "NAME TYPE DEFAULT ..."
    };
    static constexpr Column kColumns[] = {
        {"users", "active", "active INTEGER DEFAULT 1"},
        {"users", "failed_attempts", "failed_attempts INTEGER DEFAULT 0"},
        {"users", "locked_until", "locked_until TEXT DEFAULT NULL"},
        {"users", "created_at", "created_at TEXT DEFAULT NULL"},
        {"users", "last_login", "last_login TEXT DEFAULT NULL"},
        {"users", "totp_secret", "totp_secret TEXT DEFAULT NULL"},
        {"users", "totp_enabled", "totp_enabled INTEGER DEFAULT 0"},
        {"users", "recovery_json", "recovery_json TEXT DEFAULT '[]'"},
        {"users", "must_change_password", "must_change_password INTEGER DEFAULT 0"},
        {"products", "image", "image TEXT DEFAULT NULL"},
        {"products", "lote", "lote TEXT DEFAULT NULL"},
        {"products", "vencimiento", "vencimiento TEXT DEFAULT NULL"},
        {"products", "is_kit", "is_kit INTEGER DEFAULT 0"},
        {"products", "kit_json", "kit_json TEXT DEFAULT '[]'"},
        {"sales", "tax_breakdown", "tax_breakdown TEXT DEFAULT ''"}, // Fase 1: desglose por tasa
        {"products", "attrs_json", "attrs_json TEXT DEFAULT '{}'"}, // Fase 3: metadatos vertical
        {"sale_items", "attrs_json", "attrs_json TEXT DEFAULT '{}'"},
        {"sale_items", "serial", "serial TEXT DEFAULT ''"},
    };
    for (const Column &c : kColumns) {
        if (!ensureColumn(QString::fromLatin1(c.table),
                          QString::fromLatin1(c.name),
                          QString::fromLatin1(c.definition)))
            return false;
    }
    return true;
}

bool DatabaseManager::ensureColumn(const QString &table, const QString &column,
                                   const QString &definition)
{
    QSqlQuery pragma(m_db);
    pragma.exec(QStringLiteral("PRAGMA table_info(\"%1\")").arg(table));
    while (pragma.next()) {
        if (pragma.value(1).toString() == column)
            return true; // ya existe
    }
    QSqlQuery alter(m_db);
    if (!alter.exec(
            QStringLiteral("ALTER TABLE \"%1\" ADD COLUMN %2").arg(table, definition))) {
        m_status = QStringLiteral("migrate: ") + alter.lastError().text();
        return false;
    }
    return true;
}
