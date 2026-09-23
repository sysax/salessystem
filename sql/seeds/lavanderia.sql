-- Seed opt-in: lavandería (Fase 2). Idempotente. No toca usuarios/ventas existentes.
INSERT OR REPLACE INTO settings (key, value) VALUES ('business_type', 'lavanderia');
INSERT OR REPLACE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"Excluido","rate":0}]');
INSERT OR REPLACE INTO settings (key, value) VALUES ('default_tax_rate', '19');
INSERT OR REPLACE INTO settings (key, value) VALUES ('weight_unit_default', 'kg');

INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Por kilo', 0, 'lavanderia', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Por prenda', 0, 'lavanderia', 2);

INSERT OR IGNORE INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, status) VALUES
('LAV01', '7707501000011', 'Lavado por kilo', 'Precio por kilo', 'Por kilo', '', 'Casa', '', 6000.0, 0.0, 6000.0, 'IVA 19%', 'kg', 999.0, 0.0, 9999.0, '', 'activo'),
('LAV02', '7707501000028', 'Planchado camisa', 'Por prenda', 'Por prenda', '', 'Casa', '', 5000.0, 0.0, 5000.0, 'IVA 19%', 'unidad', 999, 0, 9999, '', 'activo'),
('LAV03', '7707501000035', 'Lavado tenis', 'Par', 'Por prenda', '', 'Casa', '', 15000.0, 0.0, 15000.0, 'IVA 19%', 'unidad', 999, 0, 9999, '', 'activo'),
('LAV04', '7707501000042', 'Lavado edredón', 'Por unidad', 'Por prenda', '', 'Casa', '', 25000.0, 0.0, 25000.0, 'IVA 19%', 'unidad', 999, 0, 9999, '', 'activo');
