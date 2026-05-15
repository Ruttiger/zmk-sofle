[CmdletBinding()]
param(
    [string] $FirmwareDir = "firmware"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\_ZmkPipeline.Common.ps1"
trap {
    Write-Host ""
    Write-Host "ERROR: $($_.Exception.Message)"
    exit 1
}

$root = Assert-ZmkRepoRoot
$target = if ([System.IO.Path]::IsPathRooted($FirmwareDir)) { $FirmwareDir } else { Join-Path $root $FirmwareDir }

if (-not (Test-Path -LiteralPath $target)) {
    Write-Host "Firmware directory not found: $target"
    exit 0
}

$files = @(Get-ChildItem -LiteralPath $target -Filter "*.uf2" -File -Recurse -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending)
if ($files.Count -eq 0) {
    Write-Host "No .uf2 files found under $target."
    exit 0
}

$rows = foreach ($file in $files) {
    $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
    [pscustomobject]@{
        Name         = $file.Name
        Path         = Resolve-ZmkRelativePath -Root $root -Path $file.FullName
        LastWrite    = $file.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss")
        SizeBytes    = $file.Length
        SHA256       = $hash.Hash.ToLowerInvariant()
    }
}

$rows | Format-Table -AutoSize
exit 0
