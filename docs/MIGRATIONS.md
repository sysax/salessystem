# Migraciones de BD (Fase 8)

La versión del esquema vive en `PRAGMA user_version` (entero SQLite).

- Actual: `AppVersion::kSchemaVersion = 8` (== Fase 8).
- `DatabaseManager::initialize()` aplica en orden:
  1. `sql/schema.sql` (idempotente, `CREATE TABLE IF NOT EXISTS`).
  2. `migrateLegacyColumns()` (columnas aditivas de Fases 1–6).
  3. `migrateCategoriesFk()` (reconstrucción Fase 1).
  4. `AppMetrics::ensureTable()` (tabla `app_metrics`).
  5. `ensureVersioned()` (sella/valida `user_version`).

## Reglas

| Caso | Comportamiento |
| :--- | :--- |
| `user_version == 0` (legada o fresca) | Corre migraciones históricas y sella a 8 |
| `0 < v < 8` | Corre migraciones históricas (idempotentes) y sella a 8 |
| `v == 8` | Sin cambios |
| `v > 8` (BD de app futura) | `initialize()` **falla** con "BD de versión futura": actualizar la app |

## Upgrade vN → vN+1 (procedimiento probado)

```bash
cp sistema_ventas.db /tmp/copia_produccion.db
QTSALES_DB=/tmp/copia_produccion.db ./build/qtsales  # migra la copia
# Verificar: PRAGMA user_version; debe ser 8 y la app abre en OK.
sqlite3 /tmp/copia_produccion.db "PRAGMA user_version;"
```

Nunca migrar el original sin respaldo (`DatabaseManager::backup()` /
página Respaldos).

## Downgrade

No hay downgrade automático: una BD en v8 abierta con una app v7
quedaría ilegible para features nuevas. Procedimiento documentado:

1. Restaurar el respaldo pre-upgrade (`respaldos/respaldo_*.db`).
2. Re-aplicar solo operaciones posteriores desde tickets/export CSV.
3. Los datos creados por módulos superiores quedan intactos (nunca se
   borran columnas), solo ocultos para la app vieja.
