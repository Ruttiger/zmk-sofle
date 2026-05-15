[CmdletBinding(SupportsShouldProcess = $true, ConfirmImpact = "Medium")]
param(
    [switch] $TagKnownGood,
    [switch] $Commit,
    [switch] $Force
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\_ZmkPipeline.Common.ps1"
trap {
    Write-Host ""
    Write-Host "ERROR: $($_.Exception.Message)"
    exit 1
}

function Save-TextFile {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path,

        [string] $Text
    )

    Set-Content -LiteralPath $Path -Value $Text -Encoding UTF8
}

$root = Assert-ZmkRepoRoot
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$backupRoot = Join-ZmkPath -Root $root -Parts @("firmware", "backups")
$backupDir = Join-Path -Path $backupRoot -ChildPath $timestamp
$latestDir = Join-ZmkPath -Root $root -Parts @("firmware", "latest")

New-ZmkDirectory -Path $backupDir

Write-ZmkSection "Creating restore point"
Write-Host "Backup directory: $backupDir"

Copy-ZmkFile -Source (Join-Path $root "build.yaml") -Destination (Join-Path $backupDir "build.yaml")
Copy-ZmkDirectory -Source (Join-Path $root "config") -Destination (Join-Path $backupDir "config")
Copy-ZmkFile -Source (Join-ZmkPath -Root $root -Parts @(".github", "workflows", "build.yml")) -Destination (Join-ZmkPath -Root $backupDir -Parts @(".github", "workflows", "build.yml"))

Save-TextFile -Path (Join-Path $backupDir "git-status.txt") -Text (Get-ZmkGitText -Arguments @("status", "--short", "--branch") -AllowFailure)
Save-TextFile -Path (Join-Path $backupDir "git-head.txt") -Text (Get-ZmkGitText -Arguments @("rev-parse", "HEAD") -AllowFailure)
Save-TextFile -Path (Join-Path $backupDir "git-diff.patch") -Text (Get-ZmkGitText -Arguments @("diff") -AllowFailure)
Save-TextFile -Path (Join-Path $backupDir "git-diff-staged.patch") -Text (Get-ZmkGitText -Arguments @("diff", "--staged") -AllowFailure)

if (Test-Path -LiteralPath $latestDir) {
    $latestUf2 = @(Get-ChildItem -LiteralPath $latestDir -Filter "*.uf2" -File -ErrorAction SilentlyContinue)
    if ($latestUf2.Count -gt 0) {
        $artifactsBackup = Join-Path $backupDir "previous-latest"
        New-ZmkDirectory -Path $artifactsBackup
        foreach ($file in $latestUf2) {
            Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $artifactsBackup $file.Name) -Force
        }
    }
}

$gitUser = Get-ZmkGitUser
$manifest = [pscustomobject]@{
    createdAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    branch       = Get-ZmkGitBranch
    commit       = Get-ZmkGitCommit
    repo         = Get-ZmkGitRemoteUrl
    user         = $gitUser
    hashes       = Get-ZmkImportantFileHashes -Root $root
}

Write-ZmkJson -InputObject $manifest -Path (Join-Path $backupDir "manifest.json")

if ($TagKnownGood) {
    $tagName = "known-good/$timestamp"
    Write-Host "Creating local tag: $tagName"
    Invoke-ZmkGit -Arguments @("tag", $tagName) | Out-Null
}

if ($Commit) {
    if (Test-ZmkGitHasChanges) {
        $shouldCommit = $Force
        if (-not $Force) {
            $answer = Read-Host "Create a local backup commit with current changes? Type YES to continue"
            $shouldCommit = $answer -eq "YES"
        }

        if ($shouldCommit) {
            Invoke-ZmkGit -Arguments @("add", "-A", "--", ".", ":!firmware/backups/*") | Out-Null
            $staged = Get-ZmkGitText -Arguments @("diff", "--staged", "--name-only") -AllowFailure
            if (-not [string]::IsNullOrWhiteSpace($staged)) {
                Invoke-ZmkGit -Arguments @("commit", "-m", "backup: restore point $timestamp") | Out-Null
                Write-Host "Backup commit created."
            }
            else {
                Write-Host "No staged changes to commit."
            }
        }
        else {
            Write-Host "Commit skipped."
        }
    }
    else {
        Write-Host "No git changes to commit."
    }
}

Write-Host "Restore point created: $backupDir"
exit 0
