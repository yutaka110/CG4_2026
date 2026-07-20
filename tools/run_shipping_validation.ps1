[CmdletBinding()]
param(
    [switch]$SkipBuild,
    [switch]$RequireCleanTree
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputDirectory = Join-Path (Split-Path -Parent $repoRoot) 'generated\outputs\Shipping'
$executable = Join-Path $outputDirectory 'GE3Shipping.exe'
$engineModulePaths = @('GE3.EngineCore', 'GE3.EngineRenderer', 'GE3.EngineRuntime') | ForEach-Object {
    Join-Path (Split-Path -Parent $repoRoot) "generated\lib\Shipping\$_.lib"
}
$runtimeReportPath = Join-Path $repoRoot 'logs\shipping_verification.json'
$separationReportPath = Join-Path $repoRoot 'logs\target_separation_report.json'
$manifestPath = Join-Path $repoRoot 'logs\shipping_build_manifest.json'
$startedAtUtc = [DateTime]::UtcNow

function Get-SourceState {
    $commit = (& git -C $repoRoot rev-parse HEAD 2>$null)
    if ($LASTEXITCODE -ne 0 -or -not $commit) { throw 'A valid Git HEAD is required.' }
    $status = @(& git -C $repoRoot status --porcelain=v1 --untracked-files=all 2>$null)
    if ($LASTEXITCODE -ne 0) { throw 'Git source state could not be read.' }
    $payload = [string]::Join("`n", $status)
    $bytes = [Text.UTF8Encoding]::new($false).GetBytes($payload)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { $statusHash = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '') }
    finally { $sha.Dispose() }
    return [pscustomobject]@{
        commit = ([string]$commit).Trim()
        dirty = $status.Count -ne 0
        statusEntryCount = $status.Count
        statusSha256 = $statusHash
    }
}

$sourceBefore = Get-SourceState
if ($RequireCleanTree) {
    & (Join-Path $PSScriptRoot 'assert_clean_tree.ps1')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& (Join-Path $PSScriptRoot 'check_target_separation.ps1') -ReportPath $separationReportPath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration Shipping -Clean
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    Write-Error "Shipping executable is missing: $executable"
    exit 7
}
foreach ($modulePath in $engineModulePaths) {
    if (-not (Test-Path -LiteralPath $modulePath -PathType Leaf) -or
        (Get-Item -LiteralPath $modulePath).Length -le 0) {
        Write-Error "Engine module static library is missing or empty: $modulePath"
        exit 7
    }
}

if (Test-Path -LiteralPath $runtimeReportPath -PathType Leaf) {
    Remove-Item -LiteralPath $runtimeReportPath -Force
}
$verificationProcess = Start-Process -FilePath $executable -ArgumentList '--shipping-verify' -WorkingDirectory $repoRoot -WindowStyle Hidden -Wait -PassThru
$verificationExitCode = $verificationProcess.ExitCode
if ($verificationExitCode -ne 0) {
    Write-Error "Shipping verification exited with $verificationExitCode."
    exit 7
}
if (-not (Test-Path -LiteralPath $runtimeReportPath -PathType Leaf)) {
    Write-Error 'Shipping verification report was not produced.'
    exit 7
}
$runtimeReport = Get-Content -Raw -LiteralPath $runtimeReportPath | ConvertFrom-Json
if ($runtimeReport.schema -ne 'ge3.shippingVerification.v2' -or
    $runtimeReport.runtimeApi -ne 'ge3.runtimeHost.v1' -or
    -not $runtimeReport.passed -or
    $runtimeReport.editorCompiled -or
    $runtimeReport.imguiCompiled -or
    $runtimeReport.sourceAssetsRequired -or
    [int]$runtimeReport.framesPresented -lt 3) {
    Write-Error 'Shipping runtime verification report is invalid.'
    exit 7
}

$editorCommandProcess = Start-Process -FilePath $executable -ArgumentList '--editor-core-regression' -WorkingDirectory $repoRoot -WindowStyle Hidden -Wait -PassThru
$editorCommandExitCode = $editorCommandProcess.ExitCode
if ($editorCommandExitCode -ne 64) {
    Write-Error "Shipping accepted an Editor-only command or returned an unexpected code: $editorCommandExitCode"
    exit 7
}

& (Join-Path $PSScriptRoot 'check_target_separation.ps1') -ReportPath $separationReportPath
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$separationReport = Get-Content -Raw -LiteralPath $separationReportPath | ConvertFrom-Json
$sourceAfter = Get-SourceState
$sourceStable =
    $sourceBefore.commit -eq $sourceAfter.commit -and
    $sourceBefore.statusSha256 -eq $sourceAfter.statusSha256
if (-not $sourceStable) {
    Write-Error 'Source revision or working-tree state changed during Shipping validation.'
    exit 7
}
if ($RequireCleanTree -and $sourceAfter.dirty) {
    Write-Error 'Shipping validation ended with a dirty source tree.'
    exit 7
}

$outputFiles = @(Get-ChildItem -LiteralPath $outputDirectory -File | Sort-Object Name | ForEach-Object {
    [pscustomobject]@{
        name = $_.Name
        length = $_.Length
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
    }
})
$expectedNames = @('GE3Shipping.exe', 'shipping_target.json', 'THIRD_PARTY_NOTICES.md')
$unexpectedNames = @($outputFiles.name | Where-Object { $_ -notin $expectedNames })
$missingNames = @($expectedNames | Where-Object { $_ -notin $outputFiles.name })
if ($unexpectedNames.Count -ne 0 -or $missingNames.Count -ne 0) {
    Write-Error "Shipping output contract failed. Missing: $($missingNames -join ', '); unexpected: $($unexpectedNames -join ', ')"
    exit 7
}

$engineModules = @($engineModulePaths | ForEach-Object {
    $item = Get-Item -LiteralPath $_
    [ordered]@{
        name = $item.BaseName
        path = $item.FullName
        length = $item.Length
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $item.FullName).Hash
    }
})
$manifest = [ordered]@{
    schema = 'ge3.shippingBuild.v3'
    startedAtUtc = $startedAtUtc.ToString('o')
    completedAtUtc = [DateTime]::UtcNow.ToString('o')
    passed = $true
    configuration = 'Shipping'
    target = 'GE3.Runtime'
    commit = $sourceAfter.commit
    dirty = $sourceAfter.dirty
    cleanTreeRequired = [bool]$RequireCleanTree
    cleanTreeSatisfied = -not $sourceAfter.dirty
    sourceRevisionStable = $sourceStable
    runtimeApi = 'ge3.runtimeHost.v1'
    engineModules = $engineModules
    engineRuntimeLibrary = @($engineModules | Where-Object name -eq 'GE3.EngineRuntime')[0]
    editorCommandExitCode = $editorCommandExitCode
    runtimeVerification = $runtimeReport
    targetSeparation = $separationReport
    outputFiles = $outputFiles
}
$manifestDirectory = Split-Path -Parent $manifestPath
if (-not (Test-Path -LiteralPath $manifestDirectory)) {
    New-Item -ItemType Directory -Path $manifestDirectory -Force | Out-Null
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding utf8
Write-Output 'Shipping validation result: ready'
Write-Output "Manifest: $manifestPath"
exit 0
