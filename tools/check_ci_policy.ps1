[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$workflowPath = Join-Path $repoRoot '.github\workflows\windows-editor.yml'
$lockPath = Join-Path $repoRoot 'Build\GE3.Dependencies.lock.json'

if (-not (Test-Path -LiteralPath $workflowPath -PathType Leaf)) {
    Write-Error 'Windows editor CI workflow is missing.'
    exit 4
}

$workflow = Get-Content -Raw -LiteralPath $workflowPath
$lock = Get-Content -Raw -LiteralPath $lockPath | ConvertFrom-Json
$failures = [System.Collections.Generic.List[string]]::new()
$usesMatches = [regex]::Matches($workflow, '(?m)^\s*uses:\s*([^\s#]+)')
if ($usesMatches.Count -eq 0) {
    $failures.Add('Workflow contains no action references.')
}
foreach ($match in $usesMatches) {
    $reference = $match.Groups[1].Value
    if ($reference -notmatch '@[0-9a-fA-F]{40}$') {
        $failures.Add("Action reference is not pinned to a full commit SHA: $reference")
    }
}
foreach ($action in $lock.ci.actions) {
    $expectedReference = "$($action.id)@$($action.commit)"
    if ($workflow -notmatch [regex]::Escape($expectedReference)) {
        $failures.Add("Workflow does not use locked action $expectedReference ($($action.version)).")
    }
}
if ($workflow -match '(?m)^\s*runs-on:\s*[^\r\n]*latest') {
    $failures.Add('Workflow uses a floating *-latest runner label.')
}
if ($workflow -notmatch ("(?m)^\s*runs-on:\s*" + [regex]::Escape($lock.ci.runner) + "\s*$")) {
    $failures.Add("Workflow must use the locked $($lock.ci.runner) runner family.")
}
if ($workflow -notmatch 'run_editor_validation\.ps1[^\r\n]*-RequireCleanTree') {
    $failures.Add('Commercial validation is not configured to require a clean tree.')
}
if ($workflow -notmatch 'assert_clean_tree\.ps1') {
    $failures.Add('Workflow does not assert source-tree cleanliness.')
}
if ($workflow -notmatch 'build\.ps1[^\r\n]*-Configuration Shipping[^\r\n]*-Clean') {
    $failures.Add('Workflow does not perform a clean Shipping build.')
}
if ($workflow -notmatch 'run_shipping_validation\.ps1[^\r\n]*-RequireCleanTree') {
    $failures.Add('Workflow does not require clean-tree Shipping validation.')
}
if ($workflow -notmatch 'check_target_separation\.ps1[^\r\n]*-ReportPath') {
    $failures.Add('Workflow does not audit the Editor/EngineRuntime/Shipping dependency graph.')
}
if ($workflow -notmatch ("vs-version:\s*'" + [regex]::Escape($lock.ci.visualStudioVersionRange) + "'")) {
    $failures.Add("Visual Studio selection does not match lock range $($lock.ci.visualStudioVersionRange).")
}

if ($failures.Count -ne 0) {
    $failures | ForEach-Object { Write-Output ("[FAILED] {0}" -f $_) }
    exit 4
}

Write-Output ("CI policy passed: {0} action(s) pinned, fixed runner family, Editor and Shipping clean-tree gates enabled." -f $usesMatches.Count)
exit 0
