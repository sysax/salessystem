-- Seed opt-in: cafetería (Fase 2). Idempotente. No toca usuarios/ventas existentes.
INSERT OR REPLACE INTO settings (key, value) VALUES ('business_type', 'cafeteria');
INSERT OR REPLACE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"Impoconsumo 8%","rate":8},{"name":"Excluido","rate":0}]');
INSERT OR REPLACE INTO settings (key, value) VALUES ('default_tax_rate', '8');

INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Cafés', 0, 'cafeteria', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Panadería', 0, 'cafeteria', 2);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Bebidas frías', 0, 'cafeteria', 3);

INSERT OR IGNORE INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, status) VALUES
('CAF01', '7707101000011', 'Tinto', 'Taza 7oz', 'Cafés', '', 'Casa', 'Café Origen', 2500.0, 800.0, 2000.0, 'Impoconsumo 8%', 'unidad', 500, 100, 1500, 'C1', 'activo'),
('CAF02', '7707101000028', 'Capuchino', 'Taza 12oz', 'Cafés', '', 'Casa', 'Café Origen', 7000.0, 2800.0, 6000.0, 'Impoconsumo 8%', 'unidad', 300, 60, 900, 'C1', 'activo'),
('CAF03', '7707101000035', 'Croissant mantequilla', 'Unidad', 'Panadería', '', 'Casa', 'Panhorno', 4500.0, 2200.0, 3800.0, 'Impoconsumo 8%', 'unidad', 120, 25, 400, 'C2', 'activo'),
('CAF04', '7707101000042', 'Café en grano 500g', 'Bolsa 500g', 'Cafés', '', 'Casa', 'Café Origen', 28000.0, 18000.0, 24000.0, 'IVA 19%', 'paquete', 40, 8, 120, 'C3', 'activo'),
('CAF05', '7707101000059', 'Limonada de coco', 'Vaso 16oz', 'Bebidas frías', '', 'Casa', 'Plaza Mayorista', 9000.0, 3500.0, 7600.0, 'Impoconsumo 8%', 'unidad', 150, 30, 500, 'C4', 'activo');
