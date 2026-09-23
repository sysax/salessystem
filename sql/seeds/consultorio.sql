-- Seed opt-in: consultorio (Fase 2; citas/expediente en Fase 6). Idempotente.
INSERT OR REPLACE INTO settings (key, value) VALUES ('business_type', 'consultorio');
INSERT OR REPLACE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"Excluido","rate":0}]');
INSERT OR REPLACE INTO settings (key, value) VALUES ('default_tax_rate', '0');

INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Consultas', 0, 'consultorio', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Procedimientos', 0, 'consultorio', 2);

INSERT OR IGNORE INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, status) VALUES
('CON01', '7707601000011', 'Consulta general', 'Servicio', 'Consultas', '', 'Casa', '', 80000.0, 0.0, 80000.0, 'Excluido', 'unidad', 999, 0, 9999, '', 'activo'),
('CON02', '7707601000028', 'Consulta especializada', 'Servicio', 'Consultas', '', 'Casa', '', 120000.0, 0.0, 120000.0, 'Excluido', 'unidad', 999, 0, 9999, '', 'activo'),
('CON03', '7707601000035', 'Electrocardiograma', 'Procedimiento', 'Procedimientos', '', 'Casa', '', 90000.0, 0.0, 90000.0, 'Excluido', 'unidad', 999, 0, 9999, '', 'activo'),
('CON04', '7707601000042', 'Certificado médico', 'Documento', 'Consultas', '', 'Casa', '', 30000.0, 0.0, 30000.0, 'Excluido', 'unidad', 999, 0, 9999, '', 'activo');
