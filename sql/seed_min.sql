-- QtSalesSystem — seed mínimo (modo sin demo: QTSALES_SIN_DEMO=1).
-- Idéntico al seed normal: base limpia solo con el usuario admin por defecto
-- (admin/admin123, cambio obligatorio al primer ingreso). Cero datos de negocio
-- (sin productos, clientes, ventas, compras ni promos). Los contadores
-- (counters) arrancan en 1 automáticamente al no existir (Counters::next).

-- users: 1 fila (admin, clave admin123, cambio obligatorio al primer ingreso)
INSERT OR IGNORE INTO "users" ("username", "password", "role", "active", "failed_attempts", "locked_until", "created_at", "last_login", "totp_secret", "totp_enabled", "recovery_json", "must_change_password") VALUES ('admin', '7302df752724237952f1745814bf5f99$c071827e6a815b28630aae86967b1259144a22bb7abacbce14369bcf942acdbe', 'Administrador', 1, 0, NULL, '2026-09-19T18:45:32', NULL, NULL, 0, '[]', 1);
