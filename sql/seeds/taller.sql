-- Seed opt-in: taller (Fase 2; órdenes de servicio en Fase 6). Idempotente.
INSERT OR REPLACE INTO settings (key, value) VALUES ('business_type', 'taller');
INSERT OR REPLACE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"Excluido","rate":0}]');
INSERT OR REPLACE INTO settings (key, value) VALUES ('default_tax_rate', '19');

INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Mano de obra', 0, 'taller', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Repuestos', 0, 'taller', 2);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Lubricantes', 0, 'taller', 3);

INSERT OR IGNORE INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, status) VALUES
('TAL01', '7707401000011', 'Hora de mano de obra', 'Servicio taller', 'Mano de obra', '', 'Casa', '', 60000.0, 0.0, 60000.0, 'IVA 19%', 'unidad', 999, 0, 9999, '', 'activo'),
('TAL02', '7707401000028', 'Pastillas freno', 'Juego delantero', 'Repuestos', '', 'Brembo', 'RepuestosYA', 95000.0, 62000.0, 82000.0, 'IVA 19%', 'unidad', 14, 3, 50, 'T1', 'activo'),
('TAL03', '7707401000035', 'Aceite 20W50 litro', 'Litro', 'Lubricantes', '', 'Mobil', 'RepuestosYA', 32000.0, 22000.0, 28000.0, 'IVA 19%', 'l', 60.0, 12.0, 200.0, 'T2', 'activo'),
('TAL04', '7707401000042', 'Filtro aceite', 'Unidad', 'Repuestos', '', 'Fram', 'RepuestosYA', 18000.0, 11000.0, 15000.0, 'IVA 19%', 'unidad', 40, 8, 120, 'T1', 'activo'),
('TAL05', '7707401000059', 'Alineación + balanceo', 'Servicio taller', 'Mano de obra', '', 'Casa', '', 80000.0, 0.0, 80000.0, 'IVA 19%', 'unidad', 999, 0, 9999, '', 'activo');
