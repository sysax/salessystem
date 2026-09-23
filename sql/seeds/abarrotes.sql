-- Seed opt-in: abarrotes (Fase 2). Idempotente. No toca usuarios/ventas existentes.
INSERT OR REPLACE INTO settings (key, value) VALUES ('business_type', 'abarrotes');
INSERT OR REPLACE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"IVA 5%","rate":5},{"name":"Excluido","rate":0}]');
INSERT OR REPLACE INTO settings (key, value) VALUES ('default_tax_rate', '0');
INSERT OR REPLACE INTO settings (key, value) VALUES ('weight_unit_default', 'kg');

INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Granos', 0, 'abarrotes', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Aceites', 0, 'abarrotes', 2);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Lácteos', 0, 'abarrotes', 3);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Aseo', 0, 'abarrotes', 4);

INSERT OR IGNORE INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, status) VALUES
('AB01', '7703001000011', 'Arroz blanco', 'Granel por kilo', 'Granos', '', 'Diana', 'Abastos Colombia', 5200.0, 4100.0, 4700.0, 'Excluido', 'kg', 120.0, 20.0, 300.0, 'C1', 'activo'),
('AB02', '7703001000028', 'Azúcar morena', 'Granel por kilo', 'Granos', '', 'Incauca', 'Abastos Colombia', 4800.0, 3700.0, 4300.0, 'Excluido', 'kg', 90.0, 15.0, 250.0, 'C1', 'activo'),
('AB03', '7703001000035', 'Aceite vegetal 1L', 'Botella 1 litro', 'Aceites', '', 'Gourmet', 'Abastos Colombia', 9500.0, 7200.0, 8500.0, 'IVA 19%', 'l', 70, 15, 200, 'C2', 'activo'),
('AB04', '7703001000042', 'Leche entera 1L', 'Bolsa 1 litro', 'Lácteos', '', 'Colanta', 'Lácteos Sur', 4200.0, 3300.0, 3800.0, 'Excluido', 'l', 110.0, 25.0, 300.0, 'D1', 'activo'),
('AB05', '7703001000059', 'Queso campesino', 'Bloque por kilo', 'Lácteos', '', 'Colanta', 'Lácteos Sur', 24000.0, 19000.0, 21500.0, 'Excluido', 'kg', 18.5, 4.0, 60.0, 'D1', 'activo'),
('AB06', '7703001000066', 'Tomate chonto', 'Granel por kilo', 'Granos', '', 'Plaza', 'Plaza Mayorista', 3800.0, 2500.0, 3300.0, 'Excluido', 'kg', 45.0, 8.0, 150.0, 'C3', 'activo'),
('AB07', '7703001000073', 'Jabón en polvo 1kg', 'Bolsa 1kg', 'Aseo', '', 'FAB', 'Abastos Colombia', 12500.0, 9800.0, 11200.0, 'IVA 19%', 'paquete', 55, 10, 150, 'E1', 'activo');
