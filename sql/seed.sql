-- QtSalesSystem — seed inicial de producción.
-- Solo se aplica si users está vacía (DatabaseManager::ensureSeeded).
-- Base limpia: únicamente el usuario admin por defecto (admin/admin123).
-- En el primer ingreso la app exige cambiar la contraseña
-- (users.must_change_password=1). Sin datos demo: el catálogo, clientes,
-- ventas y demás tablas arrancan vacías y se crean operando el sistema.
-- (El fixture con datos demo vive solo para tests en tests/fixtures/seed_demo.sql.)

-- users: 1 fila (admin, clave admin123, cambio obligatorio al primer ingreso)
INSERT OR IGNORE INTO "users" ("username", "password", "role", "active", "failed_attempts", "locked_until", "created_at", "last_login", "totp_secret", "totp_enabled", "recovery_json", "must_change_password") VALUES ('admin', '7302df752724237952f1745814bf5f99$c071827e6a815b28630aae86967b1259144a22bb7abacbce14369bcf942acdbe', 'Administrador', 1, 0, NULL, '2026-09-19T18:45:32', NULL, NULL, 0, '[]', 1);
