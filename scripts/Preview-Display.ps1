#Requires -Version 5.1
<#
.SYNOPSIS
    Builds and launches the ZMK display preview (SDL window, nice!view size).

.DESCRIPTION
    Compiles the eyelash_sofle_display_preview shield for native_sim/native/64
    inside WSL, then runs the resulting binary so the ZMK status screen renders
    in a desktop window (160x68 px, matching the physical nice!view).

    Requires WSLg (Windows 11) or an X11 server (Windows 10: VcXsrv / X410).

.PARAMETER NoRebuild
    Skip compilation and launch the existing binary from a previous build.

.PARAMETER Distro
    WSL distribution to use. Defaults to the first available distribution.

.PARAMETER ZmkRoot
    ZMK source root inside WSL. Defaults to ~/zmk.

.EXAMPLE
    # Full rebuild + launch:
    .\scripts\Preview-Display.ps1

    # Launch without rebuilding:
    .\scripts\Preview-Display.ps1 -NoRebuild
#>
[CmdletBinding()]
param(
    [switch] $NoRebuild,
    [string] $Distro,
    [string] $ZmkRoot = "~/zmk"
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
        throw "WSL is installed but no Linux distribution is visible. Run VS Code from your normal user session."
    }

    return @($list.Output | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
}

function Invoke-Wsl {
    # Buffered: captures stdout+stderr, prints them, then checks exit code.
    # Use for build steps. Do NOT use for interactive processes (SDL window).
    param([Parameter(Mandatory)][string] $Command)

    $arguments = @()
    if (-not [string]::IsNullOrWhiteSpace($Distro)) {
        $arguments += @("-d", $Distro)
    }
    $arguments += @("bash", "-lc", $Command)

    $result = Invoke-ZmkNative -FilePath "wsl.exe" -Arguments $arguments
    foreach ($line in $result.Output) { Write-Host $line }
    if ($result.ExitCode -ne 0) {
        throw "WSL command failed with exit code $($result.ExitCode)."
    }
    return $result
}

function Invoke-WslInteractive {
    # Inherits stdio — required for the SDL window and live build output.
    param([Parameter(Mandatory)][string] $Command)

    $wslArgs = @()
    if (-not [string]::IsNullOrWhiteSpace($Distro)) {
        $wslArgs += @("-d", $Distro)
    }
    $wslArgs += @("bash", "-lc", $Command)

    & wsl.exe @wslArgs
    if ($LASTEXITCODE -ne 0) {
        throw "WSL interactive command failed with exit code $LASTEXITCODE."
    }
}

function Invoke-WslLaunch {
    # Like Invoke-WslInteractive but routes output through PowerShell's pipeline
    # so that the WSL process has a pipe fd (not a Windows Console TTY) on stdout
    # and stderr.  This prevents a WSL/SDL/XPutImage crash that occurs when the
    # binary is attached to a Windows Console TTY.
    param([Parameter(Mandatory)][string] $Command)

    $wslArgs = @()
    if (-not [string]::IsNullOrWhiteSpace($Distro)) {
        $wslArgs += @("-d", $Distro)
    }
    $wslArgs += @("bash", "-lc", $Command)

    # Pipe through Out-Host so wsl.exe's stdout is a pipe, not a console TTY.
    wsl.exe @wslArgs | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "WSL launch command failed with exit code $LASTEXITCODE."
    }
}

function ConvertTo-WslPath {
    param([Parameter(Mandatory)][string] $WindowsPath)
    $escaped = $WindowsPath.Replace("'", "'\''")
    return (Invoke-Wsl -Command "wslpath -a '$escaped'").Text.Trim()
}

# ── Main ──────────────────────────────────────────────────────────────────────

$root    = Assert-ZmkRepoRoot
$distros = Assert-WslAvailable

Write-ZmkSection "ZMK display preview"
Write-Host "Repository root : $root"
Write-Host "WSL distros     : $($distros -join ', ')"
if (-not [string]::IsNullOrWhiteSpace($Distro)) {
    Write-Host "Selected distro : $Distro"
}
Write-Host "ZMK root (WSL)  : $ZmkRoot"
Write-Host "Shield          : eyelash_sofle_display_preview"
Write-Host "Board           : native_posix_64  (SDL 160x68)"
Write-Host ""
Write-Host "Requires WSLg (Win 11) or an X11 server (Win 10)."
Write-Host ""

$repoWslPath = ConvertTo-WslPath -WindowsPath $root

$rebuildFlag = if ($NoRebuild) { "--no-rebuild" } else { "" }
$script = "$repoWslPath/scripts/wsl/preview-display-wsl.sh"

# Phase 1: build inside WSL (inherited stdio for live output; avoids ReadToEnd deadlock)
Invoke-WslInteractive -Command "bash '$script' --repo '$repoWslPath' --zmk-root '$ZmkRoot' $rebuildFlag --build-only"

# Phase 2: launch the SDL binary.  Invoke-WslInteractive (inherited stdio)
# keeps the window alive interactively; the bash script internally pipes the
# binary's stdout through a process substitution so Zephyr native_posix does
# not see a TTY and avoids the WSL/SDL/X11 crash.
Write-Host ""
Write-Host "Launching display preview (160x68 SDL window). Close the window or Ctrl+C to exit."
Write-Host ""
Invoke-WslInteractive -Command "bash '$script' --repo '$repoWslPath' --zmk-root '$ZmkRoot' --launch-only"

exit 0
