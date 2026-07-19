# GE3 Build and Commercial Validation

## M0 contract

M0 fixes the editor build and validation contract so that a local clean build and CI use the same entry points. A successful M0 run requires all of the following:

- Debug, Development, and Release x64 builds succeed from source.
- Visual C++ toolset v145 and Windows SDK 10.0.26100.0 are present.
- Editor Core Regression, Effect Authoring Smoke, Editor Smoke Run, and all Commercial Automation Gates return exit code 0.
- The commercial report is fresh and reports failed, warning, blocked, and attention counts of zero.
- The validation manifest records the commit, dirty state, toolchain, SDK, executable hash, GPU adapter, run exit codes, and report hashes.

The checked-in configuration contract lives in `Build/GE3.Toolchain.props`, `Build/GE3.Common.props`, and the three configuration-specific property files. Build products remain outside the source tree in `../generated/outputs/<Configuration>` and intermediate files in `../generated/objs/<Project>/<Configuration>`.

## Required environment

- Windows x64
- Visual Studio with Desktop development with C++
- MSVC v145 x64/x86 build tools
- Windows SDK 10.0.26100.0, including x64 DXC/DXIL runtime files
- The checked-in Assimp libraries and DirectXTex project
- Windows PowerShell 5.1 or PowerShell 7

`GE3_MSBUILD_PATH` may point to an approved `MSBuild.exe` when automatic Visual Studio discovery is not suitable.

## Commands

Run from the repository root. If the machine blocks local scripts, invoke them through `powershell.exe -NoProfile -ExecutionPolicy Bypass -File`.

```powershell
./tools/check_prerequisites.ps1 -Configuration Development
./tools/build.ps1 -Configuration Debug -Clean
./tools/build.ps1 -Configuration Development -Clean
./tools/run_editor_validation.ps1 -Configuration Development -SkipBuild
./tools/build.ps1 -Configuration Release -Clean
```

For the complete commercial suite only:

```powershell
./tools/run_commercial_gates.ps1 -Configuration Development -SkipBuild
```

Scripts fail closed: a missing prerequisite, non-zero process exit, missing or stale artifact, invalid commercial-ready state, warning, blocked check, or attention check fails the command.

## Outputs

- `logs/editor_automation_report.json`: structured gate result (`editor.commercialCompletion.v22`)
- `logs/editor_automation_report.md`: readable gate report
- `logs/editor_build_manifest.json`: reproducibility and artifact manifest
- `logs/editor_performance_budget_report.log`: measured performance budgets
- Root-level regression and smoke logs

Runtime append-only logs use `application/AppLogFile.h`. The default policy rotates a file at 16 MiB, retains four generations, checks file size at most every 250 ms per path, and prunes log files by age when a log directory exceeds 256 MiB.

## CI

`.github/workflows/windows-editor.yml` executes the same prerequisite checker, three clean configurations, regression, smoke, commercial gates, freshness checks, and artifact upload. No CI-only test command or `BuildProjectReferences=false` shortcut is used.

