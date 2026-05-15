[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\_ZmkPipeline.Common.ps1"
trap {
    Write-Host ""
    Write-Host "ERROR: $($_.Exception.Message)"
    exit 1
}

function Add-Check {
    param(
        [System.Collections.Generic.List[object]] $Checks,

        [Parameter(Mandatory = $true)]
        [string] $Name,

        [Parameter(Mandatory = $true)]
        [bool] $Ok,

        [Parameter(Mandatory = $true)]
        [string] $Message,

        [bool] $Required = $true
    )

    $Checks.Add([pscustomobject]@{
        Name     = $Name
        Ok       = $Ok
        Required = $Required
        Message  = $Message
    }) | Out-Null
}

$root = Assert-ZmkRepoRoot
$checks = [System.Collections.Generic.List[object]]::new()

Write-ZmkSection "ZMK repo validation"
Write-Host "Repository root: $root"

foreach ($tool in @("git", "gh", "powershell")) {
    $exists = Test-ZmkCommand -Name $tool
    Add-Check -Checks $checks -Name $tool -Ok $exists -Message $(if ($exists) { "Found $tool." } else { "Missing $tool in PATH." })
}

$pwshExists = Test-ZmkCommand -Name "pwsh"
Add-Check -Checks $checks -Name "pwsh" -Ok $pwshExists -Required:$false -Message $(if ($pwshExists) { "Found PowerShell 7 (pwsh)." } else { "PowerShell 7 (pwsh) not found; Windows PowerShell can still run these scripts." })

if (Test-ZmkCommand -Name "gh") {
    $authOutput = Invoke-ZmkNative -FilePath "gh" -Arguments @("auth", "status")
    $authOk = $authOutput.ExitCode -eq 0
    Add-Check -Checks $checks -Name "gh auth status" -Ok $authOk -Message $(if ($authOk) { "GitHub CLI is authenticated." } else { "GitHub CLI is not authenticated. Run: gh auth login" })
}
else {
    Add-Check -Checks $checks -Name "gh auth status" -Ok $false -Message "Cannot check GitHub auth because gh is missing."
}

if (Test-ZmkCommand -Name "git") {
    $plainGit = Invoke-ZmkNative -FilePath "git" -Arguments @("-C", $root, "rev-parse", "--is-inside-work-tree")
    $plainGitOk = $plainGit.ExitCode -eq 0
    $gitSafeOk = Test-ZmkGitRepository
    $message = if ($plainGitOk) {
        "Git repository detected."
    }
    elseif ($plainGit.Text -match "dubious ownership") {
        "Git reports dubious ownership. The scripts use a per-command safe.directory override, but you can fix your shell with: git config --global --add safe.directory $root"
    }
    else {
        "Not a usable git repository. $($plainGit.Text)"
    }
    Add-Check -Checks $checks -Name "git repository" -Ok $gitSafeOk -Message $message
}

$requiredFiles = @(
    "build.yaml",
    "config\west.yml",
    ".github\workflows\build.yml",
    "config\eyelash_sofle.keymap"
)

foreach ($relative in $requiredFiles) {
    $path = Join-Path -Path $root -ChildPath $relative
    $exists = Test-Path -LiteralPath $path
    Add-Check -Checks $checks -Name $relative -Ok $exists -Message $(if ($exists) { "Found $relative." } else { "Missing $relative." })
}

Write-ZmkSection "Summary"
foreach ($check in $checks) {
    $status = if ($check.Ok) { "OK" } elseif ($check.Required) { "FAIL" } else { "WARN" }
    Write-Host ("[{0}] {1} - {2}" -f $status, $check.Name, $check.Message)
}

$failed = @($checks | Where-Object { $_.Required -and -not $_.Ok })
if ($failed.Count -gt 0) {
    Write-Host ""
    Write-Host "Validation failed with $($failed.Count) required issue(s)."
    exit 1
}

Write-Host ""
Write-Host "Validation passed."
exit 0
