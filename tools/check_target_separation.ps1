[CmdletBinding()]
param(
    [switch]$Json,
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$solutionPath = Join-Path $repoRoot 'CG4.sln'
$shippingPropsPath = Join-Path $repoRoot 'Build\GE3.Shipping.props'
$releasePropsPath = Join-Path $repoRoot 'Build\GE3.Release.props'
$modulePropsPath = Join-Path $repoRoot 'Build\GE3.EngineModule.props'
$outputDirectory = Join-Path (Split-Path -Parent $repoRoot) 'generated\outputs\Shipping'
$failures = [System.Collections.Generic.List[string]]::new()
$checks = [System.Collections.Generic.List[object]]::new()

$projects = [ordered]@{
    editor = [ordered]@{ name = 'GE3'; path = (Join-Path $repoRoot 'GE3.vcxproj'); guid = 'DFE964E0-98CC-464F-BBC2-818C767A5A3C' }
    shipping = [ordered]@{ name = 'GE3.Runtime'; path = (Join-Path $repoRoot 'GE3.Runtime.vcxproj'); guid = '9B7AE590-5D9D-4F06-AE0A-920A0B0F56A1' }
    runtime = [ordered]@{ name = 'GE3.EngineRuntime'; path = (Join-Path $repoRoot 'GE3.EngineRuntime.vcxproj'); guid = 'A6F3CD21-66AE-48F7-BADC-04F29BFCF25C'; props = (Join-Path $repoRoot 'Build\GE3.EngineRuntime.props') }
    renderer = [ordered]@{ name = 'GE3.EngineRenderer'; path = (Join-Path $repoRoot 'GE3.EngineRenderer.vcxproj'); guid = 'C8707E6F-3D7D-4F62-85E9-1B667E06D4A2'; props = (Join-Path $repoRoot 'Build\GE3.EngineRenderer.props') }
    core = [ordered]@{ name = 'GE3.EngineCore'; path = (Join-Path $repoRoot 'GE3.EngineCore.vcxproj'); guid = 'F3B5EAB1-3AF4-4C55-970E-0A4B9BC6D2A1'; props = (Join-Path $repoRoot 'Build\GE3.EngineCore.props') }
}

function Add-Check([string]$Name, [bool]$Passed, [string]$Expected, [string]$Actual) {
    $script:checks.Add([pscustomobject]@{ name = $Name; passed = $Passed; expected = $Expected; actual = $Actual })
    if (-not $Passed) { $script:failures.Add("$Name`: expected $Expected; actual $Actual") }
}

function Read-Project([string]$Path) {
    $document = [xml](Get-Content -Raw -LiteralPath $Path)
    $namespace = [Xml.XmlNamespaceManager]::new($document.NameTable)
    $namespace.AddNamespace('m', 'http://schemas.microsoft.com/developer/msbuild/2003')
    return [pscustomobject]@{ document = $document; namespace = $namespace; text = (Get-Content -Raw -LiteralPath $Path) }
}

function Get-Includes($Project, [string]$ItemName) {
    return @($Project.document.SelectNodes("//m:$ItemName[@Include]", $Project.namespace) |
        ForEach-Object { $_.Include.Replace('/', '\') })
}

function Test-ExactSet([string[]]$Actual, [string[]]$Expected) {
    return [string]::Join("`n", @($Actual | Sort-Object)) -ceq [string]::Join("`n", @($Expected | Sort-Object))
}

$requiredPaths = @($solutionPath, $shippingPropsPath, $releasePropsPath, $modulePropsPath)
foreach ($project in $projects.Values) {
    $requiredPaths += $project.path
    if ($project.props) { $requiredPaths += $project.props }
}
foreach ($path in $requiredPaths) {
    $present = Test-Path -LiteralPath $path -PathType Leaf
    Add-Check "Required file $([IO.Path]::GetFileName($path))" $present 'present' ([string]$present)
}
if ($failures.Count -ne 0) {
    $failures | ForEach-Object { Write-Output "[FAILED] $_" }
    exit 6
}

$loaded = @{}
foreach ($key in $projects.Keys) { $loaded[$key] = Read-Project $projects[$key].path }

$expectedSources = [ordered]@{
    shipping = @('application\runtime\ShippingMain.cpp')
    core = @('engine\src\utils\Logger.cpp')
    renderer = @(
        'engine\src\core\CommandListPool.cpp',
        'engine\src\core\Device.cpp',
        'engine\src\graphics\SwapChain.cpp'
    )
    runtime = @(
        'engine\src\platform\Window.cpp',
        'engine\src\runtime\RuntimeHost.cpp'
    )
}
$compileItems = @{}
$includeItems = @{}
$references = @{}
foreach ($key in $projects.Keys) {
    $compileItems[$key] = @(Get-Includes $loaded[$key] 'ClCompile')
    $includeItems[$key] = @(Get-Includes $loaded[$key] 'ClInclude')
    $references[$key] = @(Get-Includes $loaded[$key] 'ProjectReference')
}
foreach ($key in $expectedSources.Keys) {
    Add-Check "$($projects[$key].name) exact source allowlist" (
        Test-ExactSet $compileItems[$key] $expectedSources[$key]
    ) ([string]::Join(', ', $expectedSources[$key])) ([string]::Join(', ', $compileItems[$key]))
}
$moduleGuardMacros = [ordered]@{
    core = 'GE3_ENGINE_CORE'
    renderer = 'GE3_ENGINE_RENDERER'
    runtime = 'GE3_ENGINE_RUNTIME'
}
foreach ($key in $moduleGuardMacros.Keys) {
    $unguardedSources = @($expectedSources[$key] | Where-Object {
        (Get-Content -Raw -LiteralPath (Join-Path $repoRoot $_)) -notmatch $moduleGuardMacros[$key]
    })
    Add-Check "$($projects[$key].name) sources enforce ownership macro" ($unguardedSources.Count -eq 0) $moduleGuardMacros[$key] ([string]::Join(', ', $unguardedSources))
}

$moduleSources = @($expectedSources.core) + @($expectedSources.renderer) + @($expectedSources.runtime)
$duplicateOwnership = @($moduleSources | Group-Object | Where-Object Count -ne 1 | ForEach-Object Name)
Add-Check 'Engine module source ownership is unique' ($duplicateOwnership.Count -eq 0) 'none' ([string]::Join(', ', $duplicateOwnership))
$editorDuplicates = @($compileItems.editor | Where-Object { $_ -in $moduleSources })
$shippingDuplicates = @($compileItems.shipping | Where-Object { $_ -in $moduleSources })
Add-Check 'Editor does not compile engine module implementation directly' ($editorDuplicates.Count -eq 0) 'none' ([string]::Join(', ', $editorDuplicates))
Add-Check 'Shipping does not compile engine module implementation directly' ($shippingDuplicates.Count -eq 0) 'none' ([string]::Join(', ', $shippingDuplicates))

$expectedReferences = [ordered]@{
    editor = @('GE3.EngineRuntime.vcxproj', 'externals\DirectXTex\DirectXTex_Desktop_2022_Win10.vcxproj')
    shipping = @('GE3.EngineRuntime.vcxproj')
    runtime = @('GE3.EngineRenderer.vcxproj')
    renderer = @('GE3.EngineCore.vcxproj')
    core = @()
}
foreach ($key in $expectedReferences.Keys) {
    Add-Check "$($projects[$key].name) exact project references" (
        Test-ExactSet $references[$key] $expectedReferences[$key]
    ) ([string]::Join(', ', $expectedReferences[$key])) ([string]::Join(', ', $references[$key]))
}

$referenceEdges = @(
    @{ consumer = 'editor'; dependency = 'runtime' },
    @{ consumer = 'shipping'; dependency = 'runtime' },
    @{ consumer = 'runtime'; dependency = 'renderer' },
    @{ consumer = 'renderer'; dependency = 'core' }
)
foreach ($edge in $referenceEdges) {
    $consumerText = $loaded[$edge.consumer].text
    $dependency = $projects[$edge.dependency]
    $guidPattern = '<Project>\{' + [regex]::Escape($dependency.guid) + '\}</Project>'
    Add-Check "$($projects[$edge.consumer].name) reference GUID for $($dependency.name)" ($consumerText -match $guidPattern) $dependency.guid 'checked'
    Add-Check "$($projects[$edge.consumer].name) links $($dependency.name)" ($consumerText -match '<LinkLibraryDependencies>true</LinkLibraryDependencies>') 'true' 'checked'
    Add-Check "$($projects[$edge.consumer].name) consumes library dependency inputs" ($consumerText -match '<UseLibraryDependencyInputs>true</UseLibraryDependencyInputs>') 'true' 'checked'
}

$moduleProjectItems = @()
foreach ($key in @('core', 'renderer', 'runtime')) {
    $moduleProjectItems += @($compileItems[$key]) + @($includeItems[$key])
}
$forbiddenProjectItems = @($moduleProjectItems | Where-Object { $_ -match '(^|[\\])(application[\\]editor|externals[\\](imgui|assimp|DirectXTex))[\\]' })
Add-Check 'Engine modules exclude Editor and authoring project items' ($forbiddenProjectItems.Count -eq 0) 'none' ([string]::Join(', ', $forbiddenProjectItems))

$forbiddenIncludes = [System.Collections.Generic.List[string]]::new()
foreach ($relativePath in $moduleSources) {
    $path = Join-Path $repoRoot $relativePath
    $matches = Select-String -LiteralPath $path -Pattern '#include\s*[<"][^>"]*(application[/\\]editor|editor[/\\]|imgui|assimp|DirectXTex)' -AllMatches
    foreach ($match in $matches) { $forbiddenIncludes.Add("$relativePath`:$($match.LineNumber)") }
}
Add-Check 'Engine module translation units exclude Editor/authoring headers' ($forbiddenIncludes.Count -eq 0) 'none' ([string]::Join(', ', $forbiddenIncludes))

$publicRuntimeHeaderPath = Join-Path $repoRoot 'engine\include\runtime\RuntimeHost.h'
$publicRuntimeHeader = Get-Content -Raw -LiteralPath $publicRuntimeHeaderPath
$publicApiLeaks = @(Select-String -LiteralPath $publicRuntimeHeaderPath -Pattern '(?i)#include\s*[<"][^>"]*(Windows|d3d|dxgi|wrl|core[/\\]|graphics[/\\]|platform[/\\]|application[/\\]|editor[/\\]|imgui)' -AllMatches)
Add-Check 'Runtime public API hides platform, renderer, and Editor headers' ($publicApiLeaks.Count -eq 0) 'none' ([string]::Join(', ', @($publicApiLeaks | ForEach-Object LineNumber)))
Add-Check 'Runtime public API exposes RuntimeHost' ($publicRuntimeHeader -match 'class RuntimeHost final') 'RuntimeHost' 'checked'
Add-Check 'Runtime public API exposes configuration and result contracts' ($publicRuntimeHeader -match 'struct RuntimeConfig final' -and $publicRuntimeHeader -match 'struct RuntimeResult final') 'RuntimeConfig/RuntimeResult' 'checked'

$shippingMainPath = Join-Path $repoRoot 'application\runtime\ShippingMain.cpp'
$shippingMain = Get-Content -Raw -LiteralPath $shippingMainPath
$shippingBoundaryLeaks = @(Select-String -LiteralPath $shippingMainPath -Pattern '(?i)#include\s*[<"][^>"]*(d3d|dxgi|wrl|core[/\\]|graphics[/\\]|platform[/\\])' -AllMatches)
Add-Check 'Shipping application excludes renderer/platform concrete headers' ($shippingBoundaryLeaks.Count -eq 0) 'none' ([string]::Join(', ', @($shippingBoundaryLeaks | ForEach-Object LineNumber)))
Add-Check 'Shipping application consumes RuntimeHost facade' ($shippingMain -match '#include\s*"runtime/RuntimeHost\.h"' -and $shippingMain -match 'RuntimeHost\{\}\.Run') 'RuntimeHost::Run' 'checked'

$moduleProps = Get-Content -Raw -LiteralPath $modulePropsPath
Add-Check 'Engine modules disable Editor macro' ($moduleProps -match 'GE3_BUILD_EDITOR=0') 'GE3_BUILD_EDITOR=0' 'checked'
Add-Check 'Engine modules disable ImGui macro' ($moduleProps -match 'GE3_ENABLE_IMGUI=0') 'GE3_ENABLE_IMGUI=0' 'checked'
foreach ($key in @('core', 'renderer', 'runtime')) {
    $projectText = $loaded[$key].text
    $propsText = Get-Content -Raw -LiteralPath $projects[$key].props
    $macro = 'GE3_ENGINE_' + $key.ToUpper() + '=1'
    Add-Check "$($projects[$key].name) is a Static Library" ($projectText -match '<ConfigurationType>StaticLibrary</ConfigurationType>') 'StaticLibrary' 'checked'
    Add-Check "$($projects[$key].name) supports four configurations" (
        @('Debug', 'Development', 'Release', 'Shipping').Count -eq @(
            @('Debug', 'Development', 'Release', 'Shipping') | Where-Object { $projectText -match ('Include="' + $_ + '\|x64"') }
        ).Count
    ) 'Debug/Development/Release/Shipping' 'checked'
    Add-Check "$($projects[$key].name) boundary macro" ($propsText -match $macro) $macro 'checked'
    Add-Check "$($projects[$key].name) imports common module policy" ($propsText -match 'GE3.EngineModule.props') 'GE3.EngineModule.props' 'checked'
}

$shippingProps = Get-Content -Raw -LiteralPath $shippingPropsPath
Add-Check 'Shipping macro enabled' ($shippingProps -match 'GE3_TARGET_SHIPPING=1') 'GE3_TARGET_SHIPPING=1' 'checked'
Add-Check 'Shipping Editor/ImGui macros disabled' ($shippingProps -match 'GE3_BUILD_EDITOR=0' -and $shippingProps -match 'GE3_ENABLE_IMGUI=0') 'disabled' 'checked'
Add-Check 'Shipping has unique executable name' ($shippingProps -match '<TargetName>GE3Shipping</TargetName>') 'GE3Shipping' 'checked'
$releaseProps = Get-Content -Raw -LiteralPath $releasePropsPath
Add-Check 'Release remains optimized Editor' ($releaseProps -match '<GE3EditorBuild>true</GE3EditorBuild>' -and $releaseProps -match 'GE3_TARGET_EDITOR=1' -and $releaseProps -match 'GE3_BUILD_EDITOR=1' -and $releaseProps -match 'GE3_ENABLE_IMGUI=1') 'Editor enabled' 'checked'

$solution = Get-Content -Raw -LiteralPath $solutionPath
Add-Check 'Solution exposes Shipping configuration' ($solution -match '(?m)^\s*Shipping\|x64 = Shipping\|x64\s*$') 'Shipping|x64' 'checked'
foreach ($key in @('core', 'renderer', 'runtime')) {
    foreach ($configuration in @('Debug', 'Development', 'Release', 'Shipping')) {
        $mapping = '\{' + [regex]::Escape($projects[$key].guid) + '\}\.' + $configuration + '\|x64\.Build\.0 = ' + $configuration + '\|x64'
        Add-Check "Solution builds $($projects[$key].name) for $configuration" ($solution -match $mapping) "$configuration|x64 Build.0" 'checked'
    }
}
Add-Check 'Shipping builds executable target' ($solution -match '\{9B7AE590-5D9D-4F06-AE0A-920A0B0F56A1\}\.Shipping\|x64\.Build\.0 = Shipping\|x64') 'runtime Build.0 mapping' 'checked'
Add-Check 'Shipping does not build Editor target' ($solution -notmatch '\{DFE964E0-98CC-464F-BBC2-818C767A5A3C\}\.Shipping\|x64\.Build\.0') 'no Editor Build.0 mapping' 'checked'
Add-Check 'Shipping does not build DirectXTex target' ($solution -notmatch '\{371B9FA9-4C90-4AC6-A123-ACED756D6C77\}\.Shipping\|x64\.Build\.0') 'no DirectXTex Build.0 mapping' 'checked'

$forbiddenOutputNames = @('Resources', 'Licenses', 'dxcompiler.dll', 'dxil.dll', 'GE3.exe', 'GE3.EngineCore.lib', 'GE3.EngineRenderer.lib', 'GE3.EngineRuntime.lib')
$leakedOutputs = @()
if (Test-Path -LiteralPath $outputDirectory -PathType Container) {
    $leakedOutputs = @($forbiddenOutputNames | Where-Object { Test-Path -LiteralPath (Join-Path $outputDirectory $_) })
}
Add-Check 'Shipping output excludes Editor/source/static-library payload' ($leakedOutputs.Count -eq 0) 'none' ([string]::Join(', ', $leakedOutputs))

$result = [ordered]@{
    schema = 'ge3.targetSeparation.v3'
    passed = $failures.Count -eq 0
    editorTarget = 'GE3'
    shippingTarget = 'GE3.Runtime'
    runtimeApi = 'ge3.runtimeHost.v1'
    moduleGraph = @(
        'GE3/GE3.Runtime -> GE3.EngineRuntime',
        'GE3.EngineRuntime -> GE3.EngineRenderer',
        'GE3.EngineRenderer -> GE3.EngineCore'
    )
    shippingApplicationCompileUnitCount = $compileItems.shipping.Count
    engineCoreCompileUnitCount = $compileItems.core.Count
    engineRendererCompileUnitCount = $compileItems.renderer.Count
    engineRuntimeCompileUnitCount = $compileItems.runtime.Count
    engineModuleCompileUnitCount = $compileItems.core.Count + $compileItems.renderer.Count + $compileItems.runtime.Count
    checks = $checks
}
$resultJson = $result | ConvertTo-Json -Depth 7
if ($ReportPath) {
    $resolvedReportPath = if ([IO.Path]::IsPathRooted($ReportPath)) { $ReportPath } else { Join-Path $repoRoot $ReportPath }
    $reportDirectory = Split-Path -Parent $resolvedReportPath
    if ($reportDirectory -and -not (Test-Path -LiteralPath $reportDirectory)) { New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null }
    Set-Content -LiteralPath $resolvedReportPath -Value $resultJson -Encoding utf8
}
if ($Json) {
    Write-Output $resultJson
} elseif ($failures.Count -eq 0) {
    Write-Output ("Target separation passed: app {0}, Runtime {1}, Renderer {2}, Core {3} units; facade-only Shipping and one-way module graph." -f $compileItems.shipping.Count, $compileItems.runtime.Count, $compileItems.renderer.Count, $compileItems.core.Count)
} else {
    $failures | ForEach-Object { Write-Output "[FAILED] $_" }
}
if ($failures.Count -ne 0) { exit 6 }
exit 0
