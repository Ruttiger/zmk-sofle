[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [ValidateSet("validate", "backup", "build", "flash-left", "flash-right", "flash-reset", "inventory", "recover")]
    [string] $Action
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\_ZmkPipeline.Common.ps1"
trap {
    Write-Host ""
    Write-Host "ERROR: $($_.Exception.Message)"
    exit 1
}

Assert-ZmkRepoRoot | Out-Null

switch ($Action) {
    "validate"    { & "$PSScriptRoot\Test-ZmkRepo.ps1" }
    "backup"      { & "$PSScriptRoot\New-RestorePoint.ps1" }
    "build"       { & "$PSScriptRoot\Build-Firmware.ps1" }
    "flash-left"  { & "$PSScriptRoot\Flash-Firmware.ps1" -Side left -Confirm }
    "flash-right" { & "$PSScriptRoot\Flash-Firmware.ps1" -Side right -Confirm }
    "flash-reset" { & "$PSScriptRoot\Flash-Firmware.ps1" -Side reset -Confirm }
    "inventory"   { & "$PSScriptRoot\Get-FirmwareInventory.ps1" }
    "recover"     { & "$PSScriptRoot\Recover-Keyboard.ps1" }
}

exit $LASTEXITCODE
