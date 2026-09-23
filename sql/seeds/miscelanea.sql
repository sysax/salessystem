-- Seed opt-in: miscelánea (genérico, Fase 2). Idempotente.
INSERT OR REPLACE INTO settings (key, value) VALUES ('business_type', 'miscelanea');
INSERT OR REPLACE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"Excluido","rate":0}]');
INSERT OR REPLACE INTO settings (key, value) VALUES ('default_tax_rate', '19');

INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('General', 0, '', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Papelería', 0, '', 2);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Aseo hogar', 0, '', 3);

INSERT OR IGNORE INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, status) VALUES
('MIS01', '7707801000011', 'Cuaderno cosido', '100 hojas cuadriculado', 'Papelería', '', 'Norma', 'Papelera', 8500.0, 5200.0, 7300.0, 'IVA 19%', 'unidad', 80, 15, 250, 'M1', 'activo'),
('MIS02', '7707801000028', 'Esfero negro', 'Unidad', 'Papelería', '', 'Bic', 'Papelera', 1800.0, 900.0, 1400.0, 'IVA 19%', 'unidad', 200, 40, 600, 'M1', 'activo'),
('MIS03', '7707801000035', 'Detergente 500g', 'Bolsa', 'Aseo hogar', '', 'FAB', 'Abastos Colombia', 6800.0, 4900.0, 5900.0, 'IVA 19%', 'paquete', 60, 12, 200, 'M2', 'activo'),
('MIS04', '7707801000042', 'Azúcar 1kg', 'Bolsa', 'General', '', 'Incauca', 'Abastos Colombia', 5200.0, 4000.0, 4600.0, 'Excluido', 'paquete', 90, 20, 300, 'M3', 'activo'),
('MIS05', '7707801000059', 'Sal 1kg', 'Bolsa', 'General', '', 'Refisal', 'Abastos Colombia', 2800.0, 1900.0, 2400.0, 'Excluido', 'paquete', 100, 20, 300, 'M3', 'activo');
