$ErrorActionPreference = "Stop"

function Get-ZmkRepoRoot {
    $root = Split-Path -Parent $PSScriptRoot
    return (Resolve-Path -LiteralPath $root).Path
}

function Join-ZmkPath {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Root,

        [Parameter(Mandatory = $true)]
        [string[]] $Parts
    )

    $path = $Root
    foreach ($part in $Parts) {
        $path = Join-Path -Path $path -ChildPath $part
    }
    return $path
}

function Test-ZmkCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Name
    )

    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Invoke-ZmkNative {
    param(
        [Parameter(Mandatory = $true)]
        [string] $FilePath,

        [string[]] $Arguments = @()
    )

    function ConvertTo-NativeArgument {
        param([string] $Value)

        if ($null -eq $Value) {
            return '""'
        }

        if ($Value -notmatch '[\s"]') {
            return $Value
        }

        return '"' + $Value.Replace('"', '\"') + '"'
    }

    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $FilePath
    $startInfo.Arguments = (($Arguments | ForEach-Object { ConvertTo-NativeArgument -Value $_ }) -join " ")
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.CreateNoWindow = $true

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo

    try {
        $process.Start() | Out-Null
        # Read both streams concurrently to avoid deadlock when pipe buffers fill up
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()
        $stdout = $stdoutTask.GetAwaiter().GetResult()
        $stderr = $stderrTask.GetAwaiter().GetResult()
        $exitCode = $process.ExitCode
    }
    finally {
        $process.Dispose()
    }

    $lines = @()
    if (-not [string]::IsNullOrEmpty($stdout)) {
        $lines += $stdout.TrimEnd() -split "`r?`n"
    }
    if (-not [string]::IsNullOrEmpty($stderr)) {
        $lines += $stderr.TrimEnd() -split "`r?`n"
    }

    return [pscustomobject]@{
        ExitCode = $exitCode
        Output   = @($lines)
        Text     = ($lines | Out-String).Trim()
    }
}

function Invoke-ZmkGit {
    param(
        [Parameter(Mandatory = $true)]
        [string[]] $Arguments,

        [switch] $AllowFailure
    )

    $root = (Get-ZmkRepoRoot) -replace "\\", "/"
    $result = Invoke-ZmkNative -FilePath "git" -Arguments (@("-c", "safe.directory=$root") + $Arguments)
    $output = $result.Output
    $exitCode = $result.ExitCode

    if ($exitCode -ne 0 -and -not $AllowFailure) {
        $text = ($output | Out-String).Trim()
        throw "git $($Arguments -join ' ') failed with exit code $exitCode. $text"
    }

    return [pscustomobject]@{
        ExitCode = $exitCode
        Output   = @($output)
        Text     = ($output | Out-String).Trim()
    }
}

function Get-ZmkGitText {
    param(
        [Parameter(Mandatory = $true)]
        [string[]] $Arguments,

        [switch] $AllowFailure
    )

    $result = Invoke-ZmkGit -Arguments $Arguments -AllowFailure:$AllowFailure
    return $result.Text
}

function Test-ZmkGitRepository {
    $result = Invoke-ZmkGit -Arguments @("rev-parse", "--is-inside-work-tree") -AllowFailure
    return $result.ExitCode -eq 0 -and $result.Text -eq "true"
}

function Get-ZmkGitBranch {
    $branch = Get-ZmkGitText -Arguments @("rev-parse", "--abbrev-ref", "HEAD") -AllowFailure
    if ([string]::IsNullOrWhiteSpace($branch)) {
        return $null
    }
    return $branch.Trim()
}

function Get-ZmkGitCommit {
    $commit = Get-ZmkGitText -Arguments @("rev-parse", "HEAD") -AllowFailure
    if ([string]::IsNullOrWhiteSpace($commit)) {
        return $null
    }
    return $commit.Trim()
}

function Get-ZmkGitRemoteUrl {
    $remote = Get-ZmkGitText -Arguments @("config", "--get", "remote.origin.url") -AllowFailure
    if ([string]::IsNullOrWhiteSpace($remote)) {
        return $null
    }
    return $remote.Trim()
}

function Get-ZmkGitUser {
    $userName = Get-ZmkGitText -Arguments @("config", "--get", "user.name") -AllowFailure
    $userEmail = Get-ZmkGitText -Arguments @("config", "--get", "user.email") -AllowFailure

    return [pscustomobject]@{
        Name  = $userName
        Email = $userEmail
    }
}

function Test-ZmkGitHasChanges {
    $status = Get-ZmkGitText -Arguments @("status", "--porcelain") -AllowFailure
    return -not [string]::IsNullOrWhiteSpace($status)
}

function Write-ZmkSection {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Title
    )

    Write-Host ""
    Write-Host "== $Title =="
}

function New-ZmkDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
}

function Copy-ZmkDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Source,

        [Parameter(Mandatory = $true)]
        [string] $Destination
    )

    if (Test-Path -LiteralPath $Source) {
        New-ZmkDirectory -Path (Split-Path -Parent $Destination)
        Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
    }
}

function Copy-ZmkFile {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Source,

        [Parameter(Mandatory = $true)]
        [string] $Destination
    )

    if (Test-Path -LiteralPath $Source) {
        New-ZmkDirectory -Path (Split-Path -Parent $Destination)
        Copy-Item -LiteralPath $Source -Destination $Destination -Force
    }
}

function Get-ZmkImportantFileHashes {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Root
    )

    $paths = @()
    foreach ($relative in @("build.yaml", "config", ".github\workflows\build.yml")) {
        $full = Join-Path -Path $Root -ChildPath $relative
        if (Test-Path -LiteralPath $full -PathType Leaf) {
            $paths += Get-Item -LiteralPath $full
        }
        elseif (Test-Path -LiteralPath $full -PathType Container) {
            $paths += Get-ChildItem -LiteralPath $full -File -Recurse
        }
    }

    return @(
        foreach ($item in $paths | Sort-Object FullName) {
            $hash = Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256
            [pscustomobject]@{
                path   = Resolve-ZmkRelativePath -Root $Root -Path $item.FullName
                sha256 = $hash.Hash.ToLowerInvariant()
            }
        }
    )
}

function Resolve-ZmkRelativePath {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Root,

        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    $rootPath = (Resolve-Path -LiteralPath $Root).Path.TrimEnd("\", "/")
    $fullPath = (Resolve-Path -LiteralPath $Path).Path
    if ($fullPath.StartsWith($rootPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $fullPath.Substring($rootPath.Length).TrimStart("\", "/") -replace "\\", "/"
    }
    return $fullPath
}

function Write-ZmkJson {
    param(
        [Parameter(Mandatory = $true)]
        [object] $InputObject,

        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    $json = $InputObject | ConvertTo-Json -Depth 8
    Set-Content -LiteralPath $Path -Value $json -Encoding UTF8
}

function Assert-ZmkRepoRoot {
    $root = Get-ZmkRepoRoot
    if (-not (Test-Path -LiteralPath (Join-Path $root "build.yaml"))) {
        throw "Run this script from the repository scripts folder. Expected build.yaml at $root."
    }
    return $root
}
