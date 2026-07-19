[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Development')]
    [string]$Configuration = 'Development',
    [switch]$SkipBuild,
    [switch]$CommercialOnly
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$validationStarted = [DateTime]::UtcNow

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Clean
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$prerequisiteJson = & (Join-Path $PSScriptRoot 'check_prerequisites.ps1') -Configuration $Configuration -Json
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$prerequisites = $prerequisiteJson | ConvertFrom-Json

$executable = Join-Path (Split-Path -Parent $repoRoot) "generated\outputs\$Configuration\GE3.exe"
if (-not (Test-Path -LiteralPath $executable)) {
    Write-Error "Editor executable was not produced: $executable"
    exit 2
}

function Invoke-EditorGate {
    param([string]$Name, [string[]]$Arguments)
    $started = [DateTime]::UtcNow
    $process = Start-Process -FilePath $executable -ArgumentList $Arguments -WorkingDirectory $repoRoot -Wait -PassThru -WindowStyle Hidden
    return [pscustomobject]@{
        name = $Name
        arguments = $Arguments
        startedAtUtc = $started.ToString('o')
        completedAtUtc = [DateTime]::UtcNow.ToString('o')
        exitCode = $process.ExitCode
    }
}

$runs = [System.Collections.Generic.List[object]]::new()
if (-not $CommercialOnly) {
    $runs.Add((Invoke-EditorGate 'Editor Core Regression' @('--editor-core-regression')))
    $runs.Add((Invoke-EditorGate 'Effect Authoring Smoke' @('--effect-authoring-smoke')))
    $runs.Add((Invoke-EditorGate 'Editor Smoke Run' @('--editor-smoke-run')))
}
$runs.Add((Invoke-EditorGate 'Commercial Automation Gates' @('--editor-commercial-gates')))

$requiredArtifacts = @(
    'editor_core_regression.log',
    'effect_authoring_smoke.log',
    'editor_smoke_run.log',
    'logs/editor_automation_report.json',
    'logs/editor_automation_report.md',
    'logs/editor_performance_budget_report.log',
    'logs/editor_feature_guard_report.log'
)
$artifacts = [System.Collections.Generic.List[object]]::new()
$artifactFailure = $false
foreach ($relativePath in $requiredArtifacts) {
    $path = Join-Path $repoRoot $relativePath
    $exists = Test-Path -LiteralPath $path
    $fresh = $false
    $hash = $null
    $length = 0
    $lastWrite = $null
    if ($exists) {
        $item = Get-Item -LiteralPath $path
        $lastWrite = $item.LastWriteTimeUtc.ToString('o')
        $length = $item.Length
        $fresh = $item.LastWriteTimeUtc -ge $validationStarted.AddSeconds(-1) -and $item.Length -gt 0
        if ($fresh) { $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash }
    }
    if (-not $fresh) { $artifactFailure = $true }
    $artifacts.Add([pscustomobject]@{
        path = $relativePath.Replace('\', '/')
        exists = $exists
        fresh = $fresh
        length = $length
        lastWriteAtUtc = $lastWrite
        sha256 = $hash
    })
}

$commercialReportPath = Join-Path $repoRoot 'logs\editor_automation_report.json'
$commercialReport = if (Test-Path -LiteralPath $commercialReportPath) {
    Get-Content -Raw -LiteralPath $commercialReportPath | ConvertFrom-Json
} else { $null }
$commercialReady = $commercialReport -and $commercialReport.commercialCompletionReady -eq $true

$commit = (& git -C $repoRoot rev-parse HEAD 2>$null)
$dirty = [bool]((& git -C $repoRoot status --porcelain 2>$null) | Select-Object -First 1)
$gpuAdapters = @()
try {
    $gpuAdapters = @(Get-CimInstance Win32_VideoController -ErrorAction Stop | ForEach-Object {
        [pscustomobject]@{ name = $_.Name; driverVersion = $_.DriverVersion }
    })
} catch {
    $gpuAdapters = @([pscustomobject]@{ name = 'unavailable'; driverVersion = $null })
}

$runFailure = @($runs | Where-Object { $_.exitCode -ne 0 }).Count -ne 0
$passed = -not $runFailure -and -not $artifactFailure -and $commercialReady
$manifest = [ordered]@{
    schema = 'ge3.commercialValidation.v1'
    startedAtUtc = $validationStarted.ToString('o')
    completedAtUtc = [DateTime]::UtcNow.ToString('o')
    repositoryRoot = $repoRoot
    commit = $commit
    dirty = $dirty
    configuration = $Configuration
    platform = 'x64'
    toolset = 'v145'
    msbuildPath = $prerequisites.msbuildPath
    windowsSdkVersion = $prerequisites.windowsSdkVersion
    executable = $executable
    executableSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $executable).Hash
    gpuAdapters = $gpuAdapters
    gateSchema = if ($commercialReport) { $commercialReport.schema } else { $null }
    commercialReady = [bool]$commercialReady
    passed = $passed
    runs = $runs
    artifacts = $artifacts
}

$logsDirectory = Join-Path $repoRoot 'logs'
New-Item -ItemType Directory -Force -Path $logsDirectory | Out-Null
$manifestPath = Join-Path $logsDirectory 'editor_build_manifest.json'
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Write-Output ("Commercial validation result: {0}" -f $(if ($passed) { 'ready' } else { 'not-ready' }))
Write-Output ("Manifest: {0}" -f $manifestPath)
if (-not $passed) { exit 1 }
exit 0
