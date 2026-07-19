[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Development', 'Release')]
    [string]$Configuration = 'Development',
    [ValidateSet('x64')]
    [string]$Platform = 'x64',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$solution = Join-Path $repoRoot 'CG4.sln'

$prerequisiteJson = & (Join-Path $PSScriptRoot 'check_prerequisites.ps1') -Configuration $Configuration -Json
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$prerequisites = $prerequisiteJson | ConvertFrom-Json

$msbuildPath = $prerequisites.msbuildPath
if (-not $msbuildPath -or -not (Test-Path -LiteralPath $msbuildPath)) {
    Write-Error 'MSBuild could not be resolved.'
    exit 2
}

$target = if ($Clean) { 'Rebuild' } else { 'Build' }
$arguments = @(
    $solution,
    "/t:$target",
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    '/m',
    '/nr:false',
    '/nologo',
    '/verbosity:minimal'
)

Write-Output ("Building CG4: configuration={0} platform={1} target={2}" -f $Configuration, $Platform, $target)
& $msbuildPath @arguments
exit $LASTEXITCODE
