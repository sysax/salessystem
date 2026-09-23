#pragma once

// Helper solo-tests: carga un .sql de fixtures (multi-sentencia, estilo
// DatabaseManager::applySqlFile) en la BD temporal del test. La app en
// producción siembra base limpia (solo admin); los tests que necesitan
// datos de negocio cargan tests/fixtures/seed_demo.sql explícitamente.
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

namespace TestDb
{
inline QString demoFixturePath()
{
    return QString::fromLatin1(TEST_FIXTURES_DIR) + QStringLiteral("/seed_demo.sql");
}

inline bool loadFixture(const QSqlDatabase &db, const QString &path, QString *error = nullptr)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = QStringLiteral("no se pudo abrir: ") + path;
        return false;
    }
    QStringList codeLines;
    for (const QString &ln : QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'))) {
        if (!ln.trimmed().startsWith(QLatin1String("--")))
            codeLines << ln;
    }
    QSqlQuery q(db);
    for (const QString &raw : codeLines.join(QLatin1Char('\n')).split(QLatin1Char(';'))) {
        const QString stmt = raw.trimmed();
        if (stmt.isEmpty())
            continue;
        if (!q.exec(stmt)) {
            if (error)
                *error = q.lastError().text() + QStringLiteral(" en: ") + stmt.left(80);
            return false;
        }
    }
    return true;
}

inline bool loadDemo(const QSqlDatabase &db, QString *error = nullptr)
{
    return loadFixture(db, demoFixturePath(), error);
}
} // namespace TestDb
