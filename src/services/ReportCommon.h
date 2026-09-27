#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

#include "../core/Money.h"
#include "SettingsService.h"

// Helpers compartidos por ReportFinance/ReportOps/ReportInventory
// (antes static/private en ReportService.cpp). Sin comportamiento propio:
// las tres clases conservan firmas y consultas idénticas.
namespace ReportCommon
{
// Multitienda: rubro efectivo (param explícito manda; si no, activo).
// Vacío o 'miscelanea' = sin filtro.
inline QString effectiveBt(SettingsService *settings, const QString &businessType)
{
    QString bt = businessType.trimmed();
    if (bt.isEmpty() && settings)
        bt = settings->businessType().trimmed();
    if (bt == QLatin1String("miscelanea"))
        return {};
    return bt;
}

// Multitienda: predicado sobre el alias de products dado ('' = legacy
// visible en todos). Retorna "" si no hay filtro; el llamador enlaza bt.
inline QString btPred(const QString &alias, const QString &bt)
{
    if (bt.isEmpty())
        return {};
    return QStringLiteral(" AND (%1.business_type IS NULL OR %1.business_type='' OR "
                          "%1.business_type=?)")
        .arg(alias);
}

inline double scalar(QSqlDatabase db, const QString &sql)
{
    QSqlQuery q(db);
    if (!q.exec(sql) || !q.next())
        return 0.0;
    return q.value(0).toDouble();
}

// Frontera SQL: la BD guarda REAL; el dominio calcula en Money (céntimos).
// Solo importes de dinero usan moneyScalar; qty/conteos/tasas % siguen en double.
inline Money moneyScalar(QSqlDatabase db, const QString &sql)
{
    QSqlQuery q(db);
    if (!q.exec(sql) || !q.next())
        return Money();
    return Money::fromCop(q.value(0).toDouble());
}

inline QString normCat(const QString &cat)
{
    const QString c = cat.trimmed();
    return c.isEmpty() ? QStringLiteral("General") : c;
}

inline QString setting(QSqlDatabase db, const QString &key, const QString &fallback = {})
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT value FROM settings WHERE key=?"));
    q.addBindValue(key);
    if (!q.exec() || !q.next())
        return fallback;
    return q.value(0).toString();
}
} // namespace ReportCommon
