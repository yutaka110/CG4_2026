[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Development', 'Release', 'Shipping')]
    [string]$Configuration = 'Development',
    [switch]$Json,
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$lockPath = Join-Path $repoRoot 'Build\GE3.Dependencies.lock.json'

function Resolve-MSBuildPath {
    if ($env:GE3_MSBUILD_PATH -and (Test-Path -LiteralPath $env:GE3_MSBUILD_PATH)) {
        return (Resolve-Path -LiteralPath $env:GE3_MSBUILD_PATH).Path
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $installations = @(& $vswhere -products * -requires Microsoft.Component.MSBuild -property installationPath)
        foreach ($installation in $installations) {
            $candidate = Join-Path $installation 'MSBuild\Current\Bin\amd64\MSBuild.exe'
            if (Test-Path -LiteralPath $candidate) { return $candidate }
        }
    }

    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe'
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    return $null
}

function Resolve-WindowsSdkRoot {
    $registryPaths = @(
        'HKLM:\SOFTWARE\Microsoft\Windows Kits\Installed Roots',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows Kits\Installed Roots'
    )
    foreach ($registryPath in $registryPaths) {
        if (Test-Path $registryPath) {
            $value = (Get-ItemProperty -Path $registryPath -ErrorAction SilentlyContinue).KitsRoot10
            if ($value) { return $value }
        }
    }
    return (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10')
}

function Get-Sha256([string]$Path) {
    if (-not $Path -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash
}

function Get-FileVersionRecord([string]$Path) {
    if (-not $Path -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
    $version = (Get-Item -LiteralPath $Path).VersionInfo
    return [pscustomobject]@{
        fileVersion = $version.FileVersion
        productVersion = $version.ProductVersion
    }
}

function Get-GitTrackedTreeSha256([string]$RelativePath) {
    $lines = @(& git -C $repoRoot ls-tree -r HEAD -- $RelativePath 2>$null |
        Where-Object { $_ -notmatch '/LICENSE\.txt$' } |
        Sort-Object)
    if ($LASTEXITCODE -ne 0 -or $lines.Count -eq 0) { return $null }
    $payload = [string]::Join("`n", $lines) + "`n"
    $bytes = [System.Text.UTF8Encoding]::new($false).GetBytes($payload)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '')
    } finally {
        $sha.Dispose()
    }
}

$checks = [System.Collections.Generic.List[object]]::new()
function Add-Check(
    [string]$Name,
    [string]$Path,
    [object]$Expected,
    [object]$Actual,
    [bool]$Passed,
    [string]$Repair) {
    $checks.Add([pscustomobject]@{
        name = $Name
        path = $Path
        expected = $Expected
        actual = $Actual
        passed = $Passed
        repair = $Repair
    })
}

if (-not (Test-Path -LiteralPath $lockPath -PathType Leaf)) {
    Add-Check 'Dependency lock' $lockPath 'ge3.dependencies.v1' $null $false 'Restore Build/GE3.Dependencies.lock.json from source control.'
    $lock = $null
} else {
    try {
        $lock = Get-Content -Raw -LiteralPath $lockPath | ConvertFrom-Json
        Add-Check 'Dependency lock' $lockPath 'ge3.dependencies.v1' $lock.schema ($lock.schema -eq 'ge3.dependencies.v1') 'Regenerate the dependency lock using an approved toolchain review.'
    } catch {
        $lock = $null
        Add-Check 'Dependency lock' $lockPath 'valid JSON' $_.Exception.Message $false 'Repair the dependency lock JSON.'
    }
}

$requiredSdk = if ($lock) { [string]$lock.toolchain.windowsSdkVersion } else { '10.0.26100.0' }
$requiredToolset = if ($lock) { [string]$lock.toolchain.platformToolset } else { 'v145' }
$msbuildPath = Resolve-MSBuildPath
$visualStudioRoot = $null
$toolsetPath = $null
$compilerPath = $null
if ($msbuildPath) {
    $visualStudioRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $msbuildPath))))
    $toolsetPath = Get-ChildItem -Path (Join-Path $visualStudioRoot "MSBuild\Microsoft\VC\*\Platforms\x64\PlatformToolsets\$requiredToolset\Toolset.targets") -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
    $compilerCandidates = @(Get-ChildItem -Path (Join-Path $visualStudioRoot 'VC\Tools\MSVC\*\bin\Hostx64\x64\cl.exe') -ErrorAction SilentlyContinue |
        Sort-Object FullName -Descending)
    if ($lock) {
        $compilerPath = $compilerCandidates |
            Where-Object { $_.VersionInfo.FileVersion -eq $lock.toolchain.compiler.fileVersion } |
            Select-Object -First 1 -ExpandProperty FullName
    }
    if (-not $compilerPath) {
        $compilerPath = $compilerCandidates | Select-Object -First 1 -ExpandProperty FullName
    }
}

$sdkRoot = Resolve-WindowsSdkRoot
$sdkInclude = Join-Path $sdkRoot "Include\$requiredSdk"
$dxcPath = Join-Path $sdkRoot "bin\$requiredSdk\x64\dxcompiler.dll"
$dxilPath = Join-Path $sdkRoot "bin\$requiredSdk\x64\dxil.dll"
$sdkLicensePath = Join-Path $sdkRoot "Licenses\$requiredSdk\sdk_license.rtf"
$sdkNoticesPath = Join-Path $sdkRoot "Licenses\$requiredSdk\sdk_third_party_notices.rtf"

Add-Check 'Solution' (Join-Path $repoRoot 'CG4.sln') 'present' (Test-Path -LiteralPath (Join-Path $repoRoot 'CG4.sln')) (Test-Path -LiteralPath (Join-Path $repoRoot 'CG4.sln')) 'Restore CG4.sln from source control.'
Add-Check 'Git repository' (Join-Path $repoRoot '.git') 'present' (Test-Path -LiteralPath (Join-Path $repoRoot '.git')) (Test-Path -LiteralPath (Join-Path $repoRoot '.git')) 'Run validation from a complete Git checkout.'
Add-Check 'MSBuild' $msbuildPath 'present' ([bool]$msbuildPath) ([bool]$msbuildPath) 'Install the pinned Visual Studio C++ toolchain or set GE3_MSBUILD_PATH.'
Add-Check "Visual C++ $requiredToolset" $toolsetPath 'present' ([bool]$toolsetPath) ([bool]$toolsetPath) "Install the MSVC $requiredToolset x64/x86 build tools component."
Add-Check 'MSVC compiler' $compilerPath 'present' ([bool]$compilerPath) ([bool]$compilerPath) 'Install the compiler version recorded in Build/GE3.Dependencies.lock.json.'
Add-Check 'Windows SDK' $sdkInclude $requiredSdk (Test-Path -LiteralPath $sdkInclude) (Test-Path -LiteralPath $sdkInclude) "Install Windows SDK $requiredSdk."

$msbuildVersion = Get-FileVersionRecord $msbuildPath
$compilerVersion = Get-FileVersionRecord $compilerPath
$dxcVersion = Get-FileVersionRecord $dxcPath
$dxilVersion = Get-FileVersionRecord $dxilPath
if ($lock) {
    Add-Check 'MSBuild file version' $msbuildPath $lock.toolchain.msbuild.fileVersion $msbuildVersion.fileVersion ($msbuildVersion -and $msbuildVersion.fileVersion -eq $lock.toolchain.msbuild.fileVersion) 'Install the exact approved Visual Studio servicing version or review and update the lock.'
    Add-Check 'MSBuild product version' $msbuildPath $lock.toolchain.msbuild.productVersion $msbuildVersion.productVersion ($msbuildVersion -and $msbuildVersion.productVersion -eq $lock.toolchain.msbuild.productVersion) 'Install the exact approved Visual Studio servicing version or review and update the lock.'
    Add-Check 'MSBuild SHA-256' $msbuildPath $lock.toolchain.msbuild.sha256 (Get-Sha256 $msbuildPath) ((Get-Sha256 $msbuildPath) -eq $lock.toolchain.msbuild.sha256) 'Restore the approved MSBuild binary by repairing Visual Studio.'
    Add-Check 'MSVC file version' $compilerPath $lock.toolchain.compiler.fileVersion $compilerVersion.fileVersion ($compilerVersion -and $compilerVersion.fileVersion -eq $lock.toolchain.compiler.fileVersion) 'Install the exact approved MSVC servicing version or review and update the lock.'
    Add-Check 'MSVC product version' $compilerPath $lock.toolchain.compiler.productVersion $compilerVersion.productVersion ($compilerVersion -and $compilerVersion.productVersion -eq $lock.toolchain.compiler.productVersion) 'Install the exact approved MSVC servicing version or review and update the lock.'
    Add-Check 'MSVC SHA-256' $compilerPath $lock.toolchain.compiler.sha256 (Get-Sha256 $compilerPath) ((Get-Sha256 $compilerPath) -eq $lock.toolchain.compiler.sha256) 'Restore the approved compiler binary by repairing Visual Studio.'
    Add-Check 'Toolset targets SHA-256' $toolsetPath $lock.toolchain.toolsetTargetsSha256 (Get-Sha256 $toolsetPath) ((Get-Sha256 $toolsetPath) -eq $lock.toolchain.toolsetTargetsSha256) 'Repair the approved MSVC toolset installation.'
    Add-Check 'DXC version' $dxcPath $lock.toolchain.dxcompiler.fileVersion $dxcVersion.fileVersion ($dxcVersion -and $dxcVersion.fileVersion -eq $lock.toolchain.dxcompiler.fileVersion) 'Repair the approved Windows SDK installation.'
    Add-Check 'DXC SHA-256' $dxcPath $lock.toolchain.dxcompiler.sha256 (Get-Sha256 $dxcPath) ((Get-Sha256 $dxcPath) -eq $lock.toolchain.dxcompiler.sha256) 'Repair the approved Windows SDK installation.'
    Add-Check 'DXIL version' $dxilPath $lock.toolchain.dxil.fileVersion $dxilVersion.fileVersion ($dxilVersion -and $dxilVersion.fileVersion -eq $lock.toolchain.dxil.fileVersion) 'Repair the approved Windows SDK installation.'
    Add-Check 'DXIL SHA-256' $dxilPath $lock.toolchain.dxil.sha256 (Get-Sha256 $dxilPath) ((Get-Sha256 $dxilPath) -eq $lock.toolchain.dxil.sha256) 'Repair the approved Windows SDK installation.'
    Add-Check 'Windows SDK license SHA-256' $sdkLicensePath $lock.toolchain.sdkLicenseSha256 (Get-Sha256 $sdkLicensePath) ((Get-Sha256 $sdkLicensePath) -eq $lock.toolchain.sdkLicenseSha256) 'Repair the Windows SDK license payload.'
    Add-Check 'Windows SDK notices SHA-256' $sdkNoticesPath $lock.toolchain.sdkThirdPartyNoticesSha256 (Get-Sha256 $sdkNoticesPath) ((Get-Sha256 $sdkNoticesPath) -eq $lock.toolchain.sdkThirdPartyNoticesSha256) 'Repair the Windows SDK third-party notices payload.'

    foreach ($dependency in $lock.dependencies) {
        $licensePath = Join-Path $repoRoot $dependency.licensePath
        Add-Check "$($dependency.id) license" $licensePath $dependency.licenseSha256 (Get-Sha256 $licensePath) ((Get-Sha256 $licensePath) -eq $dependency.licenseSha256) "Restore the approved $($dependency.id) license notice."
        $relativeRoot = Split-Path -Parent $dependency.licensePath
        $treeHash = Get-GitTrackedTreeSha256 $relativeRoot
        Add-Check "$($dependency.id) tracked tree" $relativeRoot $dependency.trackedTreeSha256 $treeHash ($treeHash -eq $dependency.trackedTreeSha256) "Restore or explicitly review and relock the $($dependency.id) source tree."
        if ($dependency.projectPath) {
            $projectPath = Join-Path $repoRoot $dependency.projectPath
            Add-Check "$($dependency.id) project" $projectPath 'present' (Test-Path -LiteralPath $projectPath) (Test-Path -LiteralPath $projectPath) "Restore the approved $($dependency.id) project."
        }
        if ($dependency.artifacts) {
            foreach ($artifactProperty in $dependency.artifacts.psobject.Properties) {
                $artifact = $artifactProperty.Value
                $artifactPath = Join-Path $repoRoot $artifact.path
                Add-Check "$($dependency.id) $($artifactProperty.Name) artifact" $artifactPath $artifact.sha256 (Get-Sha256 $artifactPath) ((Get-Sha256 $artifactPath) -eq $artifact.sha256) "Restore the approved $($dependency.id) $($artifactProperty.Name) artifact."
            }
        }
    }
}

$failed = @($checks | Where-Object { -not $_.passed })
$result = [ordered]@{
    schema = 'ge3.prerequisites.v2'
    configuration = $Configuration
    repositoryRoot = $repoRoot
    dependencyLockPath = $lockPath
    dependencyLockSha256 = Get-Sha256 $lockPath
    msbuildPath = $msbuildPath
    compilerPath = $compilerPath
    windowsSdkVersion = $requiredSdk
    windowsSdkRoot = $sdkRoot
    toolchain = [ordered]@{
        platformToolset = $requiredToolset
        msbuildVersion = $msbuildVersion
        msbuildSha256 = Get-Sha256 $msbuildPath
        compilerVersion = $compilerVersion
        compilerSha256 = Get-Sha256 $compilerPath
        dxcompilerVersion = $dxcVersion
        dxcompilerSha256 = Get-Sha256 $dxcPath
        dxilVersion = $dxilVersion
        dxilSha256 = Get-Sha256 $dxilPath
    }
    passed = ($failed.Count -eq 0)
    checks = $checks
}

$serializedResult = $result | ConvertTo-Json -Depth 8
if ($ReportPath) {
    $resolvedReportPath = if ([System.IO.Path]::IsPathRooted($ReportPath)) {
        $ReportPath
    } else {
        Join-Path $repoRoot $ReportPath
    }
    $reportDirectory = Split-Path -Parent $resolvedReportPath
    if ($reportDirectory) {
        New-Item -ItemType Directory -Force -Path $reportDirectory | Out-Null
    }
    $serializedResult | Set-Content -LiteralPath $resolvedReportPath -Encoding UTF8
}

if ($Json) {
    Write-Output $serializedResult
} else {
    foreach ($check in $checks) {
        $state = if ($check.passed) { 'OK' } else { 'FAILED' }
        Write-Output ("[{0}] {1}: {2}" -f $state, $check.name, $check.path)
        if (-not $check.passed) {
            Write-Output ("  Expected: {0}" -f $check.expected)
            Write-Output ("  Actual:   {0}" -f $check.actual)
            Write-Output ("  Repair:   {0}" -f $check.repair)
        }
    }
}

if ($failed.Count -ne 0) { exit 2 }
exit 0
