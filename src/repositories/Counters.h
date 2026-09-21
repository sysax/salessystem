#pragma once

// Folios consecutivos — réplica de data/db.py::next_counter.
// Formato: prefijo + valor con ceros (V007, COT002, OC006). Misma
// semántica no-atómica que Python (lectura + reemplazo).
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

namespace Counters
{
inline QString next(QSqlDatabase db, const QString &name, const QString &prefix, int width = 3)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT value FROM counters WHERE name=?"));
    q.addBindValue(name);
    int val = 1;
    if (q.exec() && q.next())
        val = q.value(0).toInt();
    QSqlQuery up(db);
    up.prepare(QStringLiteral("INSERT OR REPLACE INTO counters (name, value) VALUES (?,?)"));
    up.addBindValue(name);
    up.addBindValue(val + 1);
    up.exec();
    return prefix + QString::number(val).rightJustified(width, u'0');
}

inline int get(QSqlDatabase db, const QString &name)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT value FROM counters WHERE name=?"));
    q.addBindValue(name);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}
} // namespace Counters
