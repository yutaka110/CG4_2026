# GE3 Runtime Module and API Boundary

## Purpose

M1.2 replaces the initial mixed EngineRuntime library with a one-way static-module
graph shared by Editor and Shipping:

```text
GE3 Editor -----------+
                      +--> GE3.EngineRuntime --> GE3.EngineRenderer --> GE3.EngineCore
GE3.Runtime ----------+
```

Executable targets reference only `GE3.EngineRuntime`. EngineRuntime coordinates the
platform and render loop, EngineRenderer owns D3D12/DXGI implementation, and EngineCore
owns lower-level services. Reverse and lateral references are fail-closed.

## Module ownership

`GE3.EngineCore` owns:

- `engine/src/utils/Logger.cpp`

`GE3.EngineRenderer` owns:

- `engine/src/core/CommandListPool.cpp`
- `engine/src/core/Device.cpp`
- `engine/src/graphics/SwapChain.cpp`

`GE3.EngineRuntime` owns:

- `engine/src/platform/Window.cpp`
- `engine/src/runtime/RuntimeHost.cpp`

Every implementation file contains a module ownership guard. Compiling it without the
corresponding `GE3_ENGINE_CORE`, `GE3_ENGINE_RENDERER`, or `GE3_ENGINE_RUNTIME` macro
fails at preprocessing time. Neither executable project compiles these files directly.

## Public Runtime API

`engine/include/runtime/RuntimeHost.h` is the Shipping application boundary. It exposes
only versioned configuration, result, failure-stage, and host-run contracts. The public
header does not expose Windows, D3D12, DXGI, WRL, Renderer, Platform, Editor, or ImGui
types. Concrete window, device, swap-chain, command-list, and GPU-fence operations live
inside `GE3.EngineRuntime`.

`ShippingMain.cpp` performs command-line policy, constructs `RuntimeConfig`, calls
`RuntimeHost::Run`, and serializes the verification result. It cannot include Core,
Renderer, or Platform concrete headers. The initial API contract identifier is
`ge3.runtimeHost.v1`.

## Build and ABI policy

All modules support Debug, Development, Release, and Shipping x64 and import the shared
`Build/GE3.EngineModule.props` policy. That policy pins the warning, language, CRT,
optimization, whole-program-optimization, intermediate, and output rules. Engine
modules always compile with Editor and ImGui disabled; Shipping variants additionally
define `GE3_RUNTIME_SHIPPING=1`.

Libraries are written to:

```text
../generated/lib/<Configuration>/GE3.EngineCore.lib
../generated/lib/<Configuration>/GE3.EngineRenderer.lib
../generated/lib/<Configuration>/GE3.EngineRuntime.lib
```

Static libraries are link-time inputs and must never be copied into an Editor or
Shipping deployment directory.

## Dependency and extension rules

- EngineCore references no engine project.
- EngineRenderer references exactly EngineCore.
- EngineRuntime references exactly EngineRenderer.
- Editor and Shipping reference exactly EngineRuntime for runtime services.
- Engine modules may not contain Editor or vendored authoring project items.
- Runtime translation units may not include Editor, ImGui, Assimp, or DirectXTex.
- A new shared source enters exactly one module allowlist and must carry its ownership
  guard.
- A new dependency edge requires an explicit graph-contract and CI review.

Legacy physical paths such as `engine/include/core/Device.h` do not define module
ownership; project ownership and the checked dependency graph are authoritative. A
later header-layout cleanup may move these paths without changing the M1.2 graph.

## Verification

`tools/check_target_separation.ps1` validates exact sources, exact project references,
GUIDs, link-library input propagation, module macros, public-API leakage, Shipping
facade-only includes, solution mappings, and deployed-output leakage. `tools/build.ps1`
requires all three non-empty libraries. Editor and Shipping validation manifests record
the size and SHA-256 of every configuration-matched module.
