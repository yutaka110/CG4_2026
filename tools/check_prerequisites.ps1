[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Development', 'Release')]
    [string]$Configuration = 'Development',
    [switch]$Json
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$requiredSdk = '10.0.26100.0'

function Resolve-MSBuildPath {
    if ($env:GE3_MSBUILD_PATH -and (Test-Path -LiteralPath $env:GE3_MSBUILD_PATH)) {
        return (Resolve-Path -LiteralPath $env:GE3_MSBUILD_PATH).Path
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $installation = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
        if ($LASTEXITCODE -eq 0 -and $installation) {
            $candidate = Join-Path ($installation | Select-Object -First 1) 'MSBuild\Current\Bin\amd64\MSBuild.exe'
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

$msbuildPath = Resolve-MSBuildPath
$toolsetPath = $null
if ($msbuildPath) {
    $visualStudioRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $msbuildPath))))
    $toolsetPath = Get-ChildItem -Path (Join-Path $visualStudioRoot 'MSBuild\Microsoft\VC\*\Platforms\x64\PlatformToolsets\v145\Toolset.targets') -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}
$sdkRoot = Resolve-WindowsSdkRoot
$sdkInclude = Join-Path $sdkRoot "Include\$requiredSdk"
$dxcPath = Join-Path $sdkRoot "bin\$requiredSdk\x64\dxcompiler.dll"
$dxilPath = Join-Path $sdkRoot "bin\$requiredSdk\x64\dxil.dll"
$assimpLibrary = if ($Configuration -eq 'Debug') {
    Join-Path $repoRoot 'externals\assimp\lib\Debug\assimp-vc143-mtd.lib'
} else {
    Join-Path $repoRoot 'externals\assimp\lib\Release\assimp-vc143-mt.lib'
}

$checks = @(
    [pscustomobject]@{ Name = 'Solution'; Path = (Join-Path $repoRoot 'CG4.sln'); Exists = (Test-Path -LiteralPath (Join-Path $repoRoot 'CG4.sln')); Repair = 'Restore CG4.sln from source control.' },
    [pscustomobject]@{ Name = 'MSBuild'; Path = $msbuildPath; Exists = [bool]$msbuildPath; Repair = 'Install Visual Studio with Desktop development with C++, or set GE3_MSBUILD_PATH.' },
    [pscustomobject]@{ Name = 'Visual C++ v145'; Path = $toolsetPath; Exists = [bool]$toolsetPath; Repair = 'Install the MSVC v145 x64/x86 build tools component.' },
    [pscustomobject]@{ Name = 'Windows SDK'; Path = $sdkInclude; Exists = (Test-Path -LiteralPath $sdkInclude); Repair = "Install Windows SDK $requiredSdk." },
    [pscustomobject]@{ Name = 'DXC'; Path = $dxcPath; Exists = (Test-Path -LiteralPath $dxcPath); Repair = "Install the x64 tools for Windows SDK $requiredSdk." },
    [pscustomobject]@{ Name = 'DXIL'; Path = $dxilPath; Exists = (Test-Path -LiteralPath $dxilPath); Repair = "Install the x64 tools for Windows SDK $requiredSdk." },
    [pscustomobject]@{ Name = 'Assimp'; Path = $assimpLibrary; Exists = (Test-Path -LiteralPath $assimpLibrary); Repair = 'Restore the approved Assimp prebuilt library from source control.' },
    [pscustomobject]@{ Name = 'DirectXTex'; Path = (Join-Path $repoRoot 'externals\DirectXTex\DirectXTex_Desktop_2022_Win10.vcxproj'); Exists = (Test-Path -LiteralPath (Join-Path $repoRoot 'externals\DirectXTex\DirectXTex_Desktop_2022_Win10.vcxproj')); Repair = 'Restore the DirectXTex subproject.' }
)

$failed = @($checks | Where-Object { -not $_.Exists })
$result = [ordered]@{
    schema = 'ge3.prerequisites.v1'
    configuration = $Configuration
    repositoryRoot = $repoRoot
    msbuildPath = $msbuildPath
    windowsSdkVersion = $requiredSdk
    windowsSdkRoot = $sdkRoot
    passed = ($failed.Count -eq 0)
    checks = $checks
}

if ($Json) {
    $result | ConvertTo-Json -Depth 5
} else {
    foreach ($check in $checks) {
        $state = if ($check.Exists) { 'OK' } else { 'MISSING' }
        Write-Output ("[{0}] {1}: {2}" -f $state, $check.Name, $check.Path)
        if (-not $check.Exists) { Write-Output ("  Repair: {0}" -f $check.Repair) }
    }
}

if ($failed.Count -ne 0) { exit 2 }
exit 0
