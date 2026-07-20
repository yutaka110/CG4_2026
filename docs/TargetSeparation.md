# GE3 Editor, Runtime Module, and Shipping Separation

## Target contract

GE3 uses separate Visual Studio projects for authoring and distributable runtime code.
Configuration names do not switch one executable between incompatible roles.

| Solution configuration | Executable target | Purpose | Editor | ImGui |
| --- | --- | --- | ---: | ---: |
| Debug | `GE3` | Diagnostic Editor | yes | yes |
| Development | `GE3` | Daily Editor | yes | yes |
| Release | `GE3` | Optimized production-validation Editor | yes | yes |
| Shipping | `GE3.Runtime` | Distributable runtime shell | no | no |

Both executable targets reference `GE3.EngineRuntime`, which references
`GE3.EngineRenderer`, which references `GE3.EngineCore`. Shipping has no reference to
the Editor target, DirectXTex, or a vendored authoring dependency. The solution has no
Shipping `Build.0` mapping for `GE3` or `DirectXTex`.

## Exact source ownership

Shipping application:

- `application/runtime/ShippingMain.cpp`

EngineRuntime:

- `engine/src/platform/Window.cpp`
- `engine/src/runtime/RuntimeHost.cpp`

EngineRenderer:

- `engine/src/core/CommandListPool.cpp`
- `engine/src/core/Device.cpp`
- `engine/src/graphics/SwapChain.cpp`

EngineCore:

- `engine/src/utils/Logger.cpp`

The allowlists are exact and source ownership is unique. Editor and Shipping compile
none of these engine implementations directly. Every module implementation rejects
compilation without its ownership macro.

## Runtime API boundary

Shipping includes only `runtime/RuntimeHost.h` from the engine. That public header is
platform- and renderer-opaque. `RuntimeHost` owns window, D3D12 device, swap-chain,
command-list, present, and bounded GPU-fence coordination. The Shipping application
owns only CLI rejection, configuration, process exit, and verification-report output.

The Shipping application defines `GE3_TARGET_SHIPPING=1`, `GE3_BUILD_EDITOR=0`, and
`GE3_ENABLE_IMGUI=0`. Every engine module defines `GE3_BUILD_EDITOR=0` and
`GE3_ENABLE_IMGUI=0`; the module-specific ownership macro is added by its property
sheet. The public Runtime contract identifier is `ge3.runtimeHost.v1`.

The Engine Window layer still exposes a generic message callback for the existing
Editor. ImGui message handling is installed by `AppImGuiLayer` in the Editor target, so
no engine module includes or calls Dear ImGui.

## Shipping runtime and output

`--shipping-verify` calls the public RuntimeHost API, presents three hidden frames, and
writes `logs/shipping_verification.json` using `ge3.shippingVerification.v2`. Any
`--editor-*` command is rejected with exit code 64 before RuntimeHost startup.

The Shipping output contract contains exactly:

```text
GE3Shipping.exe
shipping_target.json
THIRD_PARTY_NOTICES.md
```

It must not contain Resources, Licenses, DXC/DXIL, `GE3.exe`, a static engine library,
ImGui, importer, thumbnail, Document, Transaction, or other Editor payload. The current
runtime shell requires no source assets.

M1.2 closes the initial module and public-API boundary. It does not complete the Cook,
DDC, Package, gameplay-runtime migration, signing, or installer milestones. Runtime
features enter Shipping only through an approved public API and exact module allowlist.

## Validation

```powershell
./tools/check_target_separation.ps1
./tools/build.ps1 -Configuration Shipping -Clean
./tools/run_shipping_validation.ps1 -SkipBuild -RequireCleanTree
```

CI audits the dependency graph before building. Editor and Shipping manifests record
all three configuration-matched static libraries and their SHA-256 values.
