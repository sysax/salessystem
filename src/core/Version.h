#pragma once

#include <QString>

// Fase 8: versión única del producto. El número canónico vive en
// CMakeLists (project VERSION); este header lo refleja para crash
// reports, logs y "Acerca de". Al subir versión, cambiar en ambos.
namespace AppVersion
{
// Mantener sincronizado con `project(... VERSION x.y.z)` en CMakeLists.
inline constexpr char kVersion[] = "1.0.0";
inline constexpr char kName[] = "QtSalesSystem";
// Esquema SQLite (PRAGMA user_version). BDs legadas con user_version=0
// se migran por la vía histórica y luego se sellan con este valor.
// Subir en cada cambio de esquema con su migración en DatabaseManager.
inline constexpr int kSchemaVersion = 8;

inline QString versionString()
{
    return QString::fromLatin1(kVersion);
}
} // namespace AppVersion
