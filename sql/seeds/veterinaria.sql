-- Seed opt-in: veterinaria (Fase 2). Idempotente. No toca usuarios/ventas existentes.
INSERT OR REPLACE INTO settings (key, value) VALUES ('business_type', 'veterinaria');
INSERT OR REPLACE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"IVA 5%","rate":5},{"name":"Excluido","rate":0}]');
INSERT OR REPLACE INTO settings (key, value) VALUES ('default_tax_rate', '19');

INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Consultas', 0, 'veterinaria', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Concentrados', 0, 'veterinaria', 2);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Farmacia vet', 0, 'veterinaria', 3);

INSERT OR IGNORE INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, status) VALUES
('VET01', '7707701000011', 'Consulta general', 'Servicio', 'Consultas', '', 'Casa', '', 60000.0, 0.0, 60000.0, 'Excluido', 'unidad', 999, 0, 9999, '', 'activo'),
('VET02', '7707701000028', 'Concentrado adulto 8kg', 'Bulto 8kg', 'Concentrados', '', 'Dog Chow', 'VetSur', 95000.0, 72000.0, 84000.0, 'IVA 5%', 'paquete', 30, 6, 100, 'V1', 'activo'),
('VET03', '7707701000035', 'Desparasitante', 'Tableta', 'Farmacia vet', '', 'Bayer', 'VetSur', 18000.0, 11000.0, 15000.0, 'Excluido', 'unidad', 60, 12, 200, 'V2', 'activo'),
('VET04', '7707701000042', 'Vacuna pentavalente', 'Dosis + aplicación', 'Farmacia vet', '', 'Zoetis', 'VetSur', 55000.0, 36000.0, 48000.0, 'Excluido', 'unidad', 40, 8, 120, 'V2', 'activo');
