[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

& git -C $repoRoot diff --check
if ($LASTEXITCODE -ne 0) {
    [Console]::Error.WriteLine('Git diff whitespace validation failed.')
    exit 3
}

$head = (& git -C $repoRoot rev-parse HEAD 2>$null)
if ($LASTEXITCODE -ne 0 -or -not $head) {
    [Console]::Error.WriteLine('A valid Git HEAD is required.')
    exit 3
}
$head = ([string]$head).Trim()

$changes = @(& git -C $repoRoot status --porcelain=v1 --untracked-files=all 2>$null)
if ($LASTEXITCODE -ne 0) {
    [Console]::Error.WriteLine('Git working tree state could not be read.')
    exit 3
}
if ($changes.Count -ne 0) {
    Write-Output 'Clean-tree gate rejected these paths:'
    $changes | ForEach-Object { Write-Output ("  {0}" -f $_) }
    [Console]::Error.WriteLine(("Working tree contains {0} changed or untracked path(s)." -f $changes.Count))
    exit 3
}

if ($env:GITHUB_ACTIONS -eq 'true' -and $env:GITHUB_SHA -and $head -ne $env:GITHUB_SHA) {
    [Console]::Error.WriteLine(("Checked out HEAD {0} does not match GITHUB_SHA {1}." -f $head, $env:GITHUB_SHA))
    exit 3
}

Write-Output ("Clean-tree gate passed at commit {0}." -f $head)
exit 0
