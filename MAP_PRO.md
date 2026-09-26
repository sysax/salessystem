--- docs/ROADMAP_PROFESIONAL.md (原始)


+++ docs/ROADMAP_PROFESIONAL.md (修改后)
# 🗺️ Roadmap de Profesionalización — POS Qt6/C++20

Plan de implementación por fases de las mejoras identificadas en la revisión de código, arquitectura y lógica de aplicación.

**Principios rectores:**
- Cada fase debe dejar la aplicación **compilable, con tests en verde y funcional**.
- No avanzar de fase sin cumplir los *criterios de salida*.
- Las fases 0–2 son **bloqueantes para producción**; el resto mejora competitividad.

---

## 📊 Resumen de Fases

| Fase | Nombre | Prioridad | Esfuerzo estimado | Riesgo si se omite |
| :--- | :----- | :-------- | :---------------- | :----------------- |
| 0 | Higiene técnica y CI | 🔴 Alta | 1 semana | Deuda creciente, regressiones silenciosas |
| 1 | Integridad transaccional | 🔴 Crítica | 2 semanas | Corrupción de datos, pérdidas económicas |
| 2 | Seguridad y dinero exacto | 🔴 Crítica | 2–3 semanas | Fraude, errores contables, brechas |
| 3 | Lógica de negocio correcta | 🟠 Alta | 3 semanas | Ventas inválidas, inventario fantasma |
| 4 | Rendimiento y escalabilidad | 🟠 Media | 2 semanas | Degradación con datos reales |
| 5 | Operación profesional (caja, facturas, auditoría) | 🟠 Alta | 4 semanas | No apto para negocio real |
| 6 | Offline-first robusto y multi-sucursal | 🟡 Media | 4–6 semanas | Pérdida de ventas, sin expansión |
| 7 | UX profesional e i18n | 🟡 Media | 3 semanas | Adopción pobre, error-prone |
| 8 | Calidad, observabilidad y release | 🟢 Continua | 2 semanas + ongoing | Incapacidad de diagnosticar/fiabilizar |
| 9 | Ediciones de producto (Básica / Premium / Premium Plus) | 🟠 Alta | 4–6 semanas (tras Fase 5) | Sin modelo comercial escalable, features mezcladas |

---

## 🏷️ Modelo de Ediciones del Producto

La aplicación se empaquetará en **tres ediciones comerciales**, acumulativas entre sí. Las Fases 0–8 construyen el núcleo común a todas; la **Fase 9** activa las capacidades diferenciadas.

| | **BÁSICA** 🥉 | **PREMIUM** 🥈 | **PREMIUM PLUS** 🥇 |
| :-- | :---: | :---: | :---: |
| POS, catálogo, clientes, caja por turno | ✅ | ✅ | ✅ |
| Facturación **no fiscal** (tickets/recibos internos) | ✅ | ✅ | ✅ |
| Reportes operativos básicos | ✅ | ✅ | ✅ |
| **Facturación electrónica** (CFDI/DIAN, timbrado, PAC) | ❌ | ✅ | ✅ |
| Flujo documental fiscal (factura, nota de crédito, resolución) | ❌ | ✅ | ✅ |
| Envío de comprobantes (email/WhatsApp), buzón tributario | ❌ | ✅ | ✅ |
| **ERP completo**: CxC/CxP, contabilidad, multi-sucursal | ❌ | ❌ | ✅ |
| Compras avanzadas, inventario multi-almacén, lotes | ❌ | Limitado | ✅ |
| Producción/ensambles, presupuesto, nómina | ❌ | ❌ | ✅ |
| Offline-first con sync multi-sucursal | ❌ | Opcional | ✅ |
| API/integraciones y exportación contable | ❌ | Básica | ✅ Completa |

**Reglas del modelo:**
- Una licencia superior incluye todo lo de las inferiores (upgrade sin migración de datos).
- La edición se determina por **licencia activada**, no por build distinto: un solo binario con *feature gating* (ver Fase 9).
- Los clientes Básicos pueden operar legalmente con ticket interno sin validez fiscal ("No es factura"), o adquirir módulos de facturación como add-on si su país lo exige.

---

## Fase 0 — Higiene Técnica y CI 🔧

**Objetivo:** crear la red de seguridad antes de tocar lógica crítica.

### Tareas
- [ ] Añadir `clang-format` (`.clang-format`) y `cmake-format`; aplicar formateo en un commit único ("no-op").
- [ ] Integrar `qmllint` en el build (`qt_add_qml_module` + target de lint).
- [ ] Configurar CI GitHub Actions: build + tests en Linux, Windows y macOS (matriz de agentes).
- [ ] Activar compiladores con warnings estrictos: `-Wall -Wextra -Werror`, sanitizers (ASan/UBSan) en job dedicado.
- [ ] Análisis estático: clang-tidy con checks de seguridad y moveToThread/ownership.
- [ ] Cobertura de tests (gcov/llvm-cov) con umbral mínimo (ej. 60% en `src/services`).
- [ ] Corregir documentación desincronizada (`arquitectura.txt`, README vs código real C++).

### Criterios de salida
✅ Pipeline verde en 3 plataformas · ✅ `make format-check` y `qmllint` sin errores · ✅ Suite de tests ejecutándose en CI.

---

## Fase 1 — Integridad Transaccional 🔥 *(máximo riesgo actual)*

**Objetivo:** ninguna operación multi-escritura puede quedar a medias.

### Tareas
- [ ] Implementar RAII `Transaction` en `DatabaseManager`:
  ```cpp
  class Transaction {          // src/core/Transaction.h
  public:
      explicit Transaction(QSqlDatabase& db);
      ~Transaction();          // rollback automático si no se commit()
      bool commit();
      bool isValid() const;
  };
  ```
- [ ] Envolver en transacciones todos los flujos multi-escritura:
  - `SaleService::registerSale` (venta + items + pagos + stock + caja + auditoría).
  - Cancelaciones/devoluciones (reversión completa: stock, caja, saldos de crédito).
  - Recepción de compras, ajustes de inventario, cierres de caja.
- [ ] Actualizaciones atómicas de stock (eliminar patrón TOCTOU leer-luego-escribir):
  ```sql
  UPDATE products SET stock = stock - :qty
  WHERE id = :id AND stock >= :qty;   -- 0 filas afectadas => stock insuficiente
  ```
- [ ] Pragma de SQLite al abrir conexión:
  ```sql
  PRAGMA foreign_keys = ON;
  PRAGMA journal_mode = WAL;
  PRAGMA busy_timeout = 5000;
  ```
- [ ] Verificar retorno de **todo** `QSqlQuery::exec()` vía helper centralizado (`Result<T>` ya existente en `src/core/Result.h`); prohibir exec sin check (clang-tidy custom o convención revisada).
- [ ] Tests de concurrencia: N hilos vendiendo el mismo producto → stock nunca negativo, ventas totalizadas correctas.
- [ ] Tests de rollback forzado (inyectar fallo a mitad de venta → base intacta).

### Criterios de salida
✅ Test de carrera con 50 hilos estable · ✅ Ningún `exec()` sin verificar (grep limpio) · ✅ Cancelación de venta revierte exactamente todas las escrituras.

---

## Fase 2 — Seguridad y Dinero Exacto 🔐💰

**Objetivo:** credenciales seguras y aritmética monetaria sin errores de coma flotante.

### 2.1 Autenticación
- [x] Migrar de hash débil/ad-hoc a PBKDF2-HMAC-SHA256 con **600 000 iteraciones** y salt aleatorio por usuario (`QRandomGenerator`). Formato versionado `pbkdf2$<iter>$<salt>$<dk>`; v1 (100k) y texto plano se verifican y **migran al primer login correcto** (`needsUpgrade`).
- [x] Ejecutar hashing fuera del hilo de UI (`AuthController::loginAsync` con `QtConcurrent` + conexión SQLite propia por worker; eventos capturados y re-publicados en UI; `LoginPage` con `BusyIndicator`). El `login()` síncrono se conserva para compatibilidad/tests.
- [x] Política de contraseñas: longitud mínima (`MinPasswordLength`, enforced en alta/cambio/reset), `must_change_password` en primer ingreso y claves puestas por admin.
- [x] Bloqueo temporal de cuenta tras N intentos fallidos + registro en bitácora (3 intentos → 5 min, fail-closed).
- [x] Sesiones: expiración por inactividad (30 min, `touch()` en navegación/login + `checkIdle()` por timer en `Main.qml` con regreso al login) y cierre de sesión en backend.

### 2.2 Dinero como tipo exacto
- [x] Completar `Money` (`src/core/Money.h`): `taxCents`/`withRate` genéricos con **round-half-up documentado** (sin hardcodear IVA 19 %; `withIva` queda como compatibilidad).
- [x] Adoptarlo en el cálculo de impuestos por línea (`SalesService`: `lineTax` en céntimos en `buildTotals` y `calculateTotals`) + test de invariante `subtotal + impuesto − descuento == total` al céntimo (`tst_tax::centsInvariant`, `tst_money::rateHalfUp`).
- [ ] Migración total `double` → `Money` en repos/services/entidades/QML bridge (pendiente: cambio mayor que toca esquema REAL, QML y trabajo ajeno en curso; el IVA por línea —la fuente real de errores— ya va en céntimos).
- [ ] Validar en entrada de UI: máximos decimales permitidos, sin negativos donde no aplique.

> **Cierre Fase 2 (2026-09-26):** `tst_auth` (versionado/migración/async/idle) + `tst_tax::centsInvariant` + `tst_money::rateHalfUp` en verde; suite 17/17.

### Criterios de salida
✅ Tabla de usuarios migrada (hash nuevo verificado, antiguo invalidado) · ✅ Grep de `double` en rutas de dinero = 0 · ✅ Suite de tests de redondeo fiscal en verde.

---

## Fase 3 — Lógica de Negocio Correcta ⚙️

**Objetivo:** reglas de negocio completas, centralizadas y configurables.

### Tareas
- [x] **Créditos:** validar límite de crédito del cliente antes de permitir venta a crédito (`saldo_pendiente + nueva_venta <= limite` en `SalesService::create`; sin límite asignado bloquea; Mostrador sin ficha no se valida).
- [x] **Cancelaciones/devoluciones:** permisos específicos para cancelar (rol Administrador en servicio + botón solo-admin en UI, fail-closed). Reversión íntegra en curso en Fase 1.
- [x] **Promociones:** vigencia (`valid_from`/`valid_to`), prioridad (mayor primero) y límite de usos (`max_uses`/`uses`, cuenta en ventas exitosas); motor evaluado en `PromoRepository::evaluate` (punto único). Multi-código apilable pendiente (hoy un código por venta).
- [x] **Stock:** físico / reservado / disponible (`stock_reserved` aditivo + `reserveAtomic`/`releaseAtomic`); POS y ventas validan contra disponible; la venta consume apartados; apartar/liberar en Inventario con motivo. Caducidad automática de apartados pendiente.
- [x] **Caja en JSON → tablas normalizadas:** tabla `caja_movimientos(tipo, monto, metodo_pago, sale_id, user_id, ts)` con turno de ms; el JSON queda como caché; `expectedFromMovements()` cuadra con el turno (test).
- [ ] **Configuración externalizada:** impuestos, recargos, series de folios y parámetros de promo desde `SettingsRepository`, no hardcodeados. (Parcial: impuestos y tasas ya salen de Settings.)
- [ ] **Matriz de permisos única fuente de verdad:** definir en C++ (enum + tabla) y exponer a QML vía `Q_PROPERTY`; eliminar duplicación backend/UI. (Parcial: `ROLE_PERMISSIONS` en `AuthService`, QML consulta `auth.canAccess`.)
- [ ] Refactor de clases gigantes: dividir `ReportService` (financiero / operativo / inventario) y `PosPage.qml` (carrito, pagos, atajos) en componentes. (Diferido: churn sin cambio de comportamiento.)

> **Cierre Fase 3 (2026-09-26, parcial):** crédito, vigencia/prioridad/usos de promos, permiso de anulación, apartados y caja por movimientos en verde; suite 17/17.

### Criterios de salida
✅ Tests de regla por cada política (crédito, cancelación, promo con vigencia) · ✅ QML y C++ consultan el mismo permiso para la misma acción · ✅ Caja cuadra por construcción (movimientos, no estado acumulado).

---

## Fase 4 — Rendimiento y Escalabilidad ⚡

**Objetivo:** comportamiento estable con años de datos reales.

### Tareas
- [x] Paginación en el servidor (`LIMIT/OFFSET` + conteo) para productos, ventas y clientes, expuesta en controladores (`searchPaged`/`refreshPaged` + `totalCount`) y cableada en las 3 páginas.
- [x] Columnas explícitas en tablas nuevas sin legacy (categorías, seriales, movimientos de caja, outbox); tablas con columnas aditivas legacy (`products`, `sales`, …) conservan `SELECT *` a propósito (compatibilidad con BDs viejas).
- [x] Índices verificados con `EXPLAIN QUERY PLAN` en `tst_perf`: barcode, `sale_items(product_id/sale_id)`, `sales(date)`, `clients(name)`, `audit(user,timestamp)`, `serials(sku)`, `products(business_type)`; redundantes (`sku`, `clients.name` por UNIQUE) eliminados de BDs nuevas y viejas.
- [x] `QAbstractListModel` con fetch incremental (`PagedListModel` genérico + scroll infinito en Productos/Ventas/Clientes con `BusyIndicator`; lotes de 30).
- [x] Medición: `tst_perf` con dataset sintético (5k productos, 500 clientes/ventas) + tiempos acotados. Escala 100k/1M como procedimiento manual (fuera de CI).

> **Cierre Fase 4 (2026-09-26):** índices + columnas + paginación backend + scroll infinito + `tst_perf` en verde; suite 19/19.

### Criterios de salida
✅ Búsqueda de cliente < 100 ms con 1M de filas · ✅ Memoria estable tras 1 h de uso continuo de POS.

---

## Fase 5 — Operación Profesional 🏪

**Objetivo:** cubrir el ciclo de vida real de un negocio.

### Tareas
- [ ] **Cierre de caja formal:** apertura/cierre por turno con arqueo físico, diferencias justificadas, firma digital del cajero y reporte de sobras/faltantes.
- [ ] **Documentes fiscales:** flujo cotización → pedido → factura → nota de crédito con estados y folios únicos (contador ya en `Counters.h`); adaptación CFDI (México) / DIAN (Colombia) según `docs/DIAN_IMPROVEMENTS.md` con proveedor de certificación en modo pruebas.
- [ ] **Audit log inmutable:** toda escritura registra usuario, acción, valores antes/después (`AuditRepository` extendido); vista de exploración en UI de administración.
- [ ] **Compras completas:** orden de compra → recepción parcial → CxC con vencimientos y abonos.
- [ ] **Inventario avanzado:** lotes y caducidad, valuación PEPS/promedio, conteos cíclicos con ajustes justificados, alertas de stock mínimo y próximos a caducar.
- [ ] **Backups:** backup automático de SQLite (API de backup, no copiar el archivo en caliente), retención configurable y prueba de restauración documentada.

### Criterios de salida
✅ Ciclo completo compra→venta→nota de crédito→conciliación de caja demostrable · ✅ Restauración desde backup probada · ✅ Auditoría responde "¿quién cambió este precio y cuándo?".

---

## Fase 6 — Offline-First Robusto y Multi-Sucursal 🌐

**Objetivo:** sincronización confiable según `docs/SYNC_IMPROVEMENTS.md`.

### Tareas
- [ ] Cola de outbox persistente (eventos enumerados con `device_id + seq`) y reconciliación idempotente en servidor.
- [ ] Estrategia de resolución de conflictos definida por entidad (LWW para catálogo, merge aditivo para movimientos).
- [ ] Soporte multi-almacén/sucursal con traspazos y stock por ubicación.
- [ ] Estado de sincronización visible en UI (pendientes, último sync, error) con reintento exponencial.

### Criterios de salida
✅ 24 h offline → sync sin pérdida ni duplicados · ✅ Venta concurrente del mismo SKU en dos cajas resuelta correctamente.

---

## Fase 7 — UX Profesional e Internacionalización 🎨

**Objetivo:** interfaz usable por personal sin capacitación extensa.

### Tareas
- [ ] Extracción de textos a `qsTr()` + `lupdate/lrelease` (es/en como base).
- [ ] Formato localizado de moneda/fecha/hora vía `QLocale` (dinero ya tipado en Fase 2 facilita esto).
- [ ] Accesibilidad: navegación por teclado completa en POS, roles `Accessible.*` en QML, contraste AA, tamaño táctil ≥ 44 px.
- [ ] Feedback de sistema: toasts/no-blockers, indicador de guardado, confirmaciones destructivas con motivo obligatorio.
- [ ] Atajos personalizables y layout de caja configurable por operador.
- [ ] Modo alto contraste / tema oscuro para turnos nocturnos.

### Criterios de salida
✅ Flujo de venta completo ejecutable sin ratón · ✅ 100% de strings en catálogo de traducción.

---

## Fase 8 — Calidad, Observabilidad y Release 📦

**Objetivo:** operación predecible a largo plazo.

### Tareas
- [ ] Logging estructurado con niveles y rotación (`qSetMessagePattern` + sink a archivo); correlación venta↔logs con `sale_id`.
- [ ] Métricas básicas: tiempo de venta, errores de sync, fallos de caja — exportables a reporte semanal.
- [ ] Manejo global de excepciones/fallos: crash handler que preserve cola de sync y deje rastro.
- [ ] Versionado semántico + changelog generado desde commits; instaladores firmados (Windows/macOS) en pipeline de release.
- [ ] Plan de actualización de BD: migraciones versionadas (`user_version` de SQLite) con downgrade documentado.
- [ ] Revisión de licencia y cumplimiento (componentes third-party, LGPL/comercial Qt).

### Criterios de salida
✅ Release reproducibles desde tag · ✅ Migración vN→vN+1 probada sobre copia de producción · ✅ Crash report identifica módulo y versión.

---

## Fase 9 — Ediciones de Producto: Básica / Premium / Premium Plus 🏷️

**Objetivo:** materializar las tres ediciones comerciales sobre un mismo núcleo, con *feature gating* por licencia, sin bifurcar el código ni duplicar el binario.

> **Dependencias:** requiere Fase 3 (reglas centralizadas), Fase 5 (flujo documental base) y Fase 6 (sync, para multi-sucursal). Las Fases 0–8 aplican a **todas** las ediciones.

### 9.1 Núcleo común (preparación transversal)
- [ ] Definir `enum class Edition { Basic, Premium, PremiumPlus }` y `FeatureFlags` en C++ (`src/core/Licensing.h`): mapa feature → edición mínima.
- [ ] Sistema de licencia local: archivo firmado (HMAC/RSA) con edición, expiración, `machine_id` y cupos de sucursal/caja; validación al arranque y revalidación periódica si hay red.
- [ ] Exponer flags a QML vía `Q_PROPERTY bool hasFacturaElectronica`… + adjunto QML `EditionFeatures`; UI muestra íconos 🔒 con upsell en funciones bloqueadas.
- [ ] Gate también en **backend** (no solo UI): los services rechazan operaciones de features no licenciadas aunque se invoquen por API interna.
- [ ] Pantalla "Edición y licencia" en administración: edición activa, features incluidas, fecha de expiración, botón de activación/mejora.
- [ ] Pruebas: matriz de tests que ejecuta la suite crítica con cada edición simulada (Basic debe poder operar 100% sin módulos fiscales/ERP cargados).

### 9.2 Edición BÁSICA 🥉 — POS sin facturación electrónica
**Público:** pequeños comercios que emiten ticket interno o cuya obligación fiscal se cumple fuera del sistema.
- [ ] Congelar como producto auto-suficiente: POS, catálogo, clientes, créditos simples, caja por turno, reportes operativos (ventas, top productos, arqueos).
- [ ] Ticket/no-fiscal con leyenda configurable ("Este comprobante no tiene validez fiscal") y numeración interna propia.
- [ ] Exportación CSV/PDF de ventas para que el cliente entregue información a su contador.
- [ ] Backup local (Fase 5) y un solo almacén/sucursal; sin cola de sync hacia servidor.
- [ ] Documentación de onboarding y demo: trial de evaluación de 30 días (si expira la licencia, la app pasa a **modo solo lectura** — los datos permanecen accesibles para exportar).

### 9.3 Edición PREMIUM 🥈 — + Facturación Electrónica
**Público:** PyMEs obligadas a emitir comprobantes fiscales digitales (CFDI México / DIAN Colombia / equivalentes).
- [ ] Módulo `FacturaElectronica` desacoplado del POS (puerto adaptador): generación XML según estándar, canonicalización y firma (XAdES/fiel), timbrado vía PAC/SAP proveedor configurable.
  - Implementar según `docs/DIAN_IMPROVEMENTS.md` (Colombia) y diseño análogo CFDI 4.0 (México); país/régimen seleccionado en configuración.
- [ ] Certificados del cliente: carga (.cer/.key / .p12), almacenamiento seguro (clave maestra derivada + DPAPI/Keychain), vigencia y aviso de expiración.
- [ ] Flujo completo: venta → prevalidación (RFC/NIT, régimen, uso CFDI, productos codificados SAT/UNSPSC) → timbrado → PDF/A + XML → envío por email → seguimiento de estado (aceptado/rechazado/cancelado).
- [ ] Cancelación de CFDI/documentos fiscales con causa y relación de sustitución; control de folios/resoluciones vigentes.
- [ ] Contingencia offline de timbrado: cola de documentos pendientes con sello propio y reenvío automático al recuperar red (nunca perder venta por caída del PAC).
- [ ] Reportes fiscales: pólvora/diario de ventas, exportación para contabilidad (ASCII/CSV formato proveedor habilitado).
- [ ] Sync opcional mono-sucursal (backup en la nube + multi-caja coordinada).

### 9.4 Edición PREMIUM PLUS 🥇 — ERP + Facturación Electrónica
**Público:** medianas empresas con operación multi-área que necesitan integrarlo todo.
- [ ] Todo lo de Premium, más módulos ERP sobre la misma BD y dominio:
  - **Compras/CxP:** orden de compra → recepción parcial → cuentas por pagar con vencimientos, aging y pagos a proveedores; costos reales/promedio ponderado alimentados desde recepción.
  - **Ventas/CxC extendido:** cotizaciones → pedidos → remisiones → factura (Premium) con estados; crédito con límites por cliente y antigüedad de saldos.
  - **Inventario multi-almacén:** traspasos, lotes/series/caducidad, valuación PEPS/promedio, conteos cíclicos (extensión de Fase 5 y 6).
  - **Contabilidad:** pólizas automáticas desde movimientos (venta, compra, nómina, ajustes), plan de catálogos, balance y P&Y; conciliación bancaria importable (MTA/CSV).
  - **Producción/ensambles:** listas de materiales (BOM), órdenes de producción, consumo de insumos y costo de fabricación.
  - **Presupuestos y nómina básica** (según país; modular como add-on regional).
- [ ] Multi-sucursal completo: consolidación de reportes corporativos, transferencias entre sucursales, permisos por sede; sincronía basada en Fase 6 (outbox + resolución de conflictos).
- [ ] Centro de usuarios/roles granular (RBAC por módulo), sesiones concurrentes y auditoría ampliada (Fase 5) por sede.
- [ ] API REST/módulo de integración saliente (webhooks, exportación contable a ERPs externos) y conectores (e-commerce, básculas, CRM).
- [ ] Dashboard gerencial: KPIs consolidados (margen, rotación, DSO/DPO, flujo de caja proyectado).

### 9.5 Estrategia de release y actualización
- [ ] Un solo instalador; la licencia determina funciones. Upgrade Básico→Premium→Plus = reactivar licencia (sin reinstalar ni migrar datos).
- [ ] Downgrade seguro: los datos generados por módulos superiores quedan intactos pero ocultos (nunca corruptos) si una licencia expira.
- [ ] Pricing/paquetes documentados en `docs/EDICIONES.md` + tabla de características pública; trial de Premium 14 días con marca de agua en comprobantes.

### Criterios de salida
✅ Tres licencias de prueba activan exactamente las features declaradas en la matriz (gate verificado en UI **y** services) · ✅ Cliente Básico opera un día completo sin cargar rutas de facturación/ERP · ✅ Timbrado de prueba exitoso en entorno sandbox del PAC (Premium) · ✅ Ciclo compra→recepción→CxP→póliza contable demostrable (Plus) · ✅ Upgrade Básico→Plus sin pérdida de datos verificado.

---

## 🚀 Orden de ejecución recomendado

```
Fase 0 ──► Fase 1 ──► Fase 2 ──► Fase 3 ──┬─► Fase 4 ──► Fase 5 ──► Fase 6
                                          │                │          │
                          (paraleizable)  └─► Fase 7       └────┬─────┘
                                               Fase 8 (continua) ▼
                                              Fase 9: 9.1 Flags ─► 9.2 Básica (GA #1)
                                                        ─► 9.3 Premium (GA #2)
                                                        ─► 9.4 Premium Plus (GA #3)
```

**Hitos de lanzamiento sugeridos:**
1. **v1.0 Básica** — al cerrar Fases 0–5 + 7 + 9.1/9.2 (producto vendible sin fiscal).
2. **v1.5 Premium** — al certificar timbrado en sandbox y producción piloto (9.3).
3. **v2.0 Premium Plus** — ERP completo multi-sucursal estable (9.4), apoyado en Fase 6.

**Regla de oro:** nada de nuevas funcionalidades comerciales hasta completar las Fases 1–3. La integridad de datos y el manejo exacto del dinero son prerrequisito de todo lo demás — y aplican por igual a las tres ediciones.
