-- Seed opt-in: panadería (Fase 2). Idempotente. No toca usuarios/ventas existentes.
INSERT OR REPLACE INTO settings (key, value) VALUES ('business_type', 'panaderia');
INSERT OR REPLACE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"IVA 5%","rate":5},{"name":"Excluido","rate":0}]');
INSERT OR REPLACE INTO settings (key, value) VALUES ('default_tax_rate', '5');
INSERT OR REPLACE INTO settings (key, value) VALUES ('weight_unit_default', 'unidad');

INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Pan', 0, 'panaderia', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Pastelería', 0, 'panaderia', 2);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Bebidas', 0, 'panaderia', 3);

INSERT OR IGNORE INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, status) VALUES
('PAN01', '7707201000011', 'Pan francés', 'Unidad 60g', 'Pan', '', 'Casa', 'Molinos', 800.0, 350.0, 650.0, 'Excluido', 'unidad', 800, 150, 2500, 'P1', 'activo'),
('PAN02', '7707201000028', 'Pan integral molde', 'Bolsa 500g', 'Pan', '', 'Casa', 'Molinos', 7500.0, 4200.0, 6400.0, 'IVA 5%', 'paquete', 90, 20, 300, 'P1', 'activo'),
('PAN03', '7707201000035', 'Almojábana', 'Unidad', 'Pan', '', 'Casa', 'Molinos', 2200.0, 1000.0, 1800.0, 'Excluido', 'unidad', 300, 60, 1000, 'P1', 'activo'),
('PAN04', '7707201000042', 'Torta de chocolate lb', 'Por libra', 'Pastelería', '', 'Casa', 'Molinos', 42000.0, 24000.0, 36000.0, 'IVA 19%', 'unidad', 12, 2, 40, 'P2', 'activo'),
('PAN05', '7707201000059', 'Avena vaso', 'Vaso 12oz', 'Bebidas', '', 'Casa', 'Lácteos Sur', 5000.0, 2200.0, 4200.0, 'Excluido', 'unidad', 200, 40, 700, 'P3', 'activo');
