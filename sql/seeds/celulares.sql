-- Seed opt-in: celulares (Fase 2). Idempotente. No toca usuarios/ventas existentes.
INSERT OR REPLACE INTO settings (key, value) VALUES ('business_type', 'celulares');
INSERT OR REPLACE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"Excluido","rate":0}]');
INSERT OR REPLACE INTO settings (key, value) VALUES ('default_tax_rate', '19');
INSERT OR REPLACE INTO settings (key, value) VALUES ('require_serial', '1');

INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Equipos', 0, 'celulares', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Accesorios', 0, 'celulares', 2);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Repuestos', 0, 'celulares', 3);

INSERT OR IGNORE INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, status) VALUES
('CEL01', '7704001000011', 'Xiaomi Redmi 13 256GB', 'Equipo nuevo sellado', 'Equipos', '', 'Xiaomi', 'CelMayorista', 780000.0, 640000.0, 710000.0, 'IVA 19%', 'unidad', 8, 2, 20, 'V1', 'activo'),
('CEL02', '7704001000028', 'Samsung A15 128GB', 'Equipo nuevo sellado', 'Equipos', '', 'Samsung', 'CelMayorista', 690000.0, 560000.0, 625000.0, 'IVA 19%', 'unidad', 6, 2, 18, 'V1', 'activo'),
('CEL03', '7704001000035', 'Cargador turbo 33W', 'Con cable USB-C', 'Accesorios', '', 'Xiaomi', 'CelMayorista', 45000.0, 28000.0, 38000.0, 'IVA 19%', 'unidad', 30, 8, 80, 'V2', 'activo'),
('CEL04', '7704001000042', 'Vidrio templado genérico', 'Protector pantalla', 'Accesorios', '', 'Genérico', 'CelMayorista', 12000.0, 5000.0, 9000.0, 'IVA 19%', 'unidad', 100, 20, 300, 'V2', 'activo'),
('CEL05', '7704001000059', 'Funda silicona', 'Varios modelos', 'Accesorios', '', 'Genérico', 'CelMayorista', 18000.0, 8000.0, 14000.0, 'IVA 19%', 'unidad', 80, 15, 250, 'V2', 'activo'),
('CEL06', '7704001000066', 'Batería genérica', 'Repuesto taller', 'Repuestos', '', 'Genérico', 'RepuestosYA', 35000.0, 20000.0, 30000.0, 'IVA 19%', 'unidad', 20, 4, 60, 'V3', 'activo');
