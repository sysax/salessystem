-- Seed opt-in: restaurante (Fase 2; comandas/recetas en Fase 5). Idempotente.
INSERT OR REPLACE INTO settings (key, value) VALUES ('business_type', 'restaurante');
INSERT OR REPLACE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"Impoconsumo 8%","rate":8},{"name":"Excluido","rate":0}]');
INSERT OR REPLACE INTO settings (key, value) VALUES ('default_tax_rate', '8');

INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Entradas', 0, 'restaurante', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Platos', 0, 'restaurante', 2);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Bebidas', 0, 'restaurante', 3);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Postres', 0, 'restaurante', 4);

INSERT OR IGNORE INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, status) VALUES
('RES01', '7707001000011', 'Empanadas x5', 'Mixtas con ají', 'Entradas', '', 'Casa', 'Plaza Mayorista', 12000.0, 6000.0, 10000.0, 'Impoconsumo 8%', 'unidad', 200, 40, 600, 'K1', 'activo'),
('RES02', '7707001000028', 'Bandeja paisa', 'Plato insignia', 'Platos', '', 'Casa', 'Plaza Mayorista', 28000.0, 14000.0, 24000.0, 'Impoconsumo 8%', 'unidad', 150, 30, 400, 'K1', 'activo'),
('RES03', '7707001000035', 'Ajiaco santafereño', 'Con pollo y alcaparras', 'Platos', '', 'Casa', 'Plaza Mayorista', 24000.0, 12000.0, 20000.0, 'Impoconsumo 8%', 'unidad', 120, 25, 350, 'K1', 'activo'),
('RES04', '7707001000042', 'Limonada natural', 'Vaso 16oz', 'Bebidas', '', 'Casa', 'Plaza Mayorista', 7000.0, 2500.0, 6000.0, 'Impoconsumo 8%', 'unidad', 300, 60, 900, 'K2', 'activo'),
('RES05', '7707001000059', 'Jugo de mora en leche', 'Vaso 16oz', 'Bebidas', '', 'Casa', 'Plaza Mayorista', 8000.0, 3000.0, 6800.0, 'Impoconsumo 8%', 'unidad', 250, 50, 800, 'K2', 'activo'),
('RES06', '7707001000066', 'Arroz con leche', 'Porción', 'Postres', '', 'Casa', 'Plaza Mayorista', 9000.0, 3500.0, 7600.0, 'Impoconsumo 8%', 'unidad', 100, 20, 300, 'K3', 'activo');
