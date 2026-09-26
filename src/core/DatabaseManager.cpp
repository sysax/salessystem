#include "DatabaseManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

#include "Transaction.h"

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent)
{
}

bool DatabaseManager::initialize(const QString &customPath)
{
    if (m_db.isOpen())
        return true;

    m_dbPath = customPath;
    if (m_dbPath.isEmpty()) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
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

    // Fase 1 (MAP_PRO.md): integridad transaccional de SQLite.
    // foreign_keys evita huérfanos, WAL permite lector+escritor
    // concurrente (caja vendiendo + reporte leyendo) y busy_timeout
    // hace que los escritores esperen en vez de fallar con SQLITE_BUSY.
    {
        QSqlQuery pragma(m_db);
        const char *kPragmas[] = {
            "PRAGMA foreign_keys = ON",
            "PRAGMA journal_mode = WAL",
            "PRAGMA synchronous = NORMAL",
            "PRAGMA busy_timeout = 5000",
        };
        for (const char *p : kPragmas) {
            if (!pragma.exec(QString::fromLatin1(p))) {
                m_status = QStringLiteral("pragma: ") + pragma.lastError().text();
                emit openChanged();
                return false;
            }
        }
    }

    if (!applySqlFile(QStringLiteral(":/sql/schema.sql"), QStringLiteral("sql/schema.sql"))
        || !migrateLegacyColumns() || !migrateCategoriesFk() || !ensureSeeded()) {
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
        QStringLiteral("payments_cxp"), QStringLiteral("promos"),
        QStringLiteral("audit_log"),    QStringLiteral("caja"),
        QStringLiteral("counters"),     QStringLiteral("outbox"),
        QStringLiteral("settings"),     QStringLiteral("recovery_tokens"),
        QStringLiteral("categories"),   QStringLiteral("serials"),
    };
    if (!kAllowed.contains(table))
        return -1;
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("SELECT COUNT(*) FROM \"%1\"").arg(table)) || !q.next())
        return -1;
    return q.value(0).toInt();
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
        return applySqlFile(QStringLiteral(":/sql/seed_min.sql"),
                            QStringLiteral("sql/seed_min.sql"));
    return applySqlFile(QStringLiteral(":/sql/seed.sql"), QStringLiteral("sql/seed.sql"));
}

bool DatabaseManager::migrateLegacyColumns()
{
    // Réplica de _migrate_users / _migrate_products (data/db.py):
    // ALTER TABLE ADD COLUMN solo si falta. No hashea aquí: el hash
    // PBKDF2 vive en AuthService (fase 2).
    struct Column
    {
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
        {"sales", "business_type",
         "business_type TEXT DEFAULT ''"}, // Multitienda: rubro de la venta ('' = mixta/legacy)
        {"products", "attrs_json", "attrs_json TEXT DEFAULT '{}'"}, // Fase 3: metadatos vertical
        {"products", "business_type",
         "business_type TEXT DEFAULT ''"}, // Multitienda: rubro dueño, filtrar sin borrar
        {"promos", "business_type",
         "business_type TEXT DEFAULT ''"}, // Multitienda: promos por rubro ('' = todas)
        {"promos", "valid_from", "valid_from TEXT DEFAULT ''"}, // Fase 3: vigencia
        {"promos", "valid_to", "valid_to TEXT DEFAULT ''"},
        {"promos", "priority", "priority INTEGER DEFAULT 0"}, // Fase 3: prioridad
        {"promos", "max_uses", "max_uses INTEGER DEFAULT 0"}, // 0 = ilimitada
        {"promos", "uses", "uses INTEGER DEFAULT 0"},
        {"products", "stock_reserved", "stock_reserved REAL DEFAULT 0"}, // Fase 3: apartados
        {"outbox", "device_id", "device_id TEXT DEFAULT ''"}, // Fase 6: origen del evento
        {"outbox", "seq", "seq INTEGER DEFAULT 0"},
        {"audit_log", "entity", "entity TEXT DEFAULT ''"}, // Fase 5: auditoría estructurada
        {"audit_log", "entity_id", "entity_id TEXT DEFAULT ''"},
        {"audit_log", "before_json", "before_json TEXT DEFAULT ''"},
        {"audit_log", "after_json", "after_json TEXT DEFAULT ''"},
        {"sale_items", "attrs_json", "attrs_json TEXT DEFAULT '{}'"},
        {"sale_items", "serial", "serial TEXT DEFAULT ''"},
    };
    for (const Column &c : kColumns) {
        if (!ensureColumn(QString::fromLatin1(c.table), QString::fromLatin1(c.name),
                          QString::fromLatin1(c.definition)))
            return false;
    }
    // Fase 3: movimientos de caja normalizados (tabla aditiva).
    QSqlQuery mv(m_db);
    if (!mv.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS caja_movimientos (id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "ts TEXT, turno TEXT, tipo TEXT, monto REAL, metodo_pago TEXT, sale_id TEXT, user_id "
            "TEXT)"))) {
        m_status = QStringLiteral("migrate: ") + mv.lastError().text();
        return false;
    }
    // Fase 4: soltar índices redundantes (el UNIQUE ya crea autoindex).
    // barcode se conserva: no es UNIQUE y el escáner POS lo consulta.
    QSqlQuery drop(m_db);
    for (const QString &idx :
         {QStringLiteral("idx_products_sku"), QStringLiteral("idx_clients_name")}) {
        if (!drop.exec(QStringLiteral("DROP INDEX IF EXISTS %1").arg(idx))) {
            m_status = QStringLiteral("migrate: ") + drop.lastError().text();
            return false;
        }
    }
    return true;
}

bool DatabaseManager::migrateCategoriesFk()
{
    // Fase 1: el schema histórico declaraba
    // `parent_id INTEGER REFERENCES categories(id)`, incompatible con la
    // convención 0=raíz (con foreign_keys=ON, insertar raíces falla con
    // "FOREIGN KEY constraint failed"). CREATE TABLE IF NOT EXISTS no
    // toca tablas ya creadas: si la tabla existente trae esa cláusula,
    // se reconstruye idéntica pero sin el REFERENCES (datos intactos).
    bool hasFk = false;
    {
        // Alcance propio: el PRAGMA debe finalizarse antes del ALTER
        // (una lectura abierta sobre la tabla bloquea el RENAME con
        // SQLITE_LOCKED en la misma conexión).
        QSqlQuery fk(m_db);
        if (!fk.exec(QStringLiteral("PRAGMA foreign_key_list(categories)")))
            return false;
        hasFk = fk.next();
    }
    if (!hasFk)
        return true; // esquema nuevo o sin FK: nada que migrar
    Transaction tx(m_db);
    if (!tx.isValid())
        return false;
    static constexpr const char *kSteps[] = {
        "ALTER TABLE categories RENAME TO categories_legacy_fase1",
        "CREATE TABLE categories (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, "
        "parent_id INTEGER DEFAULT 0, business_type TEXT DEFAULT '', sort_order INTEGER DEFAULT "
        "0, UNIQUE(name, parent_id))",
        "INSERT INTO categories (id, name, parent_id, business_type, sort_order) SELECT id, name, "
        "parent_id, business_type, sort_order FROM categories_legacy_fase1",
        "DROP TABLE categories_legacy_fase1",
    };
    QSqlQuery q(m_db);
    for (const char *step : kSteps) {
        if (!q.exec(QString::fromLatin1(step))) {
            m_status = QStringLiteral("migrate categories: ") + q.lastError().text();
            return false;
        }
    }
    return tx.commit();
}

bool DatabaseManager::ensureColumn(const QString &table, const QString &column,
                                   const QString &definition)
{
    QSqlQuery pragma(m_db);
    if (!pragma.exec(QStringLiteral("PRAGMA table_info(\"%1\")").arg(table)))
        return false;
    while (pragma.next()) {
        if (pragma.value(1).toString() == column)
            return true; // ya existe
    }
    QSqlQuery alter(m_db);
    if (!alter.exec(QStringLiteral("ALTER TABLE \"%1\" ADD COLUMN %2").arg(table, definition))) {
        m_status = QStringLiteral("migrate: ") + alter.lastError().text();
        return false;
    }
    return true;
}

QVariantMap DatabaseManager::backup(const QString &dir, int keep)
{
    // Fase 5: VACUUM INTO copia consistente en caliente (SQLite ≥ 3.27;
    // falla dentro de transacción: las operaciones aquí son puntuales).
    QString target = dir.trimmed();
    if (target.isEmpty()) {
        target = QFileInfo(m_dbPath).absolutePath() + QStringLiteral("/respaldos");
    }
    if (!QDir().mkpath(target))
        return {{"ok", false}, {"error", QStringLiteral("No se pudo crear ") + target}};
    const QString path
        = target + QStringLiteral("/respaldo_")
          + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"))
          + QStringLiteral(".db");
    // El nombre va entre comillas simples con escape (no admite binds).
    const QString quoted = QStringLiteral("'") + QString(path).replace(u'\'', QStringLiteral("''"))
                           + QStringLiteral("'");
    QSqlQuery q(m_db);
    if (!q.exec(QStringLiteral("VACUUM INTO ") + quoted))
        return {{"ok", false},
                {"error", QStringLiteral("Respaldo falló: ") + q.lastError().text()}};
    // Retención: conservar los `keep` más recientes (por modificación).
    int pruned = 0;
    if (keep > 0) {
        QFileInfoList files = QDir(target).entryInfoList({QStringLiteral("respaldo_*.db")},
                                                         QDir::Files, QDir::Time);
        while (files.size() > keep) {
            if (QFile::remove(files.takeLast().absoluteFilePath()))
                ++pruned;
            else
                break;
        }
    }
    m_status = QStringLiteral("Respaldo: ") + path;
    emit openChanged();
    return {{"ok", true}, {"path", path}, {"pruned", pruned}};
}
