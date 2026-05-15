[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = "High")]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("left", "right", "reset")]
    [string] $Side,

    [string] $FirmwareDir = "firmware/latest"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\_ZmkPipeline.Common.ps1"
trap {
    Write-Host ""
    Write-Host "ERROR: $($_.Exception.Message)"
    exit 1
}

function Select-FirmwareFile {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Directory,

        [Parameter(Mandatory = $true)]
        [string] $TargetSide
    )

    if (-not (Test-Path -LiteralPath $Directory)) {
        throw "Firmware directory not found: $Directory"
    }

    $files = @(Get-ChildItem -LiteralPath $Directory -Filter "*.uf2" -File -ErrorAction SilentlyContinue)
    $matches = switch ($TargetSide) {
        "left"  { @($files | Where-Object { $_.Name.ToLowerInvariant() -match "left|studio_left" }) }
        "right" { @($files | Where-Object { $_.Name.ToLowerInvariant() -match "right" }) }
        "reset" { @($files | Where-Object { $_.Name.ToLowerInvariant() -match "settings_reset" }) }
    }

    if ($matches.Count -eq 0) {
        throw "No .uf2 firmware matched side '$TargetSide' in $Directory."
    }

    if ($matches.Count -eq 1) {
        return $matches[0]
    }

    Write-Host "Multiple firmware files match '$TargetSide':"
    for ($i = 0; $i -lt $matches.Count; $i++) {
        Write-Host ("[{0}] {1}" -f ($i + 1), $matches[$i].FullName)
    }
    $selection = Read-Host "Select firmware number"
    $index = 0
    if (-not [int]::TryParse($selection, [ref]$index) -or $index -lt 1 -or $index -gt $matches.Count) {
        throw "Invalid firmware selection."
    }
    return $matches[$index - 1]
}

function Get-Uf2Volumes {
    $logicalDisks = @(Get-CimInstance Win32_LogicalDisk | Where-Object { $_.DriveType -eq 2 -or $_.DriveType -eq 3 })
    foreach ($disk in $logicalDisks) {
        if ([string]::IsNullOrWhiteSpace($disk.DeviceID)) {
            continue
        }

        $rootPath = "$($disk.DeviceID)\"
        $infoPath = Join-Path -Path $rootPath -ChildPath "INFO_UF2.TXT"
        $label = [string]$disk.VolumeName
        $looksLikeUf2 = (Test-Path -LiteralPath $infoPath) -or ($label -match "UF2|NICENANO|NICE_NANO|BOOT|BOOTLOADER|RPI-RP2")

        if ($looksLikeUf2) {
            [pscustomobject]@{
                Drive      = $disk.DeviceID
                Root       = $rootPath
                VolumeName = $label
                HasInfoUf2 = Test-Path -LiteralPath $infoPath
            }
        }
    }
}

function Select-Uf2Volume {
    Write-Host ""
    Write-Host "Put the target half into bootloader mode now."
    Write-Host "Press Enter when Windows has mounted the UF2 drive."
    Read-Host | Out-Null

    $volumes = @(Get-Uf2Volumes)
    while ($volumes.Count -eq 0) {
        Write-Host "No UF2 drive detected yet."
        $again = Read-Host "Press Enter to scan again, or type Q to quit"
        if ($again -match "^[Qq]$") {
            throw "Flashing cancelled; no UF2 drive selected."
        }
        $volumes = @(Get-Uf2Volumes)
    }

    if ($volumes.Count -eq 1) {
        return $volumes[0]
    }

    Write-Host "Multiple UF2-like drives detected:"
    for ($i = 0; $i -lt $volumes.Count; $i++) {
        Write-Host ("[{0}] {1} {2} INFO_UF2={3}" -f ($i + 1), $volumes[$i].Drive, $volumes[$i].VolumeName, $volumes[$i].HasInfoUf2)
    }
    $selection = Read-Host "Select target drive number"
    $index = 0
    if (-not [int]::TryParse($selection, [ref]$index) -or $index -lt 1 -or $index -gt $volumes.Count) {
        throw "Invalid drive selection. No firmware was copied."
    }
    return $volumes[$index - 1]
}

$root = Assert-ZmkRepoRoot
$resolvedFirmwareDir = if ([System.IO.Path]::IsPathRooted($FirmwareDir)) { $FirmwareDir } else { Join-Path $root $FirmwareDir }
$firmware = Select-FirmwareFile -Directory $resolvedFirmwareDir -TargetSide $Side

Write-ZmkSection "Flash firmware"
Write-Host "Side: $Side"
Write-Host "Firmware: $($firmware.FullName)"

if ($WhatIfPreference) {
    Write-Host "WhatIf: firmware selection succeeded. No UF2 drive will be scanned and no file will be copied."
    exit 0
}

$target = Select-Uf2Volume
$destination = Join-Path -Path $target.Root -ChildPath $firmware.Name

if ($PSCmdlet.ShouldProcess($target.Root, "Copy $($firmware.Name)")) {
    Copy-Item -LiteralPath $firmware.FullName -Destination $destination -Force
    Start-Sleep -Milliseconds 500
    if (Test-Path -LiteralPath $destination) {
        Write-Host "Copy completed and file is visible on the UF2 drive."
    }
    else {
        Write-Host "Copy command completed. The UF2 drive may already have unmounted itself, which is normal."
    }
}

Write-Host ""
Write-Host "If the copy succeeded, the bootloader usually unmounts automatically."
exit 0
