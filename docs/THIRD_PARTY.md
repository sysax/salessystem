# Componentes third-party y licencias (Fase 8)

Revisión de cumplimiento al cerrar Fase 8 (2026-09-28).

| Componente | Versión | Licencia | Uso | Cumplimiento |
| :--- | :--- | :--- | :--- | :--- |
| Qt 6 (Core, Gui, Quick, QuickControls2, Sql, Network, Concurrent, Test, LinguistTools) | 6.11.2 | LGPL-3.0 / Comercial opcional | Framework app + tests + i18n | Enlace dinámico (instaladores aqt); sin modificar Qt; aviso LGPL en Acerca de / RELEASE |
| SQLite (vía QSQLITE) | embebido en Qt | Public domain | Persistencia | Sin acción |
| Compilador GCC/Clang/MSVC | según CI | GPL con excepción runtime | Solo build | Sin distribución de toolchain |
| clang-format / cmake-format / gcovr (CI) | 23.1.1 / 0.6.13 | Varias (solo dev) | Higiene, no se distribuyen | Sin acción |

## Notas

- El proyecto usa Qt bajo **LGPL-3.0** (binarios oficiales, enlace dinámico):
  el usuario puede sustituir las librerías Qt. Si se distribuye bajo
  licencia comercial de Qt, quitar este aviso y guardar la prueba de licencia.
- `sql/*`, `qml/*`, `src/*` son código propio (sin licencia third-party
  incrustada). No hay vendoring de librerías externas.
- Instaladores: CPack empaqueta solo el binario + QML; Qt se obtiene del
  entorno o se despliega con `windeployqt`/`macdeployqt` (dinámico, LGPL ok).
