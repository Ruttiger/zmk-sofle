# Known-good firmware

This directory stores firmware sets that are intentionally kept for emergency recovery.

Current base set:

- `base-local-wsl-20260515-133924/base_emergency_eyelash_sofle_right.uf2`
- `base-local-wsl-20260515-133924/base_emergency_eyelash_sofle_studio_left.uf2`
- `base-local-wsl-20260515-133924/base_emergency_settings_reset.uf2`

Use PowerShell flashing with an explicit firmware directory, for example:

```powershell
.\scripts\Flash-Firmware.ps1 -Side left -FirmwareDir firmware\known-good\base-local-wsl-20260515-133924
.\scripts\Flash-Firmware.ps1 -Side right -FirmwareDir firmware\known-good\base-local-wsl-20260515-133924
.\scripts\Flash-Firmware.ps1 -Side reset -FirmwareDir firmware\known-good\base-local-wsl-20260515-133924
```

Check `manifest.json` before flashing if you need SHA256 verification.

