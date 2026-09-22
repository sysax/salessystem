-- QtSalesSystem — seed mínimo (modo sin demo: QTSALES_SIN_DEMO=1).
-- Solo cuentas de usuario para operar/probar roles; cero datos de negocio
-- (sin productos, clientes, ventas, compras ni promos). Los contadores
-- (counters) arrancan en 1 automáticamente al no existir (Counters::next).
-- Las claves son las documentadas en README (admin/admin123, etc.).

-- users: 5 filas
INSERT OR IGNORE INTO "users" ("username", "password", "role", "active", "failed_attempts", "locked_until", "created_at", "last_login", "totp_secret", "totp_enabled", "recovery_json") VALUES ('admin', '7302df752724237952f1745814bf5f99$c071827e6a815b28630aae86967b1259144a22bb7abacbce14369bcf942acdbe', 'Administrador', 1, 0, NULL, '2026-09-19T18:45:32', '2026-09-19T16:23:33', NULL, 0, '[]');
INSERT OR IGNORE INTO "users" ("username", "password", "role", "active", "failed_attempts", "locked_until", "created_at", "last_login", "totp_secret", "totp_enabled", "recovery_json") VALUES ('vendedor', '8bbe3e495f9d6c232e290b4b93b0dcf2$55989dc708df31c3f8b9077847368522e26fa2a8504a1f2d4282089e36a66a2a', 'Vendedor', 1, 0, NULL, '2026-09-19T18:45:32', NULL, NULL, 0, '[]');
INSERT OR IGNORE INTO "users" ("username", "password", "role", "active", "failed_attempts", "locked_until", "created_at", "last_login", "totp_secret", "totp_enabled", "recovery_json") VALUES ('cajero', '64cf991542737e22b9d7efd2b3975b38$da3cb6e24f6c75d1550bf0a565e16f9c9d794ff4961ae4605db499b5233aa363', 'Cajero', 1, 0, NULL, '2026-09-19T18:45:32', NULL, NULL, 0, '[]');
INSERT OR IGNORE INTO "users" ("username", "password", "role", "active", "failed_attempts", "locked_until", "created_at", "last_login", "totp_secret", "totp_enabled", "recovery_json") VALUES ('almacen', '5f7b906cb660287bd8a4b3c780d5d2ff$c0467e7e1b7ecd416d00b3584a4f255057710d3bbb58bd73bbbb5843ebdb0d1d', 'Almacén', 1, 0, NULL, '2026-09-19T18:45:32', NULL, NULL, 0, '[]');
INSERT OR IGNORE INTO "users" ("username", "password", "role", "active", "failed_attempts", "locked_until", "created_at", "last_login", "totp_secret", "totp_enabled", "recovery_json") VALUES ('contador', 'a6294442ef9b4c02f242fae08150638d$8c65236458ae2d2ae64327fb3ed0a3664a02a9dc7ba19e0fec8d33bc3af0a', 'Contador', 1, 0, NULL, '2026-09-19T18:45:32', NULL, NULL, 0, '[]');
