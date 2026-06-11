#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Wraps PowerShell 7 Test-Json for the agent_collab protocol.
    Exits 0 on valid, non-zero on invalid (prints errors).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$Path,

    [Parameter(Mandatory=$true)]
    [string]$SchemaFile
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path $Path)) {
    Write-Error "Input file not found: $Path"
    exit 2
}
if (-not (Test-Path $SchemaFile)) {
    Write-Error "Schema file not found: $SchemaFile"
    exit 2
}

try {
    $isValid = Test-Json -Path $Path -SchemaFile $SchemaFile -ErrorAction Stop

    if ($isValid) {
        Write-Output "VALID: $Path against $SchemaFile"
        exit 0
    } else {
        Write-Error "INVALID: $Path failed schema $SchemaFile"
        exit 1
    }
} catch {
    Write-Error "Validation error for ${Path}: $_"
    if ($_.Exception.Message) {
        Write-Error $_.Exception.Message
    }
    exit 1
}
