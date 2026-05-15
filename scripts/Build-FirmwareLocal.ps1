[CmdletBinding()]
param(
    [switch] $NoBackup,
    [switch] $ValidateOnly,
    [switch] $Setup,
    [string] $Distro,
    [string] $ZmkRoot = "~/zmk",
    [string] $ZmkRevision = "v0.3.0"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\_ZmkPipeline.Common.ps1"
trap {
    Write-Host ""
    Write-Host "ERROR: $($_.Exception.Message)"
    exit 1
}

function Assert-WslAvailable {
    if (-not (Test-ZmkCommand -Name "wsl.exe")) {
        throw "wsl.exe was not found. Install WSL first: wsl --install -d Ubuntu"
    }

    $list = Invoke-ZmkNative -FilePath "wsl.exe" -Arguments @("-l", "-q")
    if ($list.ExitCode -ne 0 -or [string]::IsNullOrWhiteSpace($list.Text)) {
        $who = Invoke-ZmkNative -FilePath "whoami" -Arguments @()
        throw "WSL is installed, but no Linux distribution is visible to this Windows process ($($who.Text)). WSL distributions are registered per Windows user. If Ubuntu works in your normal terminal, run the VS Code task from your normal user session; otherwise install Ubuntu with: wsl --install -d Ubuntu"
    }

    return @($list.Output | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
}

function Invoke-Wsl {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Command
    )

    $arguments = @()
    if (-not [string]::IsNullOrWhiteSpace($Distro)) {
        $arguments += @("-d", $Distro)
    }
    $arguments += @("bash", "-lc", $Command)

    $result = Invoke-ZmkNative -FilePath "wsl.exe" -Arguments $arguments
    if ($result.ExitCode -ne 0) {
        throw "WSL command failed with exit code $($result.ExitCode). $($result.Text)"
    }
    return $result
}

function ConvertTo-WslPath {
    param(
        [Parameter(Mandatory = $true)]
        [string] $WindowsPath
    )

    $escaped = $WindowsPath.Replace("'", "'\''")
    $result = Invoke-Wsl -Command "wslpath -a '$escaped'"
    return $result.Text.Trim()
}

function New-LocalFirmwareManifest {
    param(
        [Parameter(Mandatory = $true)]
        [string] $LatestDir,

        [Parameter(Mandatory = $true)]
        [string] $BuildMode
    )

    $root = Get-ZmkRepoRoot
    $files = @(Get-ChildItem -LiteralPath $LatestDir -Filter "*.uf2" -File -ErrorAction SilentlyContinue | Sort-Object Name)
    if ($files.Count -eq 0) {
        throw "Local WSL build completed, but no .uf2 files were found in $LatestDir."
    }

    $items = @(
        foreach ($file in $files) {
            $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
            [pscustomobject]@{
                artifactName = "local-wsl"
                fileName     = $file.Name
                path         = Resolve-ZmkRelativePath -Root $root -Path $file.FullName
                sizeBytes    = $file.Length
                sha256       = $hash.Hash.ToLowerInvariant()
            }
        }
    )

    $manifest = [pscustomobject]@{
        createdAtUtc = (Get-Date).ToUniversalTime().ToString("o")
        buildMode    = $BuildMode
        runner       = "wsl"
        branch       = Get-ZmkGitBranch
        commit       = Get-ZmkGitCommit
        files        = $items
    }

    Write-ZmkJson -InputObject $manifest -Path (Join-Path $LatestDir "manifest.json")
}

$root = Assert-ZmkRepoRoot
$distros = Assert-WslAvailable

Write-ZmkSection "WSL local build"
Write-Host "Repository root: $root"
Write-Host "WSL distributions: $($distros -join ', ')"
if (-not [string]::IsNullOrWhiteSpace($Distro)) {
    Write-Host "Selected distro: $Distro"
}
Write-Host "ZMK root inside WSL: $ZmkRoot"
Write-Host "ZMK revision: $ZmkRevision"

$repoWslPath = ConvertTo-WslPath -WindowsPath $root
$latestDir = Join-ZmkPath -Root $root -Parts @("firmware", "latest")
New-ZmkDirectory -Path $latestDir

if ($ValidateOnly) {
    $script = "$repoWslPath/scripts/wsl/validate-wsl-zmk.sh"
    Invoke-Wsl -Command "bash '$script' --repo '$repoWslPath' --zmk-root '$ZmkRoot'"
    exit 0
}

if ($Setup) {
    $script = "$repoWslPath/scripts/wsl/setup-zmk-wsl.sh"
    Invoke-Wsl -Command "bash '$script' --repo '$repoWslPath' --zmk-root '$ZmkRoot' --zmk-revision '$ZmkRevision'"
    exit 0
}

if (-not $NoBackup) {
    & "$PSScriptRoot\New-RestorePoint.ps1"
    if ($LASTEXITCODE -ne 0) {
        throw "Backup failed; local build aborted."
    }
}

$script = "$repoWslPath/scripts/wsl/build-firmware-wsl.sh"
Invoke-Wsl -Command "bash '$script' --repo '$repoWslPath' --zmk-root '$ZmkRoot'"
New-LocalFirmwareManifest -LatestDir $latestDir -BuildMode "local-wsl"

Write-ZmkSection "Local WSL build complete"
& "$PSScriptRoot\Get-FirmwareInventory.ps1" -FirmwareDir "firmware/latest"
exit $LASTEXITCODE
