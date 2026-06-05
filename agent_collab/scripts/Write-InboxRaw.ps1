#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Write raw worker output to inbox/<runtime>/raw/ ONLY (watchers / fallback). Not for normalized protocol.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$Runtime,

    [Parameter(Mandatory=$true)]
    [string]$RequestId,

    [Parameter(Mandatory=$true)]
    [string]$ContentPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path $ContentPath)) {
    Write-Error "Content file not found: $ContentPath"
    exit 2
}

$destDir = "agent_collab/inbox/$Runtime/raw"
New-Item -ItemType Directory -Path $destDir -Force | Out-Null

$dest = Join-Path $destDir "$RequestId.json"
Copy-Item $ContentPath $dest -Force
Write-Output "RAW_WRITTEN: $dest"
exit 0