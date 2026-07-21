# GE3 Build and Commercial Validation

## M0 contract

M0 fixes the editor build and validation contract so that a local clean build and CI use the same entry points. A successful M0 run requires all of the following:

- Debug, Development, and Release x64 builds succeed from source.
- Visual Studio 2026 (18.x), Visual C++ toolset v145, and Windows SDK 10.0.26100.0 are present.
- Editor Core Regression, Effect Authoring Smoke, Editor Smoke Run, and all Commercial Automation Gates return exit code 0.
- The commercial report is fresh and reports failed, warning, blocked, and attention counts of zero.
- The validation manifest records the commit, dirty state, toolchain, SDK, executable hash, GPU adapter, run exit codes, and report hashes.

The checked-in configuration contract lives in `Build/GE3.Toolchain.props`, `Build/GE3.Common.props`, and the three configuration-specific property files. Build products remain outside the source tree in `../generated/outputs/<Configuration>` and intermediate files in `../generated/objs/<Project>/<Configuration>`.

## M0.1 clean CI closure

M0.1 turns the reproducible-build convention into a fail-closed supply-chain and source-state gate:

- `Build/GE3.Dependencies.lock.json` pins the complete MSBuild/MSVC versions and hashes, toolset targets, DXC/DXIL, Windows SDK notices, vendored source trees, prebuilt Assimp libraries, licenses, CI runner family, and GitHub Action commits.
- `tools/check_prerequisites.ps1` rejects every version, content hash, license, or tracked dependency tree mismatch and can always emit a JSON preflight report with `-ReportPath`.
- `tools/assert_clean_tree.ps1` rejects whitespace errors, modified paths, untracked paths, and a CI checkout whose `HEAD` differs from `GITHUB_SHA`.
- `tools/run_editor_validation.ps1 -RequireCleanTree` checks cleanliness before executing anything, verifies the source revision did not change during validation, and records the result in `ge3.commercialValidation.v3`.
- CI uses the reviewed `windows-2025` runner family, Visual Studio 2026 (18.x), and full 40-character action commit hashes. Exact binary hashes still fail the job if the hosted image is serviced underneath that label.

Dependency/toolchain lock changes are review operations, not automatic updates. Capture the new provenance, verify licenses, perform all three clean builds and commercial validation, review the diff, and only then update the lock.

## Required environment

- Windows x64
- Visual Studio 2026 (18.x) with Desktop development with C++
- MSVC v145 x64/x86 build tools
- Windows SDK 10.0.26100.0, including x64 DXC/DXIL runtime files
- The checked-in Assimp libraries and DirectXTex project
- Windows PowerShell 5.1 or PowerShell 7

`GE3_MSBUILD_PATH` may point to an approved `MSBuild.exe` when automatic Visual Studio discovery is not suitable.

## M1 target and Shipping separation

M1 separates roles at the Visual Studio project boundary. Debug, Development, and Release build the `GE3` Editor target; Release is an optimized Editor and no longer acts as an implicit runtime build. Shipping builds `GE3.Runtime.vcxproj` and its `GE3.EngineRuntime.vcxproj` dependency, with no Editor, ImGui, Assimp, DirectXTex, importer, thumbnail, Document, or Transaction implementation.

M1.1 extracts the implementation shared by Editor and Shipping into the `GE3.EngineRuntime` static library. `GE3` and `GE3.Runtime` link the configuration-matched library through a project reference instead of compiling Runtime implementation files directly. Shipping owns one application entry-point translation unit; `GE3.EngineRuntime` owns an exact five-unit allowlist. Libraries are emitted to `../generated/lib/<Configuration>/GE3.EngineRuntime.lib`, outside every deployable output directory.

M1.2 closes the Runtime module and public-API boundary. `GE3.EngineRuntime` now owns platform/runtime coordination and references `GE3.EngineRenderer`; Renderer owns the D3D12/DXGI implementation and references `GE3.EngineCore`. Editor and Shipping reference only EngineRuntime. Shipping calls the platform-opaque `ge3.runtimeHost.v1` facade instead of Device, SwapChain, Window, command-list, or fence APIs. Exact source ownership, project edges, module macros, transitive library inputs, public-header leakage, and deployed static-library leakage fail CI through `ge3.targetSeparation.v3`.

The Shipping artifact is `../generated/outputs/Shipping/GE3Shipping.exe`. Its output contract contains only the executable, `shipping_target.json`, and Shipping-specific third-party notice. Source `Resources/`, Editor licenses, DXC/DXIL, and `GE3.exe` are forbidden. See `docs/TargetSeparation.md` for the dependency and extension policy.

## Commands

Run from the repository root. If the machine blocks local scripts, invoke them through `powershell.exe -NoProfile -ExecutionPolicy Bypass -File`.

```powershell
./tools/check_prerequisites.ps1 -Configuration Development
./tools/build.ps1 -Configuration Debug -Clean
./tools/build.ps1 -Configuration Development -Clean
./tools/run_editor_validation.ps1 -Configuration Development -SkipBuild -RequireCleanTree
./tools/build.ps1 -Configuration Release -Clean
./tools/build.ps1 -Configuration Shipping -Clean
./tools/run_shipping_validation.ps1 -SkipBuild -RequireCleanTree
./tools/assert_clean_tree.ps1
```

For the complete commercial suite only:

```powershell
./tools/run_commercial_gates.ps1 -Configuration Development -SkipBuild -RequireCleanTree
```

Scripts fail closed: a dirty source tree, source revision change, missing prerequisite, version/hash/license mismatch, non-zero process exit, missing or stale artifact, invalid commercial-ready state, warning, blocked check, or attention check fails the command.

## Outputs

- `logs/editor_automation_report.json`: structured gate result (`editor.commercialCompletion.v22`)
- `logs/editor_automation_report.md`: readable gate report
- `logs/editor_build_manifest.json`: reproducibility and artifact manifest
- `logs/m0_prerequisites.json`: CI preflight checks with expected/actual values and repair actions
- `logs/editor_performance_budget_report.log`: measured performance budgets
- Root-level regression and smoke logs
- `logs/target_separation_report.json`: static project/source/output boundary report
- `logs/shipping_verification.json`: RuntimeHost-backed source-free D3D12 Shipping smoke result (`ge3.shippingVerification.v2`)
- `logs/shipping_build_manifest.json`: Shipping source state, executable and three engine-module hashes, output inventory, and rejection result (`ge3.shippingBuild.v3`)

Every configuration output also contains `THIRD_PARTY_NOTICES.md`, a `Licenses/` directory for Assimp, DirectXTex, Dear ImGui, and the Windows SDK, plus the pinned DXC/DXIL binaries. `tools/build.ps1` verifies their presence and hashes. The validation manifest records each deployed artifact and a deterministic SHA-256 manifest of the complete `Resources/` tree.

Runtime append-only logs use `application/AppLogFile.h`. The default policy rotates a file at 16 MiB, retains four generations, checks file size at most every 250 ms per path, and prunes log files by age when a log directory exceeds 256 MiB.

## CI

`.github/workflows/windows-editor.yml` executes the same prerequisite checker, CI policy audit, clean-tree checks, three clean Editor configurations, a clean Shipping target, regression, smoke, commercial gates, Shipping verification, freshness checks, and artifact upload. No CI-only test command or `BuildProjectReferences=false` shortcut is used. A remote CI run is the final evidence; local validation cannot certify availability of the hosted runner image.
