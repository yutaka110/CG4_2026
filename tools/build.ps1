[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Development', 'Release', 'Shipping')]
    [string]$Configuration = 'Development',
    [ValidateSet('x64')]
    [string]$Platform = 'x64',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$solution = Join-Path $repoRoot 'CG4.sln'
$generatedRoot = Join-Path (Split-Path -Parent $repoRoot) 'generated'
$outputDirectory = Join-Path $generatedRoot "outputs\$Configuration"
$moduleLibraryDirectory = Join-Path $generatedRoot "lib\$Configuration"

$prerequisiteJson = & (Join-Path $PSScriptRoot 'check_prerequisites.ps1') -Configuration $Configuration -Json
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$prerequisites = $prerequisiteJson | ConvertFrom-Json

$msbuildPath = $prerequisites.msbuildPath
if (-not $msbuildPath -or -not (Test-Path -LiteralPath $msbuildPath)) {
    Write-Error 'MSBuild could not be resolved.'
    exit 2
}

$target = if ($Clean) { 'Rebuild' } else { 'Build' }
if ($Clean) {
    $generatedFullPath = [IO.Path]::GetFullPath($generatedRoot).TrimEnd('\') + '\'
    foreach ($cleanTarget in @($outputDirectory, $moduleLibraryDirectory)) {
        $cleanFullPath = [IO.Path]::GetFullPath($cleanTarget)
        if (-not $cleanFullPath.StartsWith($generatedFullPath, [StringComparison]::OrdinalIgnoreCase)) {
            Write-Error "Clean target escaped generated root: $cleanFullPath"
            exit 2
        }
        if (Test-Path -LiteralPath $cleanFullPath) {
            Remove-Item -LiteralPath $cleanFullPath -Recurse -Force
        }
    }
}
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
$buildExitCode = $LASTEXITCODE
if ($buildExitCode -ne 0) { exit $buildExitCode }

$engineModuleRequirements = @('GE3.EngineCore', 'GE3.EngineRenderer', 'GE3.EngineRuntime') | ForEach-Object {
    [pscustomobject]@{
        Name = "$_ static library"
        Path = Join-Path (Split-Path -Parent $repoRoot) "generated\lib\$Configuration\$_.lib"
        ExpectedHash = $null
        Source = $null
    }
}
$lock = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'Build\GE3.Dependencies.lock.json') | ConvertFrom-Json
$requiredFiles = if ($Configuration -eq 'Shipping') {
    @(
        [pscustomobject]@{ Name = 'Shipping executable'; Path = (Join-Path $outputDirectory 'GE3Shipping.exe'); ExpectedHash = $null; Source = $null },
        [pscustomobject]@{ Name = 'Shipping target manifest'; Path = (Join-Path $outputDirectory 'shipping_target.json'); ExpectedHash = $null; Source = (Join-Path $repoRoot 'Build\GE3.ShippingTarget.json') },
        [pscustomobject]@{ Name = 'Shipping notices'; Path = (Join-Path $outputDirectory 'THIRD_PARTY_NOTICES.md'); ExpectedHash = $null; Source = (Join-Path $repoRoot 'THIRD_PARTY_NOTICES_SHIPPING.md') }
    ) + $engineModuleRequirements
} else {
    @(
    [pscustomobject]@{ Name = 'Editor executable'; Path = (Join-Path $outputDirectory 'GE3.exe'); ExpectedHash = $null; Source = $null },
    [pscustomobject]@{ Name = 'DXC'; Path = (Join-Path $outputDirectory 'dxcompiler.dll'); ExpectedHash = $lock.toolchain.dxcompiler.sha256; Source = $null },
    [pscustomobject]@{ Name = 'DXIL'; Path = (Join-Path $outputDirectory 'dxil.dll'); ExpectedHash = $lock.toolchain.dxil.sha256; Source = $null },
    [pscustomobject]@{ Name = 'Third-party notices'; Path = (Join-Path $outputDirectory 'THIRD_PARTY_NOTICES.md'); ExpectedHash = $null; Source = (Join-Path $repoRoot 'THIRD_PARTY_NOTICES.md') },
    [pscustomobject]@{ Name = 'Assimp license'; Path = (Join-Path $outputDirectory 'Licenses\Assimp_LICENSE.txt'); ExpectedHash = $null; Source = (Join-Path $repoRoot 'externals\assimp\LICENSE.txt') },
    [pscustomobject]@{ Name = 'DirectXTex license'; Path = (Join-Path $outputDirectory 'Licenses\DirectXTex_LICENSE.txt'); ExpectedHash = $null; Source = (Join-Path $repoRoot 'externals\DirectXTex\LICENSE.txt') },
    [pscustomobject]@{ Name = 'Dear ImGui license'; Path = (Join-Path $outputDirectory 'Licenses\DearImGui_LICENSE.txt'); ExpectedHash = $null; Source = (Join-Path $repoRoot 'externals\imgui\LICENSE.txt') },
    [pscustomobject]@{ Name = 'Windows SDK license'; Path = (Join-Path $outputDirectory 'Licenses\WindowsSDK_LICENSE.rtf'); ExpectedHash = $lock.toolchain.sdkLicenseSha256; Source = $null },
    [pscustomobject]@{ Name = 'Windows SDK notices'; Path = (Join-Path $outputDirectory 'Licenses\WindowsSDK_THIRD_PARTY_NOTICES.rtf'); ExpectedHash = $lock.toolchain.sdkThirdPartyNoticesSha256; Source = $null }
    ) + $engineModuleRequirements
}

$deploymentFailures = [System.Collections.Generic.List[string]]::new()
foreach ($requiredFile in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $requiredFile.Path -PathType Leaf)) {
        $deploymentFailures.Add("$($requiredFile.Name) is missing: $($requiredFile.Path)")
        continue
    }
    $item = Get-Item -LiteralPath $requiredFile.Path
    if ($item.Length -eq 0) {
        $deploymentFailures.Add("$($requiredFile.Name) is empty: $($requiredFile.Path)")
        continue
    }
    $expectedHash = $requiredFile.ExpectedHash
    if ($requiredFile.Source) {
        $expectedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $requiredFile.Source).Hash
    }
    if ($expectedHash) {
        $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $requiredFile.Path).Hash
        if ($actualHash -ne $expectedHash) {
            $deploymentFailures.Add("$($requiredFile.Name) hash mismatch: expected $expectedHash, actual $actualHash")
        }
    }
}
if ($Configuration -eq 'Shipping') {
    foreach ($forbiddenName in @('Resources', 'Licenses', 'dxcompiler.dll', 'dxil.dll', 'GE3.exe', 'GE3.EngineCore.lib', 'GE3.EngineRenderer.lib', 'GE3.EngineRuntime.lib')) {
        $forbiddenPath = Join-Path $outputDirectory $forbiddenName
        if (Test-Path -LiteralPath $forbiddenPath) {
            $deploymentFailures.Add("Editor/source deployment leaked into Shipping: $forbiddenPath")
        }
    }
} elseif (-not (Test-Path -LiteralPath (Join-Path $outputDirectory 'Resources') -PathType Container)) {
    $deploymentFailures.Add("Resources directory is missing: $outputDirectory\Resources")
}
if ($deploymentFailures.Count -ne 0) {
    $deploymentFailures | ForEach-Object { [Console]::Error.WriteLine("[DEPLOYMENT FAILED] $_") }
    exit 5
}

if ($Configuration -eq 'Shipping') {
    Write-Output ("Verified isolated Shipping runtime and absence of Editor/source deployment in {0}" -f $outputDirectory)
} else {
    Write-Output ("Verified deployed Editor runtime, resources, hashes, and license notices in {0}" -f $outputDirectory)
}
exit 0
