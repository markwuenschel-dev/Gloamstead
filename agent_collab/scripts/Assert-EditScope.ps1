#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Scope guard for agent_collab. Resolves repo toplevel from the TARGET FILE's directory
    (so it works correctly inside git worktrees where the script itself may not be rebased).

    Exits:
      0 = allowed
      2 = blocked (never exit 1, per spec)
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$TargetFile,

    [Parameter(Mandatory=$true)]
    [string[]]$AllowedRoots,

    [string[]]$ForbiddenRoots = @(".git", ".env", ".venv", "node_modules", "dist", "build", ".claude/worktrees", "Binaries", "DerivedDataCache", "Intermediate", "Saved")
)

Set-StrictMode -Version Latest

function Get-RepoToplevelFromPath {
    param([string]$Path)
    $dir = Split-Path -Parent (Resolve-Path $Path).Path
    if (-not $dir) { $dir = $Path }

    # Walk up until we find a .git dir or use git rev-parse
    try {
        $toplevel = & git -C $dir rev-parse --show-toplevel 2>$null
        if ($LASTEXITCODE -eq 0 -and $toplevel) {
            return $toplevel.Trim()
        }
    } catch {}

    # Fallback: walk filesystem for .git (rare)
    $current = $dir
    while ($current -and (Test-Path $current)) {
        if (Test-Path (Join-Path $current ".git")) {
            return $current
        }
        $parent = Split-Path -Parent $current
        if ($parent -eq $current) { break }
        $current = $parent
    }
    throw "Could not determine git toplevel from $Path"
}

try {
    $resolved = (Resolve-Path $TargetFile -ErrorAction Stop).Path
    $toplevel = Get-RepoToplevelFromPath -Path $resolved

    $relative = $resolved.Substring($toplevel.Length).TrimStart([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $relative = $relative -replace '\\', '/'

    # Reject path traversal
    if ($relative -like "../*" -or $relative -like "../*" -or $relative -eq "..") {
        Write-Error "BLOCKED: Path escapes repo: $relative"
        exit 2
    }

    # Check forbidden first (highest priority)
    foreach ($f in $ForbiddenRoots) {
        $fNorm = $f -replace '\\','/'
        if ($relative -eq $fNorm -or $relative.StartsWith($fNorm + "/")) {
            Write-Error "BLOCKED: $relative is under forbidden root '$f'"
            exit 2
        }
    }

    # Check allowed
    $allowed = $false
    foreach ($a in $AllowedRoots) {
        $aNorm = $a -replace '\\','/'
        if ($relative -eq $aNorm -or $relative.StartsWith($aNorm + "/")) {
            $allowed = $true
            break
        }
    }

    if (-not $allowed) {
        Write-Error "BLOCKED: $relative not under any allowed root. Allowed: $($AllowedRoots -join ', ')"
        exit 2
    }

    Write-Output "ALLOWED: $relative"
    exit 0

} catch {
    Write-Error "BLOCKED: Scope check failed for ${TargetFile}: $_"
    exit 2
}
