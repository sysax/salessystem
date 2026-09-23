-- Seed opt-in: peluquería (Fase 2; citas en Fase 6). Servicios como ítems sin stock
-- se modelan en Fase 6; aquí solo retail de mostrador. Idempotente.
INSERT OR REPLACE INTO settings (key, value) VALUES ('business_type', 'peluqueria');
INSERT OR REPLACE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"Excluido","rate":0}]');
INSERT OR REPLACE INTO settings (key, value) VALUES ('default_tax_rate', '19');

INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Servicios', 0, 'peluqueria', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Retail capilar', 0, 'peluqueria', 2);

INSERT OR IGNORE INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, status) VALUES
('PEL01', '7707301000011', 'Corte caballero', 'Servicio mostrador', 'Servicios', '', 'Casa', '', 25000.0, 0.0, 25000.0, 'IVA 19%', 'unidad', 999, 0, 9999, '', 'activo'),
('PEL02', '7707301000028', 'Tinte + corte', 'Servicio mostrador', 'Servicios', '', 'Casa', '', 120000.0, 0.0, 120000.0, 'IVA 19%', 'unidad', 999, 0, 9999, '', 'activo'),
('PEL03', '7707301000035', 'Shampoo keratina 500ml', 'Retail', 'Retail capilar', '', 'Loreal', 'BellezaPro', 55000.0, 34000.0, 48000.0, 'IVA 19%', 'unidad', 25, 5, 80, 'E1', 'activo'),
('PEL04', '7707301000042', 'Cera mate 100g', 'Retail', 'Retail capilar', '', 'EG', 'BellezaPro', 28000.0, 16000.0, 24000.0, 'IVA 19%', 'unidad', 30, 6, 100, 'E1', 'activo'),
('PEL05', '7707301000059', 'Afeitada clásica', 'Servicio mostrador', 'Servicios', '', 'Casa', '', 18000.0, 0.0, 18000.0, 'IVA 19%', 'unidad', 999, 0, 9999, '', 'activo');
