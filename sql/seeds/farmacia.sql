-- Seed opt-in: farmacia (Fase 2). Idempotente (INSERT OR IGNORE / OR REPLACE
-- solo en settings). No toca usuarios, ventas ni productos existentes de otros SKUs.
INSERT OR REPLACE INTO settings (key, value) VALUES ('business_type', 'farmacia');
INSERT OR REPLACE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"Excluido","rate":0}]');
INSERT OR REPLACE INTO settings (key, value) VALUES ('default_tax_rate', '19');
INSERT OR REPLACE INTO settings (key, value) VALUES ('require_expiry', '1');

INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Medicamentos', 0, 'farmacia', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Cuidado personal', 0, 'farmacia', 2);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Analgésicos', (SELECT id FROM categories WHERE name='Medicamentos' AND parent_id = 0), 'farmacia', 1);
INSERT OR IGNORE INTO categories (name, parent_id, business_type, sort_order) VALUES ('Antibióticos', (SELECT id FROM categories WHERE name='Medicamentos' AND parent_id = 0), 'farmacia', 2);

INSERT OR IGNORE INTO products (sku, barcode, name, description, cat, subcat, brand, supplier, price, price_buy, price_wholesale, tax, unit, stock, stock_min, stock_max, location, status) VALUES
('FAR01', '7702001000011', 'Acetaminofén 500mg x20', 'Caja x20 tabletas', 'Medicamentos', 'Analgésicos', 'Genfar', 'Distrifarma', 4500.0, 2800.0, 3900.0, 'Excluido', 'caja', 100, 20, 300, 'A1', 'activo'),
('FAR02', '7702001000028', 'Ibuprofeno 400mg x10', 'Caja x10 cápsulas', 'Medicamentos', 'Analgésicos', 'Genfar', 'Distrifarma', 5200.0, 3300.0, 4600.0, 'Excluido', 'caja', 80, 15, 250, 'A1', 'activo'),
('FAR03', '7702001000035', 'Amoxicilina 500mg x21', 'Caja x21 cápsulas', 'Medicamentos', 'Antibióticos', 'La Santé', 'Distrifarma', 18500.0, 13200.0, 16500.0, 'Excluido', 'caja', 40, 10, 120, 'A2', 'activo'),
('FAR04', '7702001000042', 'Alcohol antiséptico 350ml', 'Frasco 350ml', 'Cuidado personal', '', 'MK', 'Distrifarma', 6800.0, 4200.0, 5900.0, 'IVA 19%', 'unidad', 60, 12, 180, 'B1', 'activo'),
('FAR05', '7702001000059', 'Bloqueador solar FPS50', 'Tubo 120g', 'Cuidado personal', '', 'Nivea', 'Distrifarma', 42000.0, 29500.0, 37000.0, 'IVA 19%', 'unidad', 25, 5, 80, 'B2', 'activo'),
('FAR06', '7702001000066', 'Termómetro digital', 'Punta flexible', 'Cuidado personal', '', 'Omron', 'TecnoSalud', 28000.0, 19000.0, 24500.0, 'IVA 19%', 'unidad', 15, 3, 50, 'B3', 'activo');
