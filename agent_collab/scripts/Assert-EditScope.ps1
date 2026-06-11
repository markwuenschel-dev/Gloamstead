#!/usr/bin/env pwsh
<#
.SYNOPSIS
    UE5-aware scope guard for agent_collab.

    Loads agent_collab/context/scope_roots.json and enforces:
    - Coder: only coder_edit_roots (text) + (when permitted) coder_generated_output_roots for automation outputs
    - Documentor: only documentor_edit_roots
    - Orchestrator: orchestrator_edit_roots
    - Always block: vendor_content_roots, readonly_roots, forbidden_roots, path traversal

    Exits:
      0 = allowed
      2 = blocked (policy violation)
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$TargetFile,

    [Parameter(Mandatory=$true)]
    [string[]]$AllowedRoots,

    [string[]]$ForbiddenRoots = @(),

    [string]$AgentType = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-RepoToplevelFromPath {
    param([string]$Path)
    $dir = Split-Path -Parent (Resolve-Path $Path).Path
    if (-not $dir) { $dir = $Path }
    try {
        $toplevel = & git -C $dir rev-parse --show-toplevel 2>$null
        if ($LASTEXITCODE -eq 0 -and $toplevel) { return $toplevel.Trim() }
    } catch {}
    $current = $dir
    while ($current -and (Test-Path $current)) {
        if (Test-Path (Join-Path $current ".git")) { return $current }
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

    if ($relative -like "../*" -or $relative -eq "..") {
        Write-Error "BLOCKED: Path escapes repo: $relative"
        exit 2
    }

    # Load full scope roots for UE5-aware policy
    $scopeFile = Join-Path $toplevel "agent_collab/context/scope_roots.json"
    if (Test-Path $scopeFile) {
        $scope = Get-Content -Raw $scopeFile | ConvertFrom-Json

        # Always block forbidden + vendor + readonly
        $allForbidden = @($scope.forbidden_roots) + @($scope.vendor_content_roots) + @($scope.readonly_roots) + @($ForbiddenRoots)
        foreach ($f in $allForbidden) {
            if (-not $f) { continue }
            $fNorm = $f -replace '\\','/'
            if ($relative -eq $fNorm -or $relative.StartsWith($fNorm + "/")) {
                Write-Error "BLOCKED: $relative is under forbidden/vendor/readonly root '$f' (UE5 content policy)"
                exit 2
            }
        }

        # Role-specific enforcement
        $effectiveAllowed = $AllowedRoots
        if ($AgentType -like "*coder*") {
            $effectiveAllowed = $scope.coder_edit_roots
            # Note: generated output roots are allowed only when the handoff explicitly assigned them and the tool is automation (not direct Edit/Write of binary).
            # The guard here is conservative: direct file edits must be in coder_edit_roots. Generated binaries are produced by automation, not direct Edit/Write.
        } elseif ($AgentType -like "*documentor*") {
            $effectiveAllowed = $scope.documentor_edit_roots
        } elseif ($AgentType -like "*orchestrator*") {
            $effectiveAllowed = $scope.orchestrator_edit_roots
        }
    } else {
        $effectiveAllowed = $AllowedRoots
    }

    # Check allowed
    $allowed = $false
    foreach ($a in $effectiveAllowed) {
        if (-not $a) { continue }
        $aNorm = $a -replace '\\','/'
        if ($relative -eq $aNorm -or $relative.StartsWith($aNorm + "/")) {
            $allowed = $true
            break
        }
    }

    if (-not $allowed) {
        Write-Error "BLOCKED: $relative not under any allowed root for role. Allowed: $($effectiveAllowed -join ', ')"
        exit 2
    }

    Write-Output "ALLOWED: $relative"
    exit 0

} catch {
    Write-Error "BLOCKED: Scope check failed for ${TargetFile}: $_"
    exit 2
}
