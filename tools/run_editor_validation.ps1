[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Development')]
    [string]$Configuration = 'Development',
    [switch]$SkipBuild,
    [switch]$CommercialOnly,
    [switch]$RequireCleanTree
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$validationStarted = [DateTime]::UtcNow

function Get-GitSourceState {
    $commit = (& git -C $repoRoot rev-parse HEAD 2>$null)
    if ($LASTEXITCODE -ne 0 -or -not $commit) {
        throw 'A valid Git HEAD is required for commercial validation.'
    }
    $statusLines = @(& git -C $repoRoot status --porcelain=v1 --untracked-files=all 2>$null)
    if ($LASTEXITCODE -ne 0) {
        throw 'Git working tree state could not be read.'
    }
    $statusPayload = [string]::Join("`n", $statusLines)
    $statusBytes = [System.Text.UTF8Encoding]::new($false).GetBytes($statusPayload)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $statusHash = ([BitConverter]::ToString($sha.ComputeHash($statusBytes))).Replace('-', '')
    } finally {
        $sha.Dispose()
    }
    return [pscustomobject]@{
        commit = ([string]$commit).Trim()
        dirty = $statusLines.Count -ne 0
        statusEntryCount = $statusLines.Count
        statusSha256 = $statusHash
    }
}

function Get-DirectoryDigest([string]$Directory) {
    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) { return $null }
    $root = (Resolve-Path -LiteralPath $Directory).Path
    $entries = [System.Collections.Generic.List[string]]::new()
    [uint64]$totalBytes = 0
    $files = @(Get-ChildItem -LiteralPath $root -Recurse -File | Sort-Object FullName)
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($root.Length).TrimStart('\', '/').Replace('\', '/')
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash
        $entries.Add(("{0}`t{1}`t{2}" -f $relative, $file.Length, $hash))
        $totalBytes += [uint64]$file.Length
    }
    $payload = [string]::Join("`n", $entries) + "`n"
    $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($payload)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $digest = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '')
    } finally {
        $sha.Dispose()
    }
    return [pscustomobject]@{
        path = $root
        fileCount = $files.Count
        totalBytes = $totalBytes
        manifestSha256 = $digest
    }
}

$sourceStateBefore = Get-GitSourceState
if ($RequireCleanTree -and $sourceStateBefore.dirty) {
    [Console]::Error.WriteLine(
        ("Commercial clean-tree validation rejected {0} changed/untracked path(s). Commit or remove the changes, then rerun." -f $sourceStateBefore.statusEntryCount))
    exit 3
}

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration -Clean
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$prerequisiteJson = & (Join-Path $PSScriptRoot 'check_prerequisites.ps1') -Configuration $Configuration -Json
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$prerequisites = $prerequisiteJson | ConvertFrom-Json

$executable = Join-Path (Split-Path -Parent $repoRoot) "generated\outputs\$Configuration\GE3.exe"
$engineModulePaths = @('GE3.EngineCore', 'GE3.EngineRenderer', 'GE3.EngineRuntime') | ForEach-Object {
    Join-Path (Split-Path -Parent $repoRoot) "generated\lib\$Configuration\$_.lib"
}
if (-not (Test-Path -LiteralPath $executable)) {
    Write-Error "Editor executable was not produced: $executable"
    exit 2
}
foreach ($modulePath in $engineModulePaths) {
    if (-not (Test-Path -LiteralPath $modulePath -PathType Leaf) -or
        (Get-Item -LiteralPath $modulePath).Length -le 0) {
        Write-Error "Engine module static library was not produced: $modulePath"
        exit 2
    }
}

$outputDirectory = Split-Path -Parent $executable
$dependencyLock = Get-Content -Raw -LiteralPath $prerequisites.dependencyLockPath | ConvertFrom-Json
$deploymentExpectations = @(
    [pscustomobject]@{ name = 'Executable'; path = $executable; expectedHash = $null; source = $null },
    [pscustomobject]@{ name = 'DXC'; path = (Join-Path $outputDirectory 'dxcompiler.dll'); expectedHash = $dependencyLock.toolchain.dxcompiler.sha256; source = $null },
    [pscustomobject]@{ name = 'DXIL'; path = (Join-Path $outputDirectory 'dxil.dll'); expectedHash = $dependencyLock.toolchain.dxil.sha256; source = $null },
    [pscustomobject]@{ name = 'Third-party notices'; path = (Join-Path $outputDirectory 'THIRD_PARTY_NOTICES.md'); expectedHash = $null; source = (Join-Path $repoRoot 'THIRD_PARTY_NOTICES.md') },
    [pscustomobject]@{ name = 'Assimp license'; path = (Join-Path $outputDirectory 'Licenses\Assimp_LICENSE.txt'); expectedHash = $null; source = (Join-Path $repoRoot 'externals\assimp\LICENSE.txt') },
    [pscustomobject]@{ name = 'DirectXTex license'; path = (Join-Path $outputDirectory 'Licenses\DirectXTex_LICENSE.txt'); expectedHash = $null; source = (Join-Path $repoRoot 'externals\DirectXTex\LICENSE.txt') },
    [pscustomobject]@{ name = 'Dear ImGui license'; path = (Join-Path $outputDirectory 'Licenses\DearImGui_LICENSE.txt'); expectedHash = $null; source = (Join-Path $repoRoot 'externals\imgui\LICENSE.txt') },
    [pscustomobject]@{ name = 'Windows SDK license'; path = (Join-Path $outputDirectory 'Licenses\WindowsSDK_LICENSE.rtf'); expectedHash = $dependencyLock.toolchain.sdkLicenseSha256; source = $null },
    [pscustomobject]@{ name = 'Windows SDK notices'; path = (Join-Path $outputDirectory 'Licenses\WindowsSDK_THIRD_PARTY_NOTICES.rtf'); expectedHash = $dependencyLock.toolchain.sdkThirdPartyNoticesSha256; source = $null }
)
$deploymentArtifacts = [System.Collections.Generic.List[object]]::new()
$deploymentFailure = $false
foreach ($expectation in $deploymentExpectations) {
    $exists = Test-Path -LiteralPath $expectation.path -PathType Leaf
    $length = 0
    $actualHash = $null
    $expectedHash = $expectation.expectedHash
    if ($expectation.source -and (Test-Path -LiteralPath $expectation.source -PathType Leaf)) {
        $expectedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $expectation.source).Hash
    }
    if ($exists) {
        $item = Get-Item -LiteralPath $expectation.path
        $length = $item.Length
        if ($length -gt 0) {
            $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $expectation.path).Hash
        }
    }
    $valid = $exists -and $length -gt 0 -and (-not $expectedHash -or $actualHash -eq $expectedHash)
    if (-not $valid) { $deploymentFailure = $true }
    $deploymentArtifacts.Add([pscustomobject]@{
        name = $expectation.name
        path = $expectation.path
        exists = $exists
        valid = $valid
        length = $length
        sha256 = $actualHash
        expectedSha256 = $expectedHash
    })
}
$resourcesDigest = Get-DirectoryDigest (Join-Path $outputDirectory 'Resources')
if (-not $resourcesDigest -or $resourcesDigest.fileCount -eq 0) {
    $deploymentFailure = $true
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

$sourceStateAfter = Get-GitSourceState
$sourceRevisionStable =
    $sourceStateBefore.commit -eq $sourceStateAfter.commit -and
    $sourceStateBefore.statusSha256 -eq $sourceStateAfter.statusSha256
$gpuAdapters = @()
try {
    $gpuAdapters = @(Get-CimInstance Win32_VideoController -ErrorAction Stop | ForEach-Object {
        [pscustomobject]@{ name = $_.Name; driverVersion = $_.DriverVersion }
    })
} catch {
    $gpuAdapters = @([pscustomobject]@{ name = 'unavailable'; driverVersion = $null })
}

$runFailure = @($runs | Where-Object { $_.exitCode -ne 0 }).Count -ne 0
$cleanTreeSatisfied = -not $RequireCleanTree -or -not $sourceStateAfter.dirty
$passed = -not $runFailure -and -not $artifactFailure -and -not $deploymentFailure -and $commercialReady -and
    $sourceRevisionStable -and $cleanTreeSatisfied
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
    schema = 'ge3.commercialValidation.v3'
    startedAtUtc = $validationStarted.ToString('o')
    completedAtUtc = [DateTime]::UtcNow.ToString('o')
    repositoryRoot = $repoRoot
    commit = $sourceStateAfter.commit
    dirty = $sourceStateAfter.dirty
    cleanTreeRequired = [bool]$RequireCleanTree
    cleanTreeSatisfied = $cleanTreeSatisfied
    sourceRevisionStable = $sourceRevisionStable
    sourceStatusEntryCount = $sourceStateAfter.statusEntryCount
    sourceStatusSha256 = $sourceStateAfter.statusSha256
    configuration = $Configuration
    platform = 'x64'
    toolset = $prerequisites.toolchain.platformToolset
    msbuildPath = $prerequisites.msbuildPath
    msbuildVersion = $prerequisites.toolchain.msbuildVersion
    msbuildSha256 = $prerequisites.toolchain.msbuildSha256
    compilerPath = $prerequisites.compilerPath
    compilerVersion = $prerequisites.toolchain.compilerVersion
    compilerSha256 = $prerequisites.toolchain.compilerSha256
    windowsSdkVersion = $prerequisites.windowsSdkVersion
    dxcompilerVersion = $prerequisites.toolchain.dxcompilerVersion
    dxcompilerSha256 = $prerequisites.toolchain.dxcompilerSha256
    dxilVersion = $prerequisites.toolchain.dxilVersion
    dxilSha256 = $prerequisites.toolchain.dxilSha256
    dependencyLockPath = $prerequisites.dependencyLockPath
    dependencyLockSha256 = $prerequisites.dependencyLockSha256
    executable = $executable
    executableSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $executable).Hash
    runtimeApi = 'ge3.runtimeHost.v1'
    engineModules = $engineModules
    engineRuntimeLibrary = @($engineModules | Where-Object name -eq 'GE3.EngineRuntime')[0]
    deploymentArtifacts = $deploymentArtifacts
    resources = $resourcesDigest
    gpuAdapters = $gpuAdapters
    gateSchema = if ($commercialReport) { $commercialReport.schema } else { $null }
    commercialReady = [bool]$commercialReady
    passed = $passed
    ci = [ordered]@{
        provider = if ($env:GITHUB_ACTIONS -eq 'true') { 'github-actions' } else { 'local' }
        runId = $env:GITHUB_RUN_ID
        runAttempt = $env:GITHUB_RUN_ATTEMPT
        workflow = $env:GITHUB_WORKFLOW
        ref = $env:GITHUB_REF
        sha = $env:GITHUB_SHA
        runnerName = $env:RUNNER_NAME
        runnerOs = $env:RUNNER_OS
        runnerArch = $env:RUNNER_ARCH
        imageOs = $env:ImageOS
        imageVersion = $env:ImageVersion
    }
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
