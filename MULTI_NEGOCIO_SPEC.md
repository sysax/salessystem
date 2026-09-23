--- docs/MULTI_NEGOCIO_ESPECIFICACIONES.md (原始)


+++ docs/MULTI_NEGOCIO_ESPECIFICACIONES.md (修改后)
# Especificaciones: Sistema Multi-Negocio (Farmacia, Abarrotes, Celulares, Miscelánea, etc.)

**Estado:** Propuesta · **Fecha:** 2026-09-24
**Objetivo:** Eliminar supuestos hardcodeados del proyecto (Colombia/COP/IVA 19 %) y convertirlo en una plataforma parametrizable por tipo de negocio, sin romper la compatibilidad con bases de datos existentes.

Arquitectura de referencia (ver `arquitectura.txt`):
`qml/ → src/controllers → src/services → src/repositories → SQLite`
Todos los cambios respetan esta capa: **ningún dato de configuración se lee directamente desde QML**.

---

## Principio rector

Un "tipo de negocio" (vertical) NO es una bifurcación de código, sino un
**perfil de configuración** que activa/oculta funcionalidades ya genéricas:

```
business_type = farmacia | abarrotes | celulares | miscelanea | ferreteria | ...
```

Cada vertical define: categorías semilla, unidades de medida permitidas,
tax rates disponibles, campos extendidos obligatorios (lote/vencimiento, IMEI,
peso), y módulos visibles en el sidebar.

---

## Fase 1 — Configuración del Negocio (`SettingsService`)

### 1.1 Tabla `settings` (ya existe en `sql/schema.sql:97`)

Hoy solo contiene `dian_enabled` y `dian_provider`. Claves nuevas a sembrar
con valores por defecto (no requiere ALTER, es key/value):

| Clave | Ejemplo | Descripción |
|---|---|---|
| `business_name` | `Farmacia La Salud` | Aparece en tickets y reportes |
| `business_type` | `farmacia` | Vertical activa |
| `business_nit` / `business_tax_id` | `900123456-7` | Identificación fiscal |
| `business_address`, `business_phone` | | Datos del ticket |
| `business_logo_path` | `/.../logo.png` | Logo en ticket/reporte |
| `currency_code` | `COP`, `MXN`, `PEN` | ISO 4217 |
| `currency_symbol` | `$`, `S/` | Prefijo de Money |
| `currency_decimals` | `0` (COP) o `2` | Controla formato en `src/core/Money` |
| `default_tax_rate` | `19` | % usado al crear productos |
| `tax_rates_json` | `[{"name":"IVA 19%","rate":19},{"name":"Excluido","rate":0}]` | Lista editable de tasas |
| `weight_unit_default` | `g`, `kg`, `unidad` | Unidad sugerida para granel |
| `require_expiry` | `1` | Obliga lote+vencimiento en productos |
| `require_serial` | `1` | Obliga serial/IMEI en ventas |
| `mora_rate_monthly` | `2` | Reemplaza el 2 % fijo de CxC |

### 1.2 Nuevos archivos

- `src/repositories/SettingsRepository.{h,cpp}` — `get(key)`, `getAll()`, `set(key,value)` sobre la tabla `settings`.
- `src/services/SettingsService.{h,cpp}` — expone `Q_PROPERTY` (businessName, currencyCode, taxRates, flags de vertical) y `Q_INVOKABLE save(map)`. Emite señal `settingsChanged` vía `EventBus`.
- `src/controllers/SettingsController.{h,cpp}` — glue para la UI.
- `qml/SettingsPage.qml` — formulario agrupado: Negocio / Moneda e Impuestos / Vertical (checkboxes require_expiry, require_serial, venta a granel).
- Registrar el contexto en `src/main.cpp` y entrada en `qml/AppSidebar.qml` (solo rol admin).

### 1.3 Consumo en ticket y reportes

- `src/services/TicketPrinter.cpp`: reemplazar encabezado hardcodeado por `business_name`, NIT, dirección y logo (ticket `.txt`: ASCII; si hay impresora ESC/POS, imagen opcional). Desglose de impuestos dinámico desde `tax_rates_json` (agrupar ítems por tasa).
- `src/core/Money`: leer símbolo/decimales de `SettingsService` en vez de asumir COP.
- `ReportService`: cabeceras de PDF/CSV usan `business_name`.

**Criterio de aceptación:** cambiar nombre/NIT/moneda en SettingsPage refleja en nuevo ticket y en reporte exportado sin recompilar.

---

## Fase 2 — Catálogo Dinámico (categorías, unidades, decimales)

### 2.1 Categorías jerárquicas editables

Hoy `products.cat`/`subcat` son TEXT libres (`sql/schema.sql:14`).

- Tabla nueva:
  ```sql
  CREATE TABLE IF NOT EXISTS categories (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      name TEXT NOT NULL, parent_id INTEGER REFERENCES categories(id),
      business_type TEXT DEFAULT '', -- '' = aplica a todas
      sort_order INTEGER DEFAULT 0, UNIQUE(name, parent_id)
  );
  ```
- Migración aditiva en `DatabaseManager::migrateLegacyColumns()` (patrón existente `ALTER TABLE ... ADD COLUMN` / `CREATE TABLE IF NOT EXISTS`; nunca renombrar columnas).
- `CategoryRepository` + combo dependiente (cat → subcat) en `qml/ProductsPage.qml` y filtro en POS. Los valores viejos de `cat`/`subcat` siguen válidos (se leen como texto; la tabla es el diccionario, no FK estricta → compatibilidad retroactiva).

### 2.2 Cantidades decimales (venta por peso/granel)

- `sale_items.qty` es INTEGER (`schema.sql:47`) y `inventory_movements.qty` también. **SQLite es dinámico**: migración aditiva documentando cambio de semántica a REAL; en C++ usar `double` en structs de `src/domain/Entities.h`, repos y services.
- `products.unit` admite: `unidad, g, kg, ml, l, caja, paquete, metro`. Validar contra lista configurable por vertical.
- UI POS: teclado numérico con decimales solo si `unit ∉ {unidad, caja}`; mostrar precio por kg en fichas de granel.
- Redondeo: al facturar, qty real × precio; el total sigue con la regla de decimales de moneda.

### 2.3 Seeds por vertical

- `sql/seed.sql` actual → renombrar como seed genérico/demo.
- Nuevos archivos embebidos en el `.qrc`: `sql/seeds/farmacia.sql`, `abarrotes.sql`, `celulares.sql`, `miscelanea.sql`, `ferreteria.sql` (cada uno: categorías, tax rates, 5–10 productos de ejemplo, settings del vertical).
- En `SettingsPage`, botón **"Inicializar catálogo para este rubro"** ejecuta el seed correspondiente con `INSERT OR IGNORE` (idempotente). Primer arranque con BD virgen: asistente pregunta el tipo de negocio.

**Criterio de aceptación:** vender 0,350 kg de tomate y 2 cajas de aspireta en el mismo ticket; categorías aparecen según seed del vertical elegido.

---

## Fase 3 — Impuestos Flexibles

- `SalesService::parseTaxRate()` (`src/services/SalesService.cpp:19`) hoy interpreta `"IVA 19%"→19`. Mantener pero ampliar: la tasa válida se valida contra `tax_rates_json`; si el texto no está en la lista, se usa `default_tax_rate`.
- Alarma en ticket/reportes cuando una línea tiene tasa distinta a las configuradas (evita IVA fantasma heredado del bug Python).
- Soportar casos reales por vertical:
  - Farmacia: medicamentos exentos vs gravados (dos tasas en el mismo ticket).
  - Celulares/miscelánea: una sola tasa general.
  - Abarrotes: canasta básica excluida + procesados gravados.
- Desglose por tasa en `sales.tax` (guardar JSON `tax_breakdown` en columna nueva aditiva de `sales` o en `payments_json`-style field).

**Criterio de aceptación:** ticket mixto con líneas al 0 % y 19 % muestra base, impuesto y total correctos por tasa.

---

## Fase 4 — Campos Específicos por Vertical (metadatos flexibles)

### 4.1 Mecanismo común

Columna aditiva `attrs_json TEXT DEFAULT '{}'` en `products` y `sale_items`
(clave/valor libre interpretado por el service según `business_type`).
Alternativa normalizada si se requieren índices: tabla `product_attrs(product_id, key, value)`.

### 4.2 Farmacia

- Lote + vencimiento: ya existen (`products.lote`, `vencimiento`). Volverlos **obligatorios** si `require_expiry=1` (validación en `ProductRepository::save`).
- Alertas de vencimiento: vista en Dashboard/Inventory (próximos 30/60/90 días) + reporte CSV. Salida de inventario descuenta FIFO por fecha de vencimiento.
- Venta con receta: flag `requires_prescription` en `attrs_json`; POS pide confirmación de número de receta antes de agregar al carrito (log en `audit_log`).
- Droga controlada: segundo flag que exige usuario supervisor (reuso de roles de `AuthService`).

### 4.3 Tienda de celulares

- Serial/IMEI: con `require_serial=1`, el stock de ese SKU es implícitamente 1 por unidad; tabla nueva:
  ```sql
  CREATE TABLE IF NOT EXISTS serials (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      product_id INTEGER, sku TEXT, serial TEXT UNIQUE,
      status TEXT, -- in_stock | sold | rma | repaired
      sale_id TEXT, imei2 TEXT, notes TEXT
  );
  ```
- POS: escaneo/guardado de IMEI al agregar ítem; no permite vender más cantidad que seriales `in_stock`.
- Garantía: campo `warranty_months` en `attrs_json`; en `SalesPage`, botón "verificar garantía" por serial (fecha venta + meses).
- Accesorios: usan flujo normal (sin serial) — convive en el mismo ticket.

### 4.4 Abarrotes / miscelánea

- Granel cubierto por Fase 2.2 (decimales + unidades de peso).
- Precios por presentación: `price_wholesale` ya existe; agregar `price_list` por cliente (existe en `clients`) → resolver en `SalesService` al tomar precio.
- Código de barras EAN-13: `products.barcode` ya soporta; agregar validación de dígito verificador y foco de escáner en POS (hardware-ready).

**Criterio de aceptación:** en perfil farmacia no se puede guardar producto sin vencimiento; en perfil celulares no se venden 2 equipos con 1 serial disponible.

---

## Fase 5 — UI y Reportes Adaptativos

### 5.1 Sidebar condicional

`AppSidebar.qml` oculta módulos según `business_type` (context property):

| Módulo | farmacia | abarrotes | celulares | miscelanea |
|---|---|---|---|---|
| Productos/POS/Ventas/CxC-CxP/Reportes/Usuarios | ✔ | ✔ | ✔ | ✔ |
| Lotes y vencimientos | ✔ | – | – | – |
| Seriales/Garantías/RMA | – | – | ✔ | – |
| Compras/Promociones | ✔ | ✔ | ✔ | opcional |

Componente QML `components/BusinessBadge.qml` muestra rubro activo en dashboard.

### 5.2 Reportes específicos (`ReportService`)

- Genéricos (todos): top productos, margen, rotación por categoría, valor de inventario.
- Farmacia: **reporte de vencimientos** (30/60/90), medicamentos controlados vendidos.
- Celulares: **rotación por modelo/serie**, dispositivos en garantía abiertos, RMA.
- Abarrotes: rendimiento por kilo/unidad, mermas (usa `inventory_movements` type=merma).
- Todos exportables a CSV/PDF con el mecanismo actual.

### 5.3 Dashboard

Widgets configurables por vertical (KPIs actuales + alertas de vencimiento/serial bajo stock mínimo).

---

## Orden de implementación y esfuerzo

| # | Entrega | Archivos principales | Esfuerzo |
|---|---|---|---|
| 1 | SettingsService + SettingsPage + ticket dinámico | schema(seed keys), repositorio/servicio/controlador nuevos, `TicketPrinter`, `Money` | M |
| 2 | Tax rates dinámicos | `SalesService`, `TicketPrinter`, `ProductsPage` | S |
| 3 | Categorías editables + seeds por vertical | tabla `categories`, `seeds/*.sql`, `ProductsPage`, `PosPage` | M |
| 4 | Cantidades decimales | `Entities.h`, `SaleRepository`, `InventoryRepository`, `PosPage` | M |
| 5 | attrs_json + lotes obligatorios + alertas vencimiento | migración, `ProductRepository`, `DashboardPage` | M |
| 6 | Seriales/IMEI + garantía | tabla `serials`, `SalesService`, `PosPage`, `SalesPage` | L |
| 7 | Sidebar/reportes adaptativos | `AppSidebar`, `ReportService`, `ReportsPage` | M |

Total estimado: ~7 iteraciones; cada una con suites QtTest nuevas en `tests/` (siguiendo las 11 existentes) y `ctest` en verde antes de merge.

## Reglas transversales

1. **Migraciones solo aditivas** (`CREATE TABLE IF NOT EXISTS`, `ALTER TABLE ADD COLUMN`) en `DatabaseManager::migrateLegacyColumns()` — jamás renombrar/borrar; la BD debe seguir abierta por el sistema Python anterior.
2. Config siempre vía `settings` key/value → sin variables de entorno por vertical.
3. Ningún literal `"COP"`, `"19"` o nombre de empresa en QML/services fuera de `SettingsService`.
4. Cada feature nueva detrás de un flag de `settings` para no alterar negocios ya operando.
5. Tickets siguen `.txt` + cola `outbox` idempotente; los datos de negocio viajan dentro del payload sincronizado.
