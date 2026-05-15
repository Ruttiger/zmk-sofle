[CmdletBinding()]
param(
    [switch] $NoBackup,
    [string] $CommitMessage,
    [switch] $Push,
    [string] $Workflow = "build.yml"
)

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\_ZmkPipeline.Common.ps1"
trap {
    Write-Host ""
    Write-Host "ERROR: $($_.Exception.Message)"
    exit 1
}

function Assert-GhReady {
    if (-not (Test-ZmkCommand -Name "gh")) {
        throw "GitHub CLI (gh) is required for this build mode. Install it, then run: gh auth login"
    }

    $auth = Invoke-ZmkNative -FilePath "gh" -Arguments @("auth", "status")
    if ($auth.ExitCode -ne 0) {
        throw "GitHub CLI is not authenticated. Run: gh auth login"
    }
}

function Invoke-Gh {
    param(
        [Parameter(Mandatory = $true)]
        [string[]] $Arguments
    )

    $result = Invoke-ZmkNative -FilePath "gh" -Arguments $Arguments
    if ($result.ExitCode -ne 0) {
        throw "gh $($Arguments -join ' ') failed. $($result.Text)"
    }
    return @($result.Output)
}

function Get-LatestWorkflowRun {
    param(
        [Parameter(Mandatory = $true)]
        [string] $WorkflowName,

        [Parameter(Mandatory = $true)]
        [string] $Branch,

        [Parameter(Mandatory = $true)]
        [string] $Commit,

        [Parameter(Mandatory = $true)]
        [datetime] $StartedAfterUtc
    )

    $json = Invoke-Gh -Arguments @("run", "list", "--workflow", $WorkflowName, "--branch", $Branch, "--json", "databaseId,headSha,status,conclusion,createdAt,event", "--limit", "20")
    $runs = @($json | Out-String | ConvertFrom-Json)
    $matches = @(
        $runs |
            Where-Object { [datetime]::Parse($_.createdAt).ToUniversalTime() -ge $StartedAfterUtc.AddSeconds(-30) } |
            Sort-Object { [datetime]::Parse($_.createdAt) } -Descending
    )

    if ($matches.Count -eq 0) {
        throw "Could not find the workflow run that was just started. Check GitHub Actions in the repository."
    }

    $commitMatches = @($matches | Where-Object { $_.headSha -eq $Commit })
    if ($commitMatches.Count -eq 0) {
        $seen = (($matches | Select-Object -First 3 | ForEach-Object { $_.headSha }) -join ", ")
        throw "Found recent workflow run(s), but none matched local commit $Commit. Seen head SHA(s): $seen. Push your branch first or rerun with -Push."
    }

    return $commitMatches[0]
}

$root = Assert-ZmkRepoRoot
Assert-GhReady

$branch = Get-ZmkGitBranch
if ([string]::IsNullOrWhiteSpace($branch) -or $branch -eq "HEAD") {
    throw "Cannot run GitHub Actions from a detached HEAD. Check out a branch first."
}

$latestDir = Join-ZmkPath -Root $root -Parts @("firmware", "latest")
$downloadsDir = Join-ZmkPath -Root $root -Parts @("firmware", "downloads")
New-ZmkDirectory -Path $latestDir
New-ZmkDirectory -Path $downloadsDir

$madeBackup = $false
if (-not $NoBackup) {
    & "$PSScriptRoot\New-RestorePoint.ps1"
    if ($LASTEXITCODE -ne 0) {
        throw "Backup failed; build aborted."
    }
    $madeBackup = $true
}

if (-not $madeBackup) {
    $existingUf2 = @(Get-ChildItem -LiteralPath $latestDir -Filter "*.uf2" -File -ErrorAction SilentlyContinue)
    if ($existingUf2.Count -gt 0) {
        Write-Host "Existing firmware/latest artifacts detected. Creating safety backup before replacing them."
        & "$PSScriptRoot\New-RestorePoint.ps1"
        if ($LASTEXITCODE -ne 0) {
            throw "Safety backup failed; build aborted."
        }
    }
}

if (-not [string]::IsNullOrWhiteSpace($CommitMessage)) {
    Invoke-ZmkGit -Arguments @("add", "-A", "--", ".", ":!firmware/backups/*", ":!firmware/latest/*", ":!firmware/downloads/*") | Out-Null
    $staged = Get-ZmkGitText -Arguments @("diff", "--staged", "--name-only") -AllowFailure
    if (-not [string]::IsNullOrWhiteSpace($staged)) {
        Invoke-ZmkGit -Arguments @("commit", "-m", $CommitMessage) | Out-Null
        Write-Host "Commit created: $CommitMessage"
    }
    else {
        Write-Host "No staged changes to commit."
    }
}

if ($Push) {
    Invoke-ZmkGit -Arguments @("push", "origin", $branch) | Out-Null
    Write-Host "Pushed branch $branch."
}

$commit = Get-ZmkGitCommit
$startedAt = (Get-Date).ToUniversalTime()

Write-ZmkSection "Starting GitHub Actions build"
Write-Host "Workflow: $Workflow"
Write-Host "Branch: $branch"
Write-Host "Commit: $commit"

Invoke-Gh -Arguments @("workflow", "run", $Workflow, "--ref", $branch) | Out-Null
Start-Sleep -Seconds 8
$run = Get-LatestWorkflowRun -WorkflowName $Workflow -Branch $branch -Commit $commit -StartedAfterUtc $startedAt
$runId = [string]$run.databaseId

if ([string]::IsNullOrWhiteSpace($runId)) {
    throw "GitHub did not return a workflow run id."
}

Write-Host "Watching workflow run: $runId"
$watch = Invoke-ZmkNative -FilePath "gh" -Arguments @("run", "watch", $runId, "--exit-status")
if ($watch.ExitCode -ne 0) {
    Write-Host $watch.Text
    throw "GitHub Actions build failed. Inspect it with: gh run view $runId --web"
}

$runDownloadDir = Join-Path -Path $downloadsDir -ChildPath $runId
if (Test-Path -LiteralPath $runDownloadDir) {
    Remove-Item -LiteralPath $runDownloadDir -Recurse -Force
}
New-ZmkDirectory -Path $runDownloadDir

Write-ZmkSection "Downloading artifacts"
Invoke-Gh -Arguments @("run", "download", $runId, "--dir", $runDownloadDir) | Out-Null

$zipFiles = @(Get-ChildItem -LiteralPath $runDownloadDir -Filter "*.zip" -File -Recurse -ErrorAction SilentlyContinue)
foreach ($zip in $zipFiles) {
    $extractDir = Join-Path -Path $zip.DirectoryName -ChildPath ([System.IO.Path]::GetFileNameWithoutExtension($zip.Name))
    New-ZmkDirectory -Path $extractDir
    Expand-Archive -LiteralPath $zip.FullName -DestinationPath $extractDir -Force
}

$uf2Files = @(Get-ChildItem -LiteralPath $runDownloadDir -Filter "*.uf2" -File -Recurse -ErrorAction SilentlyContinue)
if ($uf2Files.Count -eq 0) {
    throw "No .uf2 files were found in downloaded artifacts."
}

Get-ChildItem -LiteralPath $latestDir -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ne ".gitkeep" } |
    Remove-Item -Force

$manifestItems = @()
foreach ($uf2 in $uf2Files) {
    $destination = Join-Path -Path $latestDir -ChildPath $uf2.Name
    Copy-Item -LiteralPath $uf2.FullName -Destination $destination -Force
    $copied = Get-Item -LiteralPath $destination
    $hash = Get-FileHash -LiteralPath $copied.FullName -Algorithm SHA256
    $artifactName = Split-Path -Leaf (Split-Path -Parent $uf2.FullName)
    $manifestItems += [pscustomobject]@{
        artifactName = $artifactName
        fileName     = $copied.Name
        path         = Resolve-ZmkRelativePath -Root $root -Path $copied.FullName
        sizeBytes    = $copied.Length
        sha256       = $hash.Hash.ToLowerInvariant()
    }
}

$manifest = [pscustomobject]@{
    createdAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    workflow     = $Workflow
    runId        = $runId
    branch       = $branch
    commit       = Get-ZmkGitCommit
    files        = $manifestItems
}

Write-ZmkJson -InputObject $manifest -Path (Join-Path $latestDir "manifest.json")

Write-ZmkSection "Build complete"
$manifestItems | Format-Table artifactName, fileName, sizeBytes, sha256 -AutoSize
exit 0
