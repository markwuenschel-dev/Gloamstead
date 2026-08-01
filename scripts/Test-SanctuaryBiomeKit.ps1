[CmdletBinding()]
param([switch] $ProbeWorkstation)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$helper = Join-Path $PSScriptRoot 'worldforge_asset_forge/forge_contract.py'
$versionRoot = Join-Path $root 'specs/worldforge_asset_forge/sanctuary-biome-kit-1.0.0'
$schemaRoot = Join-Path $root 'specs/worldforge_asset_forge/schemas'
$schemaPairs = @(
    @('art-intent.json', 'art-intent.schema.json'),
    @('acceptance-profile.json', 'acceptance-profile.schema.json'),
    @('inventory.json', 'inventory.schema.json')
)
foreach ($pair in $schemaPairs) {
    $valid = Get-Content -Raw -LiteralPath (Join-Path $versionRoot $pair[0]) |
        Test-Json -SchemaFile (Join-Path $schemaRoot $pair[1])
    if (-not $valid) { throw "Schema validation failed for $($pair[0])" }
}
$lockValid = Get-Content -Raw -LiteralPath (Join-Path $root 'specs/worldforge_asset_forge/worldforge-plugin.lock.json') |
    Test-Json -SchemaFile (Join-Path $schemaRoot 'worldforge-plugin-lock.schema.json')
if (-not $lockValid) { throw 'Schema validation failed for worldforge-plugin.lock.json' }
& python $helper --root $root test-contract
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& python -m unittest discover -s (Join-Path $PSScriptRoot 'worldforge_asset_forge/tests') -v
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($ProbeWorkstation) {
    & python $helper --root $root probe
    exit $LASTEXITCODE
}
exit 0
