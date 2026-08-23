[CmdletBinding(DefaultParameterSetName = 'Verify')]
param(
    [Parameter(Mandatory, ParameterSetName = 'Sync')][string] $Package,
    [Parameter(Mandatory, ParameterSetName = 'Sync')][string] $Manifest,
    [Parameter(ParameterSetName = 'Verify')][switch] $Verify
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$helper = Join-Path $PSScriptRoot 'worldforge_asset_forge/forge_contract.py'
$arguments = @($helper, '--root', $root)
if ($PSCmdlet.ParameterSetName -eq 'Sync') {
    $arguments += @('sync', '--package', (Resolve-Path -LiteralPath $Package).Path,
        '--manifest', (Resolve-Path -LiteralPath $Manifest).Path)
} else {
    $arguments += 'verify-installed'
}
& python @arguments
exit $LASTEXITCODE
