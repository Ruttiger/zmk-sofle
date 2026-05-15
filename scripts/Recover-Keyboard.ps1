[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\_ZmkPipeline.Common.ps1"
trap {
    Write-Host ""
    Write-Host "ERROR: $($_.Exception.Message)"
    exit 1
}

Assert-ZmkRepoRoot | Out-Null

Write-ZmkSection "Eyelash Sofle recovery wizard"
Write-Host "1. Reflash last known left firmware"
Write-Host "2. Reflash last known right firmware"
Write-Host "3. Flash settings_reset on both halves"
Write-Host "Q. Quit"
Write-Host ""
Write-Host "settings_reset erases saved BLE bonds, profiles, and persistent ZMK settings."
Write-Host "After settings_reset, flash the normal firmware again on each half."
Write-Host ""

$choice = Read-Host "Choose an option"
switch ($choice) {
    "1" {
        & "$PSScriptRoot\Flash-Firmware.ps1" -Side left -Confirm
    }
    "2" {
        & "$PSScriptRoot\Flash-Firmware.ps1" -Side right -Confirm
    }
    "3" {
        Write-Host ""
        Write-Host "First flash settings_reset to the left half."
        & "$PSScriptRoot\Flash-Firmware.ps1" -Side reset -Confirm
        Write-Host ""
        Write-Host "Now flash settings_reset to the right half."
        & "$PSScriptRoot\Flash-Firmware.ps1" -Side reset -Confirm
        Write-Host ""
        Write-Host "Now flash normal left and right firmware again."
    }
    { $_ -match "^[Qq]$" } {
        Write-Host "Recovery cancelled."
    }
    default {
        throw "Invalid recovery option."
    }
}

exit 0
