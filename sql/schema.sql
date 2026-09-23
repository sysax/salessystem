-- QtSalesSystem — Esquema SQLite fiel a data/sistema_ventas.db (Python/KivyMD)
-- Colombia COP, IVA 19%, NIT. Compatible con la base existente: NO renombra
-- tablas/columnas. Ejecutar con IF NOT EXISTS + migraciones en DatabaseManager.
-- Origen: data/db.py::init_db + _migrate_users + _migrate_products.

-- Fase 2: diccionario de categorías (los productos guardan cat/subcat como
-- texto plano; esta tabla NO es FK, solo alimenta combos y filtros).
CREATE TABLE IF NOT EXISTS categories (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    parent_id INTEGER REFERENCES categories(id),
    business_type TEXT DEFAULT '',
    sort_order INTEGER DEFAULT 0,
    UNIQUE(name, parent_id)
);

CREATE TABLE IF NOT EXISTS users (
    username TEXT PRIMARY KEY, password TEXT, role TEXT,
    active INTEGER DEFAULT 1, failed_attempts INTEGER DEFAULT 0,
    locked_until TEXT, created_at TEXT, last_login TEXT,
    totp_secret TEXT DEFAULT NULL, totp_enabled INTEGER DEFAULT 0,
    recovery_json TEXT DEFAULT '[]',
    must_change_password INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS products (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sku TEXT UNIQUE, barcode TEXT, name TEXT, description TEXT, cat TEXT, subcat TEXT,
    brand TEXT, supplier TEXT, price REAL, price_buy REAL, price_wholesale REAL,
    tax TEXT, unit TEXT, stock INTEGER, stock_min INTEGER, stock_max INTEGER,
    location TEXT, status TEXT, image TEXT, lote TEXT, vencimiento TEXT,
    is_kit INTEGER DEFAULT 0, kit_json TEXT DEFAULT '[]',
    attrs_json TEXT DEFAULT '{}'
);

CREATE TABLE IF NOT EXISTS clients (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE, nit TEXT, rfc TEXT, nit_dv TEXT, razon TEXT, regimen TEXT,
    responsabilidad TEXT, email TEXT, phone TEXT, address TEXT, city TEXT,
    credit REAL, credit_limit REAL, discount INTEGER, balance REAL, price_list TEXT, status TEXT
);

CREATE TABLE IF NOT EXISTS suppliers (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE, nit TEXT, rfc TEXT, contact TEXT, phone TEXT, email TEXT,
    city TEXT, address TEXT, catalog TEXT, lead_time TEXT, payment_terms TEXT, balance REAL
);

CREATE TABLE IF NOT EXISTS sales (
    id TEXT PRIMARY KEY, date TEXT, client TEXT, vendedor TEXT, total REAL, subtotal REAL,
    tax REAL, discount REAL, promo TEXT, status TEXT, doc_type TEXT, payment TEXT,
    payments_json TEXT, paid REAL, balance REAL, due TEXT, estado TEXT, dian_cufe TEXT, dian_status TEXT,
    tax_breakdown TEXT DEFAULT ''
);

CREATE TABLE IF NOT EXISTS sale_items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sale_id TEXT, product_id INTEGER, qty INTEGER, subtotal REAL,
    attrs_json TEXT DEFAULT '{}', serial TEXT DEFAULT ''
);

-- Fase 3: seriales/IMEI (celulares). status: in_stock|sold|rma|repaired.
CREATE TABLE IF NOT EXISTS serials (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    product_id INTEGER, sku TEXT, serial TEXT UNIQUE,
    status TEXT DEFAULT 'in_stock',
    sale_id TEXT, imei2 TEXT, notes TEXT
);

CREATE TABLE IF NOT EXISTS purchases (
    id TEXT PRIMARY KEY, date TEXT, supplier TEXT, total REAL, status TEXT, items_json TEXT, notes TEXT
);

CREATE TABLE IF NOT EXISTS inventory_movements (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts TEXT, sku TEXT, product TEXT, type TEXT, qty INTEGER, before_qty INTEGER, after_qty INTEGER, reason TEXT, user TEXT
);

CREATE TABLE IF NOT EXISTS payables (
    id TEXT PRIMARY KEY, supplier TEXT, due TEXT, amount REAL, paid REAL, balance REAL, discount_early REAL, status TEXT
);

CREATE TABLE IF NOT EXISTS payments_cxc (
    id INTEGER PRIMARY KEY AUTOINCREMENT, sale_id TEXT, date TEXT, amount REAL, method TEXT, user TEXT
);

CREATE TABLE IF NOT EXISTS payments_cxp (
    id INTEGER PRIMARY KEY AUTOINCREMENT, payable_id TEXT, date TEXT, amount REAL, method TEXT, user TEXT
);

CREATE TABLE IF NOT EXISTS promos (
    id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, type TEXT, value REAL, condition TEXT, code TEXT UNIQUE, active INTEGER, desc TEXT
);

CREATE TABLE IF NOT EXISTS audit_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT, ts TEXT, user TEXT, action TEXT, detail TEXT
);

CREATE TABLE IF NOT EXISTS caja (
    id INTEGER PRIMARY KEY CHECK (id=1), open INTEGER, opening_amount REAL, opening_ts TEXT, opening_user TEXT, sales_today_json TEXT, expected REAL
);
INSERT OR IGNORE INTO caja (id, open, opening_amount, sales_today_json, expected) VALUES (1, 0, 0, '[]', 0);

CREATE TABLE IF NOT EXISTS counters (
    name TEXT PRIMARY KEY, value INTEGER
);

CREATE TABLE IF NOT EXISTS outbox (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    created_ts TEXT,
    op_type TEXT,
    idempotency_key TEXT UNIQUE,
    payload_json TEXT,
    status TEXT DEFAULT 'pending',
    attempts INTEGER DEFAULT 0,
    last_error TEXT,
    synced_ts TEXT
);

CREATE TABLE IF NOT EXISTS settings (
    key TEXT PRIMARY KEY, value TEXT
);
INSERT OR IGNORE INTO settings (key, value) VALUES ('dian_enabled', '0');
INSERT OR IGNORE INTO settings (key, value) VALUES ('dian_provider', 'simulado');
-- Fase 0 multinegocio: perfil del negocio (key/value, sin ALTER).
INSERT OR IGNORE INTO settings (key, value) VALUES ('business_name', 'Mi Negocio');
INSERT OR IGNORE INTO settings (key, value) VALUES ('business_type', 'miscelanea');
INSERT OR IGNORE INTO settings (key, value) VALUES ('business_nit', '');
INSERT OR IGNORE INTO settings (key, value) VALUES ('business_tax_id', '');
INSERT OR IGNORE INTO settings (key, value) VALUES ('business_address', '');
INSERT OR IGNORE INTO settings (key, value) VALUES ('business_phone', '');
INSERT OR IGNORE INTO settings (key, value) VALUES ('business_logo_path', '');
INSERT OR IGNORE INTO settings (key, value) VALUES ('currency_code', 'COP');
INSERT OR IGNORE INTO settings (key, value) VALUES ('currency_symbol', '$');
INSERT OR IGNORE INTO settings (key, value) VALUES ('currency_decimals', '0');
INSERT OR IGNORE INTO settings (key, value) VALUES ('default_tax_rate', '19');
INSERT OR IGNORE INTO settings (key, value) VALUES ('tax_rates_json', '[{"name":"IVA 19%","rate":19},{"name":"Excluido","rate":0}]');
INSERT OR IGNORE INTO settings (key, value) VALUES ('weight_unit_default', 'unidad');
INSERT OR IGNORE INTO settings (key, value) VALUES ('require_expiry', '0');
INSERT OR IGNORE INTO settings (key, value) VALUES ('require_serial', '0');
INSERT OR IGNORE INTO settings (key, value) VALUES ('mora_rate_monthly', '2');

CREATE TABLE IF NOT EXISTS recovery_tokens (
    token TEXT PRIMARY KEY,
    username TEXT,
    created_ts TEXT,
    expires_ts TEXT,
    used INTEGER DEFAULT 0
);

-- Índices (9) — ver data/db.py::init_db
CREATE INDEX IF NOT EXISTS idx_outbox_status ON outbox(status);
CREATE INDEX IF NOT EXISTS idx_products_sku ON products(sku);
CREATE INDEX IF NOT EXISTS idx_products_barcode ON products(barcode);
CREATE INDEX IF NOT EXISTS idx_products_category ON products(cat);
CREATE INDEX IF NOT EXISTS idx_sales_date ON sales(date);
CREATE INDEX IF NOT EXISTS idx_sales_client ON sales(client);
CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
CREATE INDEX IF NOT EXISTS idx_clients_nit ON clients(nit);
CREATE INDEX IF NOT EXISTS idx_inventory_movements_ts ON inventory_movements(ts);

-- Migraciones de columnas para DBs creadas por versiones antiguas (Python):
-- users.active/failed_attempts/locked_until/created_at/last_login/totp_secret/totp_enabled/recovery_json/must_change_password
-- products.image/lote/vencimiento/is_kit/kit_json
-- Se aplican en DatabaseManager::migrate() con PRAGMA table_info, igual que _migrate_users/_migrate_products.
