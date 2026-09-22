# Sistema de Ventas — Qt6/QML + C++20 (Colombia, COP)

Aplicación de escritorio para punto de venta y gestión comercial: catálogo,
inventario, POS con pagos mixtos, compras, CxC/CxP, reportes con KPIs,
usuarios con 2FA TOTP y modo offline-first con sincronización.

Migrado desde KivyMD/Python (ver historial git) siguiendo `arquitectura.txt`:

```
qml/ (Pages/Components) → src/controllers → src/services
                        → src/repositories → SQLite
```

## Módulos

| # | Módulo | QML | Servicio |
|---|--------|-----|----------|
| 1 | Usuarios y Roles (PBKDF2, lockout, bitácora, 2FA TOTP) | `UsersPage` | `AuthService` |
| 2 | Catálogo (ABM, lotes, kits, barcodes) | `ProductsPage` | `ProductRepository` |
| 3 | Clientes CRM (NIT, crédito, descuentos) | `ClientsPage` | `ClientRepository` |
| 4 | Proveedores | `SuppliersPage` | `SupplierRepository` |
| 5 | Inventario (entradas/salidas/transferencias, valorizado, alertas) | `InventoryPage` | `InventoryService` |
| 6 | POS (pagos mixtos, promos, ticket, caja, offline) | `PosPage` | `SalesService` |
| 7 | Ventas y documentos (6 tipos, 6 estados + Cancelada) | `SalesPage` | `SaleRepository` |
| 8 | Cuentas por Cobrar (abonos, mora 2 %) | `ReceivablesPage` | `ReceivablesService` |
| 9 | Cuentas por Pagar (pronto pago) | `PayablesPage` | `PayablesService` |
| 10 | Reportes (operativos, financieros, KPIs, CSV/PDF) | `ReportsPage` | `ReportService` |
| 11 | Compras (OC → recepción → CxP automática) | `PurchasesPage` | `PurchaseService` |
| 12 | Promociones (7 tipos) | `PromosPage` | `PromoRepository` |

Extras: Dashboard estilo GesNet, `DIAN OFF` por defecto (documentos internos),
tickets siempre en `.txt`, cola `outbox` idempotente.

## Requisitos

- Qt 6.x (probado 6.11.2) con Quick, QuickControls2, Sql, Network
- CMake 3.28+, compilador C++20 (g++ 13+)

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.11.2/gcc_64
cmake --build build -j$(nproc)
ctest --test-dir build   # 11 suites QtTest
QT_QUICK_CONTROLS_STYLE=Material ./build/qtsales
```

La base SQLite se crea/siembra sola en `AppDataLocation/sistema_ventas.db`
(`QTSALES_DB=<ruta>` para usar otra; el esquema es compatible con la BD
del sistema Python anterior).

```bash
# Probar con BD vacía (solo esquema): se siembra con demo + usuarios
QTSALES_DB=/tmp/ventas_vacia.db ./build/qtsales
# Probar sin datos demo (solo los 5 usuarios, cero productos/ventas)
QTSALES_DB=/tmp/ventas_cero.db QTSALES_SIN_DEMO=1 ./build/qtsales
```

## Usuarios de prueba

| Usuario | Clave | Rol |
|---------|-------|-----|
| admin | admin123 | Administrador |
| vendedor | venta123 | Vendedor |
| cajero | caja123 | Cajero |
| almacen | alma123 | Almacén |
| contador | conta123 | Contador |

## Estructura

```
├── CMakeLists.txt          # qt_add_executable + qt_add_qml_module, C++20
├── src/
│   ├── main.cpp            # DI manual + contexto QML
│   ├── core/               # DatabaseManager, EventBus, Money, Result
│   ├── domain/Entities.h   # 14 structs 1:1 con SQLite
│   ├── repositories/       # 11 repos QSql (sin SQL en UI)
│   ├── services/           # Auth, Sales, Inventory, Purchase, Report,
│   │                       #  Sync, Credit, Dian, TicketPrinter, Totp
│   └── controllers/        # 13 controllers Q_PROPERTY/Q_INVOKABLE
├── qml/                    # Main + 14 páginas + sidebar, Material
├── sql/schema.sql + seed.sql (embebidos en el binario vía .qrc)
├── tests/                  # 11 suites QtTest (ctest)
└── fases.md                # Especificación original de los 12 módulos
```

## Notas

- Moneda COP, IVA 19 %, NIT.
- Offline-first: las ventas siempre se guardan local; `outbox` sincroniza
  al reconectar (idempotente por clave).
- Sin impresora, los tickets se guardan como `.txt` (y se intenta `lp`).
- Desviaciones del Python original (bugs no replicados): notas
  crédito/cargo guardan el motivo en bitácora (el `UPDATE sales SET
  reason/ref` original referenciaba columnas inexistentes); el IVA por
  línea se interpreta desde el texto (`"IVA 19%"`→19) en vez de
  `Decimal("IVA 19%")` que fallaba.
