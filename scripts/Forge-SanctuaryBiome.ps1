[CmdletBinding()]
param(
    [string] $WorldForgeCheckout = $env:GLOAMSTEAD_WORLDFORGE_CHECKOUT,
    [string] $WorldForgePython = $env:GLOAMSTEAD_WORLDFORGE_PYTHON,
    [string] $QualifiedToolchainPins = $env:GLOAMSTEAD_WORLDFORGE_TOOLCHAIN_PINS,
    [string] $GenerationRef = 'refs/heads/codex/gloamstead-production-asset-forge',
    [string] $Deadline = ([DateTimeOffset]::UtcNow.AddHours(12).ToString('yyyy-MM-ddTHH:mm:ssZ'))
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$helper = Join-Path $PSScriptRoot 'worldforge_asset_forge/forge_contract.py'
if ([string]::IsNullOrWhiteSpace($WorldForgeCheckout) -or
    [string]::IsNullOrWhiteSpace($WorldForgePython) -or
    [string]::IsNullOrWhiteSpace($QualifiedToolchainPins)) {
    [Console]::Out.WriteLine('{"status":"Rejected","failure":{"code":"FAIL-UNVERIFIED-RUNTIME","stage":"preflight","retry":"after_change","message":"Configure GLOAMSTEAD_WORLDFORGE_CHECKOUT, GLOAMSTEAD_WORLDFORGE_PYTHON, and GLOAMSTEAD_WORLDFORGE_TOOLCHAIN_PINS or pass the corresponding explicit parameters."}}')
    exit 2
}
& python $helper --root $root run `
    --pins (Resolve-Path -LiteralPath $QualifiedToolchainPins).Path `
    --deadline $Deadline `
    --generation-ref $GenerationRef `
    --worldforge-checkout (Resolve-Path -LiteralPath $WorldForgeCheckout).Path `
    --worldforge-python (Resolve-Path -LiteralPath $WorldForgePython).Path
exit $LASTEXITCODE
