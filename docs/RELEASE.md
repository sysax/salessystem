# Release (Fase 8)

Releases reproducibles desde tag `v*` (ej. `v1.0.0`).

## Procedimiento

```bash
git tag v1.0.0 && git push origin v1.0.0
# CI genera: changelog + binarios (Linux/Windows/macOS) + instaladores CPack
cmake --build build --target changelog  # regenera CHANGELOG.md local
```

## Qué hace el workflow `release.yml`

1. Verifica que el tag coincide con `project(VERSION)` + `AppVersion::kVersion`.
2. Genera changelog desde `git log` (últimos 100 commits).
3. Compila en Linux/Windows/macOS y empaqueta con CPack (TGZ/ZIP, DEB en Linux).
4. Sube artefactos al GitHub Release.

## Instaladores firmados

- **Windows**: firma con certificado en secreto `CERT_PFX` (base64) + `CERT_PASSWORD`.
  Sin secretos, el `.zip` sale **sin firmar** (el workflow lo marca en las notas).
- **macOS**: firma + notarización con `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD`.
  Sin secretos, la `.app` sale sin firmar (Gatekeeper avisará; documentado en el release).

## Versionado

SemVer (`MAYOR.MENOR.PARCHE`). Subir versión implica tocar a la vez:

- `CMakeLists.txt` → `project(... VERSION x.y.z)`
- `src/core/Version.h` → `kVersion`
- `CHANGELOG.md` (vía target `changelog`)
