[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Development')]
    [string]$Configuration = 'Development',
    [switch]$SkipBuild
)

& (Join-Path $PSScriptRoot 'run_editor_validation.ps1') -Configuration $Configuration -CommercialOnly -SkipBuild:$SkipBuild
exit $LASTEXITCODE
