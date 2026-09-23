-- Seed opt-in: ropa (Fase 2). Idempotente. No toca usuarios/ventas existentes.
INSERT OR REPLACE INTO settings (key, value) VALUES ('business_type', 'ropa');
INSERT OR REPLACE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"Excluido","rate":0}]');
INSERT OR REPLACE INTO settings (key, value) VALUES ('default_tax_rate', '19');

INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Hombre', 0, 'ropa', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Mujer', 0, 'ropa', 2);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Camisetas', (SELECT id FROM categories WHERE name='Hombre' AND parent_id = 0), 'ropa', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Jeans', (SELECT id FROM categories WHERE name='Hombre' AND parent_id = 0), 'ropa', 2);

INSERT OR IGNORE INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, status) VALUES
('ROP01', '7706001000011', 'Camiseta básica M', 'Algodón blanca', 'Hombre', 'Camisetas', 'Propia', 'TextilAndina', 35000.0, 18000.0, 29000.0, 'IVA 19%', 'unidad', 40, 8, 120, 'R1', 'activo'),
('ROP02', '7706001000028', 'Jean clásico 32', 'Denim azul', 'Hombre', 'Jeans', 'Propia', 'TextilAndina', 89000.0, 48000.0, 75000.0, 'IVA 19%', 'unidad', 25, 5, 80, 'R1', 'activo'),
('ROP03', '7706001000035', 'Blusa floral S', 'Viscosa', 'Mujer', '', 'Propia', 'TextilAndina', 55000.0, 29000.0, 46000.0, 'IVA 19%', 'unidad', 30, 6, 90, 'R2', 'activo'),
('ROP04', '7706001000042', 'Vestido corto M', 'Lino', 'Mujer', '', 'Propia', 'TextilAndina', 95000.0, 52000.0, 80000.0, 'IVA 19%', 'unidad', 18, 4, 60, 'R2', 'activo'),
('ROP05', '7706001000059', 'Medias x3', 'Paquete x3 pares', 'Hombre', '', 'Propia', 'TextilAndina', 15000.0, 7000.0, 12000.0, 'IVA 19%', 'paquete', 60, 12, 200, 'R3', 'activo');
