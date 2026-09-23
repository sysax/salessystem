# Plan ERP — Ruta de evolución del Sistema de Ventas Qt

> Estado base: sistema de ventas e inventario (POS, inventario, compras, CxC/CxP,
> catálogo, clientes/proveedores, promociones, reportes, usuarios/roles, modo offline).
> Objetivo: ERP pyme para comercio minorista (tienda, minimarket, papelería, farmacia, ferretería).

## 1. Brechas detectadas

| # | Módulo ERP faltante | Estado actual | Esfuerzo aprox. |
|---|---|---|---|
| 1 | Contabilidad general (catálogo de cuentas, pólizas/asientos automáticos, balance general, PyG contable) | ❌ Solo reportes operativos | Alto — es el corazón del ERP |
| 2 | Multi-sucursal / multi-almacén (existencias por bodega, traslados, caja por sucursal) | ❌ Una sola ubicación | Alto |
| 3 | Nómina y RRHH (empleados, salarios, prestaciones, comisiones de vendedores) | ❌ Solo usuarios/roles | Alto |
| 4 | Activos fijos (registro, depreciación) | ❌ Nada | Bajo |
| 5 | Facturación fiscal completa (DIAN está en `OFF` por defecto) | ⚠️ Parcial | Medio |
| 6 | Multi-moneda (COP + USD, tasa de cambio) | ❌ Solo COP | Bajo-Medio |
| 7 | Manufactura / MRP y escandallos (listas de materiales, órdenes de producción) | ❌ Nada | Medio-Alto |
| 8 | CRM comercial (pipeline cotización→pedido→venta con seguimiento) | ⚠️ Documentos sueltos | Medio |
| 9 | Presupuestos y control (vs. real) | ❌ Nada | Medio |
| 10 | Auditoría y aprobaciones (flujos de autorización, bitácora contable inmutable) | ⚠️ Lockout/bitácora básica | Medio |

## 2. Fase A — Base contable (prioritaria, habilita todo lo demás)

- [ ] `sql`: tablas `cuentas_contables`, `polizas`, `poliza_lineas` (partida doble con constraint `debe == haber`).
- [ ] `AccountingService`: `generarPoliza(origen, lineas)` + catálogo base NIIF-pyme precargado en seed.
- [ ] Asientos automáticos en: `SalesService::create` (ingreso + IVA + CxC/caja), `PurchaseService` (costo + CxP), `CreditService::pay` (cobros/pagos), ajustes de inventario.
- [ ] UI: `AccountsPage` (catálogo de cuentas), `JournalPage` (pólizas con filtro por periodo), balance de comprobación en `ReportsPage`.
- [ ] Tests: `tst_accounting` (toda póliza cuadra; cada operación comercial genera la suya).

## 3. Fase B — Estructura

- [ ] `sucursales` + `existencias(sucursal, sku)`; traslados entre bodegas; caja por sucursal.
- [ ] `monedas` + `tasa_cambio`; totales duales en POS y reportes.

## 4. Fase C — Personas

- [ ] `empleados`, `nomina` (salario, prestaciones, deducciones), `comisiones` por vendedor (% sobre venta pagada).
- [ ] `activos_fijos` + depreciación línea recta automática mensual.

## 5. Fase D — Fiscal y producción

- [ ] DIAN full (documentos electrónicos firmados; hoy solo documentos internos).
- [ ] `recetas` (insumo × cantidad) con descuento automático al vender + órdenes de producción.
- [ ] `presupuestos` mensuales por cuenta/categoría + comparativa vs. real.

## 6. Criterios de aceptación global

- Toda operación comercial genera póliza cuadrada (verificado en tests).
- Cierre mensual bloquea edición retroactiva (auditoría).
- Reportes nuevos: balance general, PyG contable, flujo por sucursal.

## 7. Nota de alcance

No perseguir el “ERP total” de golpe: la Fase A sola ya convierte el sistema en un **ERP pyme**
funcional para comercio, que es su mercado natural (venta mostrador: el cliente pide, paga
y se lleva la mercancía). Bares/restaurantes (mesas, comandas) y manufactura quedan para
fases posteriores o verticales separadas.
