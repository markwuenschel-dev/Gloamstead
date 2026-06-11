#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Runtime-neutral invocation entry point.

    Validates the request against registry, invokes the runtime via its adapter,
    captures raw output, validates structured result, and returns validated result.

    Raw output is always saved to outbox/runtime_raw/<runtime>/<invocation_id>/
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$Runtime,

    [Parameter(Mandatory=$true)]
    [string]$Role,

    [Parameter(Mandatory=$true)]
    [string]$InvocationId,

    [Parameter(Mandatory=$true)]
    [string]$TaskId,

    [Parameter(Mandatory=$true)]
    [string]$HandoffPath,

    [Parameter(Mandatory=$true)]
    [string]$WorktreePath,

    [string[]]$ContextPaths = @(),

    [Parameter(Mandatory=$true)]
    [string]$OutputSchemaPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (git rev-parse --show-toplevel).Trim()
$runtimesFile = Join-Path $repoRoot "agent_collab/registry/runtimes.json"
$adapterDir = Join-Path $repoRoot "agent_collab/adapters/$Runtime"

# Basic validation
$runtimes = Get-Content -Raw $runtimesFile | ConvertFrom-Json
if (-not $runtimes.runtimes.PSObject.Properties[$Runtime] -or -not $runtimes.runtimes.$Runtime.enabled) {
    Write-Error "BLOCKED: Runtime $Runtime not enabled."
    exit 2
}
$rt = $runtimes.runtimes.$Runtime
if ($Role -notin $rt.supported_roles) {
    Write-Error "BLOCKED: Runtime $Runtime does not support role $Role."
    exit 2
}

if (-not (Test-Path $adapterDir)) {
    Write-Error "BLOCKED: No adapter directory for runtime $Runtime at $adapterDir"
    exit 2
}

# Ensure raw output dir
$rawDir = Join-Path $repoRoot "agent_collab/outbox/runtime_raw/$Runtime/$InvocationId"
New-Item -ItemType Directory -Path $rawDir -Force | Out-Null
$rawOutputFile = Join-Path $rawDir "raw_output.txt"

# Placeholder invocation - in real use the adapter/launcher or native projection handles the actual call.
# For scaffold, we simulate a successful structured result for testing purposes.
# Real adapters would call the runtime (claude, grok, etc.) here.

$simulatedResult = @{
    invocation_id = $InvocationId
    runtime = $Runtime
    role = $Role
    task_id = $TaskId
    summary = "Simulated successful result for scaffold test (replace with real runtime invocation in production adapters)."
    changed_text_files = @()
    generated_binary_files = @()
    commands_run = @()
    verification_results = @{}
    acceptance_criteria_status = @{}
    notes = "This is a placeholder. Production implementation must invoke the actual runtime (via .claude projection, grok skills, CLI, etc.) and capture real output."
}

# Write raw (simulated)
$simulatedResult | ConvertTo-Json -Depth 10 | Set-Content $rawOutputFile -Encoding UTF8

# Validate against schema (basic)
try {
    $schema = Get-Content -Raw $OutputSchemaPath | ConvertFrom-Json
    # In production: full JSON Schema validation using Validate-JsonSchema.ps1 or equivalent
    Write-Output "STRUCTURED_RESULT_VALIDATED (placeholder)"
} catch {
    Write-Error "Validation failed for simulated result: $_"
    exit 2
}

# Return the structured result on stdout (Orchestrator captures it)
$simulatedResult | ConvertTo-Json -Depth 10
exit 0
