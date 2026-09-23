-- Seed opt-in: ferretería (Fase 2). Idempotente. No toca usuarios/ventas existentes.
INSERT OR REPLACE INTO settings (key, value) VALUES ('business_type', 'ferreteria');
INSERT OR REPLACE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"Excluido","rate":0}]');
INSERT OR REPLACE INTO settings (key, value) VALUES ('default_tax_rate', '19');
INSERT OR REPLACE INTO settings (key, value) VALUES ('weight_unit_default', 'metro');

INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Herramientas', 0, 'ferreteria', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Tornillería', 0, 'ferreteria', 2);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Pinturas', 0, 'ferreteria', 3);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Eléctricos', 0, 'ferreteria', 4);

INSERT OR IGNORE INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, status) VALUES
('FER01', '7705001000011', 'Martillo carpintero 16oz', 'Mango fibra', 'Herramientas', '', 'Stanley', 'FerreSur', 42000.0, 30000.0, 37000.0, 'IVA 19%', 'unidad', 20, 4, 60, 'F1', 'activo'),
('FER02', '7705001000028', 'Tornillo drywall x100', 'Caja x100 und', 'Tornillería', '', 'Genérico', 'FerreSur', 9500.0, 6000.0, 8200.0, 'IVA 19%', 'caja', 60, 10, 200, 'F2', 'activo'),
('FER03', '7705001000035', 'Cable eléctrico m', 'Calibre 12 por metro', 'Eléctricos', '', 'Procables', 'FerreSur', 2800.0, 1900.0, 2400.0, 'IVA 19%', 'metro', 200.0, 30.0, 600.0, 'F3', 'activo'),
('FER04', '7705001000042', 'Pintura blanca galón', 'Tipo 1 lavable', 'Pinturas', '', 'Pintuco', 'FerreSur', 89000.0, 68000.0, 79000.0, 'IVA 19%', 'unidad', 15, 3, 50, 'F4', 'activo'),
('FER05', '7705001000059', 'Cinta aislante', 'Rollo 20m', 'Eléctricos', '', '3M', 'FerreSur', 8500.0, 5200.0, 7300.0, 'IVA 19%', 'unidad', 50, 10, 150, 'F3', 'activo');
