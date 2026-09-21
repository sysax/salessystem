#include "Dian.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QSqlQuery>

namespace Dian
{
namespace
{
QString setting(QSqlDatabase db, const QString &key, const QString &fallback)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT value FROM settings WHERE key=?"));
    q.addBindValue(key);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return fallback;
}
} // namespace

bool isEnabled(QSqlDatabase db)
{
    return setting(db, QStringLiteral("dian_enabled"), QStringLiteral("0")) == QLatin1String("1");
}

QString provider(QSqlDatabase db)
{
    return setting(db, QStringLiteral("dian_provider"), QStringLiteral("simulado"));
}

QString defaultDocType(QSqlDatabase db)
{
    return isEnabled(db) ? QStringLiteral("Factura electrónica DIAN")
                         : QStringLiteral("Ticket de venta");
}

QString generateCufe(QSqlDatabase db, const QString &folio)
{
    // CUFE simulado con formato estable: SHA-1 de los campos clave
    // (en modo real: SHA-1 del XML UBL según resolución DIAN).
    Q_UNUSED(db);
    const QString raw = folio + u'|'
        + QDateTime::currentDateTime().toString(Qt::ISODateWithMs).left(19);
    return QString::fromLatin1(
        QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Sha1).toHex());
}

} // namespace Dian
