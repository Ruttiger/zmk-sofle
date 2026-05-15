# Pipeline local ZMK para Eyelash Sofle

Este repo incluye un pipeline local para Windows 11 y PowerShell. La ruta recomendada de build usa GitHub Actions mediante GitHub CLI, porque el repo ya tiene un workflow ZMK funcional.

## Requisitos

- Windows 11
- PowerShell o PowerShell 7 (`pwsh`)
- Git
- GitHub CLI (`gh`)
- VS Code

El pipeline no instala herramientas automaticamente. Si falta algo, la validacion lo reporta con instrucciones.

## Setup inicial

1. Instala GitHub CLI si no existe.
2. Autentica GitHub CLI:

   ```powershell
   gh auth login
   ```

3. Abre este repo en VS Code.
4. Ejecuta la tarea `ZMK: Validate repo`.

Si Git muestra `dubious ownership`, puedes corregir tu entorno con:

```powershell
git config --global --add safe.directory D:/Projects/zmk-sofle-ali/zmk-sofle
```

Los scripts usan una configuracion segura por comando y no modifican tu Git global automaticamente.

## Flujo normal

1. Edita el keymap o la configuracion ZMK.
2. Ejecuta `ZMK: Backup restore point`.
3. Ejecuta `ZMK: Build firmware via GitHub Actions`.
4. Flashea izquierda con `ZMK: Flash left half`.
5. Prueba por USB.
6. Flashea derecha con `ZMK: Flash right half`.
7. Prueba split y BLE.

Los firmware generados se copian a `firmware/latest/` y se documentan en `firmware/latest/manifest.json` con hashes SHA256.

## Flujo seguro antes de experimentar

Ejecuta un restore point antes de cambios grandes:

```powershell
.\scripts\New-RestorePoint.ps1 -TagKnownGood
```

Esto crea una carpeta `firmware/backups/YYYYMMDD-HHMMSS/` con:

- `build.yaml`
- `config/`
- `.github/workflows/build.yml`
- estado, commit y diffs de Git
- artifacts UF2 previos de `firmware/latest/`, si existen
- `manifest.json` con metadatos y hashes SHA256

El tag `known-good/YYYYMMDD-HHMMSS` es local. Empujalo manualmente si quieres conservarlo en remoto.

## Build GitHub Actions

El comando principal es:

```powershell
.\scripts\Build-Firmware.ps1
```

Opciones utiles:

```powershell
.\scripts\Build-Firmware.ps1 -CommitMessage "keymap: tune layout" -Push
.\scripts\Build-Firmware.ps1 -NoBackup
```

Si `firmware/latest/` ya contiene `.uf2`, el script crea un backup de seguridad antes de reemplazarlos.

## Build local con WSL

Tambien puedes construir el firmware localmente usando WSL. En ese flujo, PowerShell sigue haciendo backup, inventario y flasheo, mientras que WSL ejecuta `west build`.

Primero instala una distro WSL, por ejemplo Ubuntu:

```powershell
wsl --install -d Ubuntu
```

Despues reinicia si Windows lo pide, abre Ubuntu al menos una vez y crea tu usuario Linux. Desde VS Code o PowerShell, valida:

```powershell
.\scripts\Build-FirmwareLocal.ps1 -ValidateOnly
```

Si Ubuntu funciona en Windows Terminal pero el script dice que no hay distros instaladas, revisa desde esa misma terminal:

```powershell
wsl -l -v
whoami
```

WSL registra las distros por usuario de Windows. Ejecuta las tasks de VS Code desde tu sesion normal de Windows, no desde un proceso elevado o sandbox que use otro usuario.

El setup local recomendado usa un checkout ZMK dentro de WSL en `~/zmk`:

```powershell
.\scripts\Build-FirmwareLocal.ps1 -Setup
```

Ese setup clona `zmkfirmware/zmk`, crea `.venv`, instala `west`, ejecuta `west update`, `west zephyr-export` y `west packages pip --install`. No modifica `west.yml`, `build.yaml` ni el keymap de este repo.

El checkout ZMK se fija por defecto a `v0.3.0`, igual que el workflow actual del repo.

Si faltan dependencias de sistema en Ubuntu, el script muestra el comando `apt` sugerido. La Zephyr SDK se instala de forma explicita con:

```bash
bash scripts/wsl/install-zephyr-sdk-wsl.sh
```

Por defecto instala Zephyr SDK `0.16.8` en `~/zephyr-sdk-0.16.8` con la toolchain `arm-zephyr-eabi`, suficiente para nice!nano/nRF.

Para compilar localmente:

```powershell
.\scripts\Build-FirmwareLocal.ps1
```

El build se ejecuta desde `~/zmk/app` y usa esta configuracion con `-DZMK_CONFIG=<ruta-wsl-del-repo>/config` y `-DZMK_EXTRA_MODULES=<ruta-wsl-del-repo>`, necesario para que ZMK encuentre la placa custom `eyelash_sofle`. Produce:

- `firmware/latest/eyelash_sofle_right.uf2`
- `firmware/latest/eyelash_sofle_studio_left.uf2`
- `firmware/latest/settings_reset.uf2`
- `firmware/latest/manifest.json`

Tambien hay tareas VS Code:

- `ZMK: WSL validate local build env`
- `ZMK: WSL setup local build env`
- `ZMK: WSL build firmware local`

El flasheo sigue siendo PowerShell/Windows. WSL no se usa para copiar a la unidad UF2 porque Windows es quien normalmente monta el teclado como disco extraible.

## Flasheo

Los comandos de VS Code son interactivos y no copian si hay ambiguedad:

- `ZMK: Flash left half`
- `ZMK: Flash right half`
- `ZMK: Flash settings reset`

Tambien puedes ejecutar:

```powershell
.\scripts\Flash-Firmware.ps1 -Side left
.\scripts\Flash-Firmware.ps1 -Side right
.\scripts\Flash-Firmware.ps1 -Side reset
```

Usa `-WhatIf` para comprobar que archivo seleccionaria sin copiarlo:

```powershell
.\scripts\Flash-Firmware.ps1 -Side left -WhatIf
```

## Recuperacion

Ejecuta:

```powershell
.\scripts\Recover-Keyboard.ps1
```

Opciones:

- Reflashear ultimo firmware bueno izquierdo.
- Reflashear ultimo firmware bueno derecho.
- Flashear `settings_reset` en ambas mitades.

`settings_reset` borra perfiles BLE, bonds y settings persistentes. Despues de usarlo, vuelve a flashear el firmware normal en cada mitad.

## Inventario

Lista artifacts y hashes:

```powershell
.\scripts\Get-FirmwareInventory.ps1
```

## Limitaciones importantes

No se puede garantizar una lectura o backup fiable del firmware ya grabado en el teclado. El backup fiable es el repo, los artifacts `.uf2`, los manifiestos y los hashes SHA256.

Si Windows no monta el teclado como unidad UF2:

- Desconecta y reconecta el USB.
- Repite el doble toque del boton reset o el procedimiento de bootloader de tu placa.
- Prueba otro cable USB que transmita datos.
- Revisa el Administrador de dispositivos.
- Evita hubs USB durante recuperacion.

El script de flasheo espera una unidad que contenga `INFO_UF2.TXT` o que parezca un volumen UF2/bootloader. Si detecta varias, pide seleccion.
