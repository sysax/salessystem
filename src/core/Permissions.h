#pragma once

#include <QString>
#include <QStringList>

// Fase 3 (MAP_PRO.md): matriz de permisos como única fuente de verdad en C++
// expuesta a QML vía AuthController. Header-only, sin Q_OBJECT.
namespace Permissions
{

inline QString kAdminRole()
{
    return QStringLiteral("Administrador");
}

inline QStringList roles()
{
    return {
        QStringLiteral("Administrador"), QStringLiteral("Vendedor"), QStringLiteral("Cajero"),
        QStringLiteral("Almacén"),       QStringLiteral("Contador"),
    };
}

inline QStringList screensForRole(const QString &role)
{
    if (role == QStringLiteral("Administrador"))
        return {QStringLiteral("*")};
    if (role == QStringLiteral("Vendedor"))
        return {QStringLiteral("dashboard"), QStringLiteral("products"), QStringLiteral("pos"),
                QStringLiteral("sales"),     QStringLiteral("clients"),
                QStringLiteral("inventory"), QStringLiteral("serials")};
    if (role == QStringLiteral("Cajero"))
        return {QStringLiteral("dashboard"), QStringLiteral("pos"), QStringLiteral("sales")};
    if (role == QStringLiteral("Almacén"))
        return {QStringLiteral("dashboard"), QStringLiteral("products"),
                QStringLiteral("inventory"), QStringLiteral("purchases"),
                QStringLiteral("suppliers"), QStringLiteral("lots"),
                QStringLiteral("serials")};
    if (role == QStringLiteral("Contador"))
        return {QStringLiteral("dashboard"),   QStringLiteral("sales"),
                QStringLiteral("receivables"), QStringLiteral("payables"),
                QStringLiteral("reports"),     QStringLiteral("purchases")};
    return {};
}

inline bool isAdmin(const QString &role)
{
    return role == kAdminRole();
}

inline bool canAccess(const QString &role, const QString &screen)
{
    const QStringList screens = screensForRole(role);
    return screens.contains(QStringLiteral("*")) || screens.contains(screen);
}

} // namespace Permissions
