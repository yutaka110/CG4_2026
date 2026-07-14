# Editor Core Design

This document defines the shared editor foundation for CG4. The goal is to move
from feature-specific ImGui tools toward a coherent editor architecture without
breaking the existing VFX, course authoring, RenderGraph diagnostics, terrain,
post-process, and runtime debug workflows.

The design is intentionally incremental. Existing panels remain usable while
new editor services are introduced behind adapters.

## Goals

- Keep current VFX authoring, live tuning, runtime queue inspection, and
  RenderGraph diagnostics working during the migration.
- Introduce shared editor concepts for selection, commands, transactions,
  properties, assets, validation, and runtime inspection.
- Make future tools cheaper to build by routing them through common editor
  services instead of one-off panel state.
- Allow editor features to preview changes immediately while keeping runtime
  data and authoring data separated.
- Provide a practical path toward Unreal-style workflows: viewport, outliner,
  inspector, content browser, PIE/simulate, validation, profiling, and
  extension points.
- Define measurable completion gates for moving from the current Editor Core
  foundation to a production-ready commercial editor.

## Non-goals

- Do not replace the current ImGui editor windows in one large rewrite.
- Do not rewrite `EffectSystem`, `EffectRuntime`, or VFX renderer inputs as part
  of the initial Editor Core.
- Do not force all assets into a new serialized format immediately.
- Do not build Blueprint, Niagara, Sequencer, or a full material graph before the
  editor foundations are stable.
- Do not make runtime systems depend on editor-only APIs.
- Do not define "Unreal-level" as copying every Unreal Engine subsystem. For
  CG4, it means reaching production-grade workflows, data safety, extensibility,
  and validation for the engine's actual domains.

## Commercial Editor Completion Definition

For this project, "100%" means a CG4-specific commercial editor with Unreal-style
authoring quality. It does not require feature parity with every Unreal Engine
tool, but it does require that daily production work can be done through stable,
discoverable, undoable, validated, and extensible editor workflows.

Completion is measured across ten editor pillars:

| Pillar | 100% completion requirement |
| --- | --- |
| Core services | Selection, transactions, commands, context, dirty state, notifications, validation, and document lifecycle are the default path for all authoring edits. |
| Workspace layout | Main viewport, outliner, details, content browser, diagnostics, runtime inspector, and tool panels are hosted by a persistent dock/layout system. |
| Viewport authoring | Rendering, camera aspect, picking, gizmo coordinates, HUD composition, focus, and input capture all use the editor viewport boundary. |
| Property/details | Course, VFX, terrain, post-process, and renderer settings expose typed descriptors, safe accessors, validation, and transaction-backed edits. |
| Asset/content pipeline | Assets have stable IDs, logical paths, metadata, dependency tracking, missing-reference diagnostics, thumbnails where useful, and safe rename/move flows. |
| Diagnostics/profiling | Domain diagnostics, RenderGraph issues, VFX loader/runtime warnings, validation errors, and profiling views use common tables with source navigation. |
| Play/simulate boundary | Play, simulate, pause, stop, and apply/keep-changes flows operate on isolated runtime state and never silently mutate authoring data. |
| Extensibility | New panels, commands, menus, toolbar actions, asset adapters, validation adapters, and details sections can be registered without central UI rewrites. |
| Persistence/project UX | Open documents, layout, selection, asset browser filters, editor preferences, recent files, and project settings persist across sessions. |
| Quality automation | Build, smoke, editor-service unit tests, asset indexing tests, validation tests, and Feature Guard checks protect all critical workflows. |

Use the following maturity scale when reporting progress:

| Maturity | Completion range | Meaning |
| --- | ---: | --- |
| Prototype | 0-40% | Feature-specific panels exist, but workflows are local and fragile. |
| Foundation | 40-70% | Shared services exist and some production flows are routed through them. |
| Production beta | 70-90% | Most daily authoring tasks use common services, with limited persistence and automation gaps. |
| Commercial ready | 90-100% | Workflows are persistent, extensible, validated, test-covered, and safe for team production. |

## Existing Systems To Preserve

The current project already has several editor-like surfaces:

- VFX Inspector and VFX Diagnostics in `application/AppImGuiLayer.cpp`.
- Effect asset diagnostics and component authoring views in
  `application/AppEffectAssetEditorPanel.cpp`.
- Runtime instance inspection in `application/AppEffectInstancePanel.cpp`.
- Runtime queue and telemetry panels in VFX debug panels.
- RenderGraph debugging in `application/AppRenderGraphDebugPanel.cpp`.
- Course timeline/object authoring in
  `application/AppCourseTimelineDebugPanel.cpp`.
- Course object local undo/redo in `application/AppRunLoop.cpp`.
- Course validation in `application/course/CourseValidation.cpp`.
- Technique, renderer, and simulation registries in `application/EffectSystem.h`
  and related registry files.

Editor Core must treat these as working products, not disposable prototypes.
Each migration step should wrap one of these surfaces, then retire duplicate
local logic only after behavior parity is verified.

## Architecture Overview

Editor Core is a set of application-layer services. Runtime systems can expose
data to the editor through narrow adapters, but renderer, simulation, asset
runtime, and gameplay code should not take dependencies on editor UI code.

```text
ImGui Panels
  -> Editor UI Widgets
  -> Editor Core Services
  -> Domain Adapters
  -> Existing Systems

Existing Systems:
  EffectSystem / EffectRuntime / CourseAsset / Terrain / RenderGraph / RuntimeState
```

The first version can live under `application/editor/` once implementation
starts. That keeps the migration close to current application tools while
leaving `engine/` free of editor-only dependencies.

## Core Services

### EditorContext

`EditorContext` is the frame-level hub passed to editor panels and services.

Responsibilities:

- Own shared editor state that is not domain-specific.
- Provide access to selection, transactions, asset registry, validation results,
  and runtime inspection.
- Carry frame timing and editor mode state.
- Avoid direct ownership of renderer or gameplay runtime data.

Initial shape:

```cpp
struct EditorContext {
    EditorSelection* selection = nullptr;
    EditorTransactionStack* transactions = nullptr;
    EditorAssetRegistry* assets = nullptr;
    EditorValidationRegistry* validation = nullptr;
    EditorRuntimeInspector* runtimeInspector = nullptr;
    EditorCommandRegistry* commands = nullptr;
};
```

### EditorSelection

Selection should become a shared service instead of each panel owning unrelated
selected indices.

Required features:

- Stable selected object handles.
- Multi-selection.
- Domain tags such as `EffectAsset`, `EffectComponent`, `CourseTerrainPlacement`,
  `CourseRockCluster`, `RenderGraphPass`, and `RuntimeEntity`.
- Synchronization between viewport picking, outliner/list selection, and details
  panels.

Initial handle:

```cpp
struct EditorObjectHandle {
    EditorDomainId domain;
    std::string stableId;
    uint64_t localIndex = 0;
    uint32_t generation = 0;
};
```

For legacy data that does not yet have stable IDs, adapters may use local index
handles. Those handles must be treated as temporary and refreshed after reload,
sort, or asset reconstruction.

### EditorCommandRegistry

Commands are named editor actions. They are not necessarily undoable, but they
provide a common way to bind toolbar buttons, menus, shortcuts, and scripts.

Examples:

- `course.save`
- `course.reload`
- `course.apply`
- `course.teleportToSelection`
- `vfx.reloadAssets`
- `vfx.spawnSelectedEffect`
- `rendergraph.copyPassSummary`
- `editor.undo`
- `editor.redo`

Each command should declare:

- Stable command ID.
- Display label.
- Optional shortcut.
- Enabled predicate.
- Execute callback.
- Optional transaction label if it modifies authoring data.

### EditorTransactionStack

All authoring edits should eventually pass through a shared transaction stack.
The existing course object undo/redo is the first migration target because it
already has snapshot-based behavior.

Transaction requirements:

- Begin/end named transaction scopes.
- Record before/after changes.
- Support snapshot transactions for coarse legacy assets.
- Support property-level delta transactions for reflected objects later.
- Clear redo on new edit.
- Limit memory usage.
- Mark modified assets dirty.

Initial API:

```cpp
class EditorTransactionStack {
public:
    void Begin(std::string label);
    void Commit();
    void Cancel();

    void PushSnapshot(
        std::string label,
        EditorObjectHandle target,
        EditorSnapshot before,
        EditorSnapshot after);

    bool CanUndo() const;
    bool CanRedo() const;
    void Undo(EditorApplyContext& context);
    void Redo(EditorApplyContext& context);
};
```

Migration rule:

- Phase 1 may keep snapshot transactions for `CourseAsset` object edits.
- Phase 2 should use property deltas for simple scalar/vector edits.
- Phase 3 should cover VFX component live tuning and post-process/terrain
  presets.

### EditorPropertyRegistry

The property registry is the foundation for generic details panels,
serialization, validation, transactions, and future node exposure.

This project should start with manual descriptors instead of a full C++ macro
reflection system. Macros can come later after the descriptor shape proves
itself.

Descriptor shape:

```cpp
enum class EditorPropertyKind {
    Bool,
    Int,
    UInt,
    Float,
    String,
    Vec2,
    Vec3,
    Vec4,
    Color,
    Enum,
    AssetRef,
    ObjectRef,
};

struct EditorPropertyDescriptor {
    std::string name;
    std::string displayName;
    EditorPropertyKind kind;
    std::string category;
    float minValue = 0.0f;
    float maxValue = 0.0f;
    bool hasRange = false;
    bool readOnly = false;
    bool runtimeOnly = false;
};
```

The first useful targets are:

- `EffectComponentCommon`
- `EffectParticleSettings`
- `EffectTrailSettings`
- `EffectBeamSettings`
- `EffectDistortionSettings`
- `CourseTerrainPlacement`
- `CourseRockCluster`
- `CourseCameraKey`
- Post-process preset parameters
- Terrain authoring settings

### EditorDetailsPanel

The details panel should be generated from property descriptors where possible,
while still allowing custom sections for complex domain data.

Rules:

- Generic fields handle scalar/vector/color/enum/string editing.
- Domain adapters can add custom controls after generic properties.
- Every edit path must be able to create a transaction.
- Multiple selection should show shared editable properties when safe.
- Runtime-only values must be visually separated from authoring values.

Existing hand-written panels should not be removed immediately. Instead, use
generic property sections inside the existing panels first.

### EditorAssetRegistry

The asset registry is the long-term replacement for scattered file scans and
direct path references.

Initial registry data:

```cpp
struct EditorAssetRecord {
    std::string guid;
    std::string logicalPath;
    std::string sourcePath;
    std::string type;
    std::string displayName;
    std::vector<std::string> tags;
    std::vector<std::string> dependencies;
    bool dirty = false;
    bool missing = false;
};
```

Initial sources:

- `Resources/effects/*.effect`
- `Resources/courses/*.course`
- `Resources/courses/actors/*.actor`
- `Resources/courses/waves/*.wave`
- `Resources/courses/bullet_patterns/*.pattern`
- `Resources/courses/obstacles/*.obstacle`
- Terrain and post-process preset files
- Textures, models, shaders, and audio files as non-editable records

Important rule:

Asset Registry should begin as an index over existing files. It should not force
a new storage format or GUID migration in its first version.

GUID migration can be staged:

1. Build records from path and type.
2. Add sidecar `.meta` files for new or touched assets.
3. Resolve references by GUID when present, then path fallback.
4. Add validation warnings for path-only references.
5. Move rename/move safety onto GUID references.

### EditorValidationRegistry

Validation should collect diagnostics from domain validators and present them in
a unified way.

Existing validators:

- Course validation.
- Effect asset loader diagnostics.
- Technique/renderer/simulation registry diagnostics.
- RenderGraph validation/errors.

Unified diagnostic:

```cpp
enum class EditorDiagnosticSeverity {
    Info,
    Warning,
    Error,
};

struct EditorDiagnostic {
    EditorDiagnosticSeverity severity;
    std::string domain;
    std::string code;
    std::string subject;
    std::string message;
    std::string assetGuid;
    std::string sourcePath;
    int line = 0;
};
```

The unified registry should not replace domain-specific diagnostics at first.
Panels can display both during migration until the common diagnostic table has
parity.

### EditorRuntimeInspector

Runtime inspection must stay read-only by default. Editing runtime state should
be explicit and should not silently mutate authoring data.

Responsibilities:

- Expose selected runtime instance values.
- Watch VFX instance age, component activity, queue routing, and fallback
  reasons.
- Expose course runtime actors, obstacles, bullets, and camera/director state.
- Expose RenderGraph pass state and resource aliases.
- Support "copy runtime value to authoring" only through explicit commands.

This preserves the existing distinction between authoring assets and runtime
frames in the VFX architecture.

## Domain Adapters

Domain adapters connect existing systems to Editor Core without forcing those
systems to depend on editor UI.

### VFX Adapter

The VFX adapter is read/write for authoring assets and read-only for runtime
frames unless an explicit command applies runtime data back to authoring data.

Must preserve:

- `EffectAsset` typed component storage and compatibility storage boundaries.
- `EffectRuntimeFrame` typed queue inputs.
- Renderer-facing inputs that do not expose full `EffectComponentAsset`.
- Current diagnostics from `EffectAssetLoader`.
- Current live telemetry and fallback reason reporting.

Initial adapter responsibilities:

- Register asset records for loaded `.effect` files.
- Provide object handles for effect assets and components.
- Expose property descriptors for component common fields and typed settings.
- Route component edits through existing typed replace helpers.
- Emit validation diagnostics from loader and registry checks.
- Keep runtime queue panels reading the existing runtime/debug structures.

VFX edit rule:

Do not mutate packed compatibility component storage from generic editor code.
Generic editor edits must copy the typed component, modify it, and commit
through the matching typed replace/sync API.

### Course Adapter

The course adapter is the first target for shared transactions because current
course object editing already uses snapshots.

Initial adapter responsibilities:

- Register `.course` assets and related actor/wave/pattern/obstacle assets.
- Expose course sections, camera keys, events, terrain placements, and rock
  clusters as selectable handles.
- Convert existing course object snapshots into `EditorTransactionStack`
  transactions.
- Reuse `ValidateCourseAsset` and convert issues to common diagnostics.
- Keep existing Save, Reload, Apply, and Teleport callbacks.

Migration constraint:

Do not remove the current course authoring panel until shared selection,
details, and transaction behavior can reproduce current add/remove/edit/save
flows.

### RenderGraph Adapter

The RenderGraph adapter is read-only in the editor core.

Responsibilities:

- Register RenderGraph passes as runtime-inspector objects.
- Expose pass inputs, outputs, aliases, transient target counts, and validation
  errors.
- Keep the existing RenderGraph panel as the source of detailed display until a
  common diagnostics view is ready.

### Terrain And Post-process Adapters

These can start as command and property adapters.

Responsibilities:

- Register preset assets.
- Expose editable settings through descriptors.
- Route save requests through existing stores.
- Add transactions for changes that currently mutate state directly.

## Editor UI Layout

The first common layout should be practical and compatible with current panels:

- Main Viewport: existing rendered scene.
- Inspector window: selected object details and runtime watch.
- Content/Assets tab: registry records and validation state.
- Diagnostics tab: common diagnostics plus domain-specific detail links.
- Existing VFX Diagnostics tabs: kept intact while adapters mature.
- Existing Course Timeline tab: kept intact while shared selection/details are
  introduced.

The commercial target is a persistent workspace layout:

- Center: `EditorViewportPanel`, with the game/editor render target, transform
  gizmo, viewport overlays, and PIE/SIM focus controls.
- Left: outliner, scene hierarchy, course object tree, and optional runtime
  object tree.
- Right: details, selected asset properties, runtime watch, and validation
  details.
- Bottom: diagnostics, output log, profiling, RenderGraph, VFX telemetry, and
  timeline tools.
- Content area: asset browser, thumbnails, filters, dependency view, and
  reference repair tools.
- Top chrome: menu bar, document tabs, toolbar, command palette entry points,
  and status bar.

Fixed ImGui windows are acceptable only during foundation validation. Once a
panel is stable, it should move behind `EditorPanelRegistry` and be hosted by
`EditorPanelHost`. Commercial readiness requires layout persistence, predictable
panel IDs, tab restoration, and safe defaults for missing or renamed panels.

## Data Safety Rules

- Runtime data and authoring data are separate.
- Play/simulate must operate on a copied or isolated runtime scene/state.
- Generic editor code must use adapters, not reach into renderer/runtime private
  data.
- A transaction should be created before mutating authoring data.
- Reload must refresh temporary handles and clear invalid selection.
- Save must run validation and report errors before claiming success.
- Asset references should move toward GUID + logical path, but path fallback is
  required during migration.
- Existing VFX runtime and renderer input contracts must not change as a side
  effect of Editor Core work.

## Implementation Phases

### Phase 0: Inventory And Adapter Boundaries

Deliverables:

- Create `application/editor/` skeleton.
- Define `EditorObjectHandle`, `EditorContext`, and domain IDs.
- Add VFX, Course, RenderGraph adapter interfaces without changing panel
  behavior.
- Add a small debug panel showing selected handles and registered domains.

Success criteria:

- Existing VFX and Course panels compile and behave unchanged.
- Editor Core can list domain adapters, but owns no critical behavior yet.

### Phase 1: Shared Selection And Course Transaction Bridge

Deliverables:

- Add `EditorSelection`.
- Bridge course object selection to shared selection.
- Move course object undo/redo snapshot handling behind
  `EditorTransactionStack`, keeping current UI buttons.
- Keep local fallback code until parity is verified.

Success criteria:

- Course object add/edit/remove/undo/redo/save behavior remains unchanged.
- Selection can be observed by a generic inspector/debug panel.

### Phase 2: Property Descriptors And Generic Details Sections

Deliverables:

- Add manual descriptors for course object types and VFX typed settings.
- Add generic ImGui property drawing for bool/int/float/string/vector/color/enum.
- Embed generic details sections inside existing Course and VFX panels.
- Route generic edits through domain adapters and transactions.

Success criteria:

- At least one Course type and one VFX component type can be edited through the
  generic property path.
- Existing specialized controls still work.

### Phase 3: Asset Registry Lite And Unified Diagnostics

Deliverables:

- Build asset records from existing resource folders.
- Add common diagnostics table.
- Convert Course validation and Effect loader diagnostics into common
  diagnostics.
- Show asset dirty/missing/validation state.

Success criteria:

- Current validation information is still available in existing panels.
- The common diagnostics view can jump to or select the relevant asset/object
  where adapters support it.

### Phase 4: Runtime Inspector And PIE/SIM Boundary

Deliverables:

- Add read-only runtime object watch.
- Define explicit authoring/runtime copy commands.
- Document and enforce copied runtime state for play/simulate workflows.
- Add pause/step hooks if runtime loop structure allows it.

Success criteria:

- Runtime inspection does not mutate authoring data.
- Any "keep simulation changes" behavior is explicit and transaction-backed.

### Phase 5: Content Browser And Extension API

Deliverables:

- Add asset browser over `EditorAssetRegistry`.
- Add command/menu registration for tools.
- Add first custom tool registration path for VFX or Course.
- Prepare GUID sidecar migration.

Success criteria:

- New editor tools can add commands and panels without editing the central ImGui
  layer directly.
- Asset records can be searched and filtered by type, path, tag, and diagnostic
  severity.

### Phase 6: Persistent Workspace And Docking

Deliverables:

- Expand `EditorPanelHostArea` beyond diagnostics into viewport, left sidebar,
  right inspector, bottom tray, content browser, and floating modal areas.
- Move stable panels behind `EditorPanelRegistry` with persistent panel IDs.
- Add layout serialization for panel visibility, selected tabs, split ratios,
  browser filters, and last active document.
- Provide default layouts for authoring, VFX debugging, runtime profiling, and
  minimal playtest mode.
- Keep legacy direct ImGui windows available behind Feature Guard until hosted
  panel parity is verified.

Success criteria:

- Restarting the editor restores the same workspace.
- Rail lock-on, VFX diagnostics, details, asset browser, viewport, and runtime
  inspector can coexist without overlapping the central viewport.
- Missing panels or renamed panel IDs fall back to a safe default layout instead
  of corrupting the editor session.

### Phase 7: Production Transform And Viewport Tools

Deliverables:

- Turn `EditorTransformGizmoService` from state reporting into transaction-backed
  manipulation for translate, rotate, and scale.
- Add world/local axis modes, snap settings, pivot mode, numeric transform input,
  and cancel/commit behavior.
- Route viewport picking through `EditorViewportPickResult` and
  `EditorSelectionRequest` for all selectable authoring domains.
- Keep camera aspect, picking rays, gizmo drawing, and HUD overlays based on
  `EditorViewportRenderTarget`.
- Draw editor-visible gameplay debug overlays through `EditorViewportOverlay`
  so render-target coordinates are converted to `viewportRect` display space and
  clipped before reaching the editor chrome.
- Add viewport focus mode, input capture, and read-only play/simulate boundaries
  for all authoring tools.

Success criteria:

- A selected course object can be moved, rotated, scaled, undone, redone, saved,
  reloaded, and validated through the shared editor path.
- Mouse position, reticle, picking ray, gizmo handle, and HUD overlay remain
  aligned after resizing the editor window.
- Play/simulate locks authoring mutation unless the user explicitly applies
  runtime changes through a transaction-backed command.

### Phase 8: Asset Identity, Metadata, And Dependencies

Deliverables:

- Extend `EditorAssetRecord` with GUID, logical path, tags, dependencies,
  dirty/missing flags, source timestamps, and optional thumbnail handles.
- Add sidecar `.meta` files for new or touched assets while preserving path
  fallback for legacy assets.
- Add dependency scanning for course references, VFX references, meshes,
  textures, audio, shaders, and preset files.
- Add safe rename/move/delete validation and reference repair commands.
- Add asset browser search, filters, type grouping, dependency view, and missing
  reference diagnostics.

Success criteria:

- Assets can be selected, inspected, renamed or moved safely when GUID metadata
  exists.
- Path-only references continue to work, but validation reports migration
  warnings.
- A missing mesh, effect, texture, course dependency, or pattern file appears in
  the common diagnostics table with a selectable subject.

Current staged implementation:

- Phase 8-A adds GUID/logical path/sidecar metadata fields to
  `EditorAssetRecord`, reads simple key-value `.meta` files, and keeps
  deterministic provisional GUID fallback for legacy assets.
- Phase 8-B adds an `asset.createMeta` command for the selected asset,
  lightweight text dependency scanning after folder indexing, dependency counts
  in the Asset Browser and Feature Guard, and an
  `asset.repairMissingReference` command that repairs the selected object's
  first unresolved AssetRef through `EditorPropertyAccessor` and
  `EditorTransactionStack`.
- Phase 8-C adds missing asset and dependency diagnostics to the shared
  Diagnostics table via `EditorAssetReferenceDiagnosticsAdapter`, and lets the
  Diagnostics selected-subject filter include the selected Asset as well as the
  selected editor object.
- Phase 8-C also adds the first rename/move/delete safety preflight:
  `EditorAssetMutationSafety` evaluates durable `.meta` GUID readiness, missing
  source files, runtime-only records, and indexed dependents. The Asset Browser
  shows selected asset safety status, and command palette entries
  `asset.renameSafety`, `asset.moveSafety`, and `asset.deleteSafety` expose the
  same checks. Delete uses modal confirmation but does not physically delete
  files until the later asset mutation executor phase.
- Phase 8-D adds `EditorAssetMutationExecutor` for guarded rename/move. It
  executes only after safety preflight passes, moves the asset file and sidecar
  `.meta`, rewrites metadata with the preserved GUID, updates
  `EditorAssetRegistry`, and refreshes Asset selection. Delete remains
  preflight/confirmation-only until reference rewrite and undoable file
  operations are added.
- Phase 8-E promotes asset mutation to formal transactions. Rename, move, and
  delete operations record before/after asset state, participate in undo/redo,
  and keep Asset Browser commands on the shared transaction path.
- Phase 8-F formalizes the dependency graph and Reference Inspector. The
  registry can navigate direct dependencies, dependents, missing dependency
  tokens, and malformed dependency tokens, and the Asset Browser exposes this
  graph for selected assets.
- Phase 8-G adds `EditorAssetThumbnailService` and thumbnail diagnostics. Asset
  records carry thumbnail cache keys/source stamps, the Asset Browser can show
  preview state, and smoke/regression gates protect cache behavior.
- Phase 8-H formalizes migration and handle stability. Asset handles can be
  resolved/refreshed after registry mutation, legacy path-fallback assets emit
  migration warnings until durable `.meta` files exist, and automated pipeline
  tests cover migration, rename, move, delete, reference rewrites, thumbnails,
  and transaction undo/redo.
- Phase 8-I adds `EditorAssetImportService` as the formal import/reimport and
  batch metadata migration path. Folder indexing, command palette reimport,
  and bulk `.meta` creation share the same asset kind/id rules, preserve stable
  GUIDs during reimport, rescan dependencies, refresh thumbnails, and include
  regression/smoke gates for failed reimport identity safety.
- Phase 8-J adds external-file import policy and production Asset Browser UI.
  `ImportExternal` copies supported files into a safe `Resources/` destination,
  applies rename/skip/overwrite collision policy, rejects unsupported assets
  before copy, rolls back partial files on failed registration, and routes the
  result through the same metadata, dependency, thumbnail, selection, and
  notification path as normal import/reimport.
- Phase 8-K adds `EditorAssetPreviewProvider` as the formal rich preview
  metadata generator behind `EditorAssetThumbnailService`. Texture previews
  can read lightweight image dimensions, mesh/course/effect/audio previews
  expose source metadata, Asset Browser preview rows/tooltips show the richer
  summary, and import/reimport/external import regression gates verify that
  preview metadata refreshes through the formal asset pipeline.
- Phase 8-L adds production import entry points. Asset Browser exposes a
  native file picker, dropped OS files are queued into the same panel import
  path, and `EditorAssetImportService::ImportExternalBatch` provides the
  formal multi-file import path with collision policy, unsupported-file skips,
  selection refresh, notification summaries, dependency rescans, and thumbnail
  refresh through the existing asset pipeline.
- Phase 8-M adds `EditorAssetPreviewJobQueue` as the formal budgeted preview
  work queue. `EditorAssetThumbnailService::Sync` now queues preview work,
  `ProcessPreviewJobs` advances jobs within a per-frame budget, Asset Browser
  exposes queued/running/ready/failed counts and retry for failed previews, and
  import/reimport/batch external import tests verify that preview refreshes
  flow through the job path without stale source stamps overwriting newer
  thumbnail state.
- Phase 8-N adds `EditorAssetGpuThumbnailRenderer` as the formal GPU-backed
  thumbnail rendering queue. Ready preview metadata now feeds budgeted GPU
  thumbnail requests, thumbnail entries expose GPU status/render revision/
  resident handle tokens, Asset Browser shows GPU queue health and retry, and
  regression/smoke gates verify import-driven thumbnails become GPU resident.
- Phase 8-O replaces placeholder GPU thumbnail tokens with a formal SRV
  allocation path. `EditorAssetGpuThumbnailRenderer` now accepts a backend,
  `EditorAssetD3D12ThumbnailGpuBackend` allocates shader-visible SRV
  descriptors for the editor thumbnail range, thumbnail entries carry
  `displayTextureId`/descriptor metadata, Asset Browser draws ready GPU
  thumbnails through `ImGui::ImageButton`, and regression coverage verifies
  SRV allocation plus stale descriptor release on source timestamp changes.
- Phase 8-P adds texture thumbnail pixel decode and GPU upload. Texture
  thumbnail requests now carry source paths, `EditorAssetThumbnailTextureLoader`
  decodes DDS/WIC/TGA sources through DirectXTex, normalizes thumbnails to
  RGBA8, `EditorAssetD3D12ThumbnailGpuBackend` records upload copies into the
  frame command list, retains upload resources, creates SRVs against real
  default-heap textures, and regression coverage verifies decode/resize plus
  existing stale descriptor release behavior.
- Phase 8-Q adds mesh/material preview render payloads and production fallback
  icon atlas pixels to the same GPU thumbnail path. Mesh thumbnails now carry
  vertex/face metadata into `EditorAssetMeshThumbnailPreviewRenderer`,
  non-texture or decode-failed previews route through
  `EditorAssetFallbackIconAtlas`, and the D3D12 thumbnail backend uploads those
  generated RGBA8 payloads into real SRV-backed thumbnail textures instead of
  relying on null descriptors.
- Phase 8-R adds thumbnail lifetime telemetry and persistent preview cache
  policy. `EditorAssetThumbnailCacheStore` defines stable preview cache keys,
  memory limits, optional disk persistence, LRU eviction, and fallback storage
  policy; `EditorAssetD3D12ThumbnailGpuBackend` uses it before regenerating
  RGBA8 payloads, reports allocation/release/resident/upload/cache/fallback
  counters, bounds retained upload resources by policy, and Asset Browser
  surfaces lifetime/cache telemetry next to preview and GPU queue state.
- Phase 8-S adds fence-aware thumbnail upload retirement and the first formal
  engine-scene material preview pass boundary. Thumbnail upload buffers now
  enter `EditorThumbnailUploadRetirementQueue` with the frame fence value that
  will make the copy safe to release, the D3D12 backend retires uploads only
  after the completed fence reaches that value, telemetry exposes pending and
  retired upload bytes/counts, and mesh/material thumbnail payload generation
  routes through `EditorAssetPreviewSceneRenderer` before GPU upload.
- Phase 8-T replaces deterministic preview-scene publication with a real
  offscreen D3D12 render target path for mesh/material thumbnails.
  `EditorAssetPreviewRenderTargetPool` owns preview RTV resources, the D3D12
  thumbnail backend clears/renders the offscreen target, transitions it to a
  shader resource, publishes its SRV directly to Asset Browser, and records
  preview-scene direct/fallback/reuse/resize telemetry while retaining the
  cached RGBA upload path as the fallback.
- Phase 8-U binds real mesh/material preview scene metadata into the thumbnail
  path. OBJ previews now extract source bounds, face/vertex counts, material
  slots, camera distance, and light preset data on the preview worker path;
  thumbnail entries and GPU allocation requests preserve that metadata;
  offscreen D3D12 preview targets draw a material-aware proxy scene before SRV
  publication; cache keys include material/bounds inputs; and regression/smoke
  gates verify OBJ geometry/material binding.
- Phase 8-V replaces the proxy-only direct preview target with a renderer-backed
  thumbnail mesh pass. `EditorAssetD3D12ThumbnailGpuBackend` now owns a
  dedicated preview root signature/PSO, compiles thumbnail VS/PS resources,
  extracts capped OBJ triangles into preview vertex/index buffers, binds a
  material/light/camera constant buffer, draws into the offscreen preview render
  target, publishes the target SRV directly, and retains VB/IB/CB upload
  resources through the fence-aware retirement queue. Unsupported mesh formats
  still fall back to a procedural renderer-backed proxy, and shader/data failure
  falls back to the prior clear-rect proxy with telemetry.
- Phase 8-W binds the thumbnail preview path to production mesh/material inputs.
  `EditorAssetPreviewProvider` now uses Assimp-based mesh inspection for OBJ,
  FBX, GLTF/GLB, and engine mesh candidates before falling back to legacy
  lightweight scans, and propagates material texture count/timestamp into
  thumbnail entries, GPU requests, cache keys, and generation diffing. The
  D3D12 thumbnail backend now tries an Assimp-backed preview payload before the
  legacy OBJ extractor, colors preview triangles from material diffuse data,
  samples diffuse/base-color textures into preview material colors when they
  decode successfully, falls back explicitly when texture sampling fails, and
  exposes loader/texture binding telemetry in Asset Browser.
- Phase 8-X replaces CPU-side material texture color sampling with a real GPU
  preview material texture binding. The D3D12 thumbnail backend now extracts a
  primary diffuse/base-color texture path from production Assimp materials,
  uploads the decoded texture into a fence-aware preview texture resource,
  allocates a dedicated material SRV descriptor, passes UVs and texture weights
  through the renderer-backed mesh vertex stream, and samples the material
  texture in the thumbnail preview pixel shader. Missing/unsupported textures
  fall back to material vertex color with explicit SRV fallback telemetry, while
  Asset Browser reports material texture SRV bound/fallback counts and live
  descriptor usage.
- Phase 8-Y upgrades the preview material path from a single primary texture to
  slot-aware multi-material binding. The thumbnail backend now allocates
  contiguous material SRV descriptor tables, binds up to four diffuse/base-color
  textures per preview mesh, carries texture indices, UVs, roughness, and
  metallic values in the preview vertex stream, disables sampling per slot when
  decode/upload fails, and shades thumbnails with a lightweight PBR-style
  diffuse/specular/rim evaluation. Asset Browser telemetry now reports material
  texture table usage, PBR preview counts, live descriptor usage, and explicit
  partial fallback cases.
- Phase 8-Z reuses more of the production material/PBR preview path. The
  thumbnail backend now builds a role-major material texture table for albedo,
  normal, roughness, and metallic maps across the first four material slots,
  extracts those maps from Assimp material metadata, gates shader sampling by
  successfully bound descriptors, and feeds normal/roughness/metallic weights
  through the preview vertex stream. Material texture decoding now goes through
  a preview material pixel cache so repeated thumbnails reuse decoded source
  pixels before GPU upload. Asset Browser telemetry reports PBR normal,
  roughness, metallic map usage and material decode cache hit/miss counts.
- Remaining Phase 8 risk is focused on deeper renderer parity, especially
  shared environment lighting, production BRDF/material graph evaluation, and
  GPU material resource reuse beyond decode-level cache reuse.

### Phase 9: Full Details/Reflection Coverage

Deliverables:

- Register property descriptors for all production authoring targets: course
  objects, VFX typed settings, terrain, post-process, render presets, camera
  keys, events, and gameplay tuning data.
- Support multi-selection intersection editing, per-property read-only reasons,
  reset-to-default, copy/paste property values, and validation hints.
- Route every generic details edit through `EditorPropertyAccessor` and
  `EditorTransactionStack`.
- Allow domain adapters to provide custom detail sections without bypassing
  transaction, validation, dirty, or notification services.

Success criteria:

- Details editing can replace most hand-written scalar/vector/enum controls
  without behavior loss.
- Invalid edits are blocked or reported before save/apply claims success.
- Runtime-only fields are visibly read-only and cannot mutate authoring data by
  accident.

Current staged implementation:

- Phase 9-A expands property descriptor coverage beyond course objects. The
  editor now has a formal `RegisterBuiltInEditorProperties()` entry point that
  registers descriptor groups for course objects, VFX asset/runtime state,
  terrain generation/material presets, post-process passes, render presets,
  camera keys/rigs, RenderGraph runtime passes, and gameplay tuning data.
  `EditorPropertyDescriptor` now carries reset/default metadata, validation
  hints, multi-edit readiness, and explicit read-only/runtime-only reasons so
  Details and registry tooling can distinguish descriptor coverage from
  mutation support. The app-level registry initialization uses the full editor
  registration path, while the existing Course accessor remains the only
  mutable adapter until the next Phase 9 steps formalize domain adapters.
- Phase 9-B formalizes the first multi-selection Details foundation. The
  Details panel now derives an intersection of common same-domain properties,
  shows mixed values explicitly, and routes multi-object edits through
  `EditorPropertyEditSession` into `EditorPropertyEditService::ApplyBatch()`
  so undo/redo and dirty state stay on the official mutation path. Descriptors
  with `readOnlyReason` now surface that reason through service/session
  rejection messages, and reset-to-default is available for descriptors with
  parseable `defaultValue` metadata, including mutable Course transform fields.
- Phase 9-C formalizes the first production property accessor adapters beyond
  Course object editing. `EditorCompositePropertyAccessor` now lets Details,
  commands, undo/redo, and sessions share one mutation gateway, while
  `EditorProductionPropertyAdapter` binds descriptors for VFX assets,
  post-process passes, course camera keys, terrain generation, camera rig, and
  gameplay tuning to live production data. The Selection panel exposes these
  production targets for Details inspection/editing, read-only fields such as
  derived VFX technique and post-process stage are rejected with explicit
  ownership reasons, and regression coverage verifies that non-Course edits use
  generic property dirty state while Course camera authoring remains on the
  Course dirty path.
- Phase 9-D formalizes typed property clipboard and validation enforcement for
  Details editing. `EditorPropertyClipboardService` stores a formatted,
  descriptor-aware property value so Details can copy from the active accessor
  and paste only into compatible descriptors, including multi-selection paste
  through the existing `EditorPropertyEditSession` and
  `EditorPropertyEditService` mutation path. `ValidateEditorPropertyValue()`
  now rejects descriptor/domain mismatches, read-only values, invalid enum
  options, and enforced numeric/vector ranges before any accessor mutation,
  while Details surfaces paste/validation failures through the existing tooltip
  and notification UX.
- Phase 9-E formalizes custom Details sections as registered providers instead
  of ad-hoc panel code. `EditorDetailsSectionProviderRegistry` lets domain
  adapters publish section UI for production-only workflows while still using
  the active `EditorPropertyAccessor`, `EditorPropertyEditService`, transaction
  stack, dirty state, validation, and notifications. Built-in providers now
  cover production VFX defaults, post-process tuning, Course event dispatch
  inspection, and Render preset read-only authoring status. Course event marker
  descriptors and the production adapter are also bound so Details can inspect
  and edit event distance/type/id/payload through the official mutation path.

### Phase 10: Play-In-Editor And Runtime Isolation

Deliverables:

- Define a clear authoring world and runtime world boundary for play, simulate,
  pause, step, stop, and reset.
- Snapshot or clone all mutable authoring state needed by runtime sessions.
- Add explicit "apply runtime change to authoring" commands with confirmations,
  transactions, dirty state updates, and validation.
- Add runtime inspector watch rows for selected actors, course director state,
  VFX instances, RenderGraph resources, and gameplay systems.

Success criteria:

- Stopping play restores authoring state unless an explicit apply command was
  accepted.
- Runtime inspection remains read-only by default.
- Dirty state, document lifecycle, and save/apply policy reflect play/simulate
  state consistently.

Current staged implementation:

- Phase 10-A formalizes Play/Simulate lifecycle ownership with
  `EditorPlaySessionLifecycleService`. Begin Play/Simulate now captures the
  authoring snapshot, enters the requested mode, binds the snapshot to the
  session serial, and marks runtime isolation active in one service call. Stop
  now requires a captured snapshot from the active session and restores
  authoring state before returning to Stopped. `EditorBuiltinCommandProvider`
  no longer falls back to direct `EditorPlaySessionState::Play()`,
  `Simulate()`, or `Stop()` calls, so command entry points must use the
  official lifecycle service path.
- Phase 10-B adds an explicit runtime-to-authoring apply path. The
  `editor.applyRuntimeChanges` command requests confirmation before calling
  `EditorRuntimeAuthoringApplyService`, which compares the active Play/Sim
  snapshot with the current runtime authoring state, rejects validation-blocked
  applies, pushes a `RuntimeAuthoringApply` transaction, marks Course authoring
  dirty, and adopts the applied state as the new restore snapshot. Stop keeps
  only explicitly applied runtime changes; later runtime changes are still
  discarded by snapshot restore. The transaction payload can be undone/redone
  after returning to authoring mode.
- Phase 10-C formalizes runtime pause, resume, single-step, and reset through
  `EditorPlaySessionRuntimeControlService`. Toolbar/command entry points now
  route `editor.pauseRuntime`, `editor.resumeRuntime`, `editor.stepRuntime`,
  and `editor.resetRuntime` through the service. `EditorPlaySessionState`
  tracks runtime pause/step/reset state and runtime frame count separately from
  session lifetime, `AppRunLoop` gates Rail/VFX runtime delta advancement
  through that state, Runtime Watch exposes the control state read-only, and
  reset restores the active Play/Sim snapshot without stopping the session.
- Phase 10-D closes the Runtime Watch coverage path with
  `EditorRuntimeWatchBuilder`. Watch row generation moved out of
  `AppImGuiLayer` into a read-only builder that emits stable records for
  Play/Sim state, runtime pause/step/reset state, editor selection, Course
  director/checkpoint state, VFX runtime and selected instance state, gameplay
  spawn/collision/combat stats, and RenderGraph pass/resource summaries. The
  builder owns observation only; runtime edits still route through explicit
  services, commands, and transactions.
- Phase 10-E replaces the fixed Course/Terrain snapshot implementation with
  `IEditorPlayIsolationProvider`, `EditorPlayIsolationRegistry`, and a
  type-erased `EditorPlaySnapshot`. Production providers cover Course,
  Terrain/Gameplay Tuning, VFX Authoring Assets, and Post-process. Capture is
  published only after every provider succeeds; Restore first captures a
  rollback snapshot and reverts all partially restored providers on failure.
  The Runtime Changes panel exposes provider fingerprints and Apply/Ignore
  selection. Keep Changes adopts only selected providers and records their
  Course/Terrain/VFX/Post-process state as one undoable grouped transaction.
- Phase 10-F completes the Transaction Core migration. Asset Mutation and
  Runtime Apply now register immutable `IEditorUndoCommand` implementations
  and resolve Domain execution through `EditorExecutionContext`. The shared
  stack no longer contains Asset/Runtime payload kinds, concrete records,
  file-transaction cleanup, or Domain apply branches. Disk-backed trash is
  owned by the Asset command lifetime, while Runtime Apply stores only the
  selected provider deltas in one grouped command.
- Phase 10-G adds the Generic Document Model. Stable document IDs and a
  provider registry now own multi-document open/active/dirty state. Save All
  validates and stages every document before committing one file transaction;
  autosave writes revisioned recovery data without touching the source.
  External content hashes block silent overwrite, and schema migration writes
  a backup and report before publishing migrated data. Course authoring is the
  first live provider; Scene, Effect/VFX, Material/Render Preset, and Project
  Settings providers use the same lifecycle contract.
- Phase 10-H adds the Editor World Model. Domain-neutral records use the
  document ID, provider ID, and persistent object GUID as their canonical
  identity; array indices remain compatibility locators only. A deterministic
  provider registry aggregates Course terrain, rock, camera, and event objects
  with read-only VFX asset/runtime objects. Course schema v3 persists GUIDs,
  Outliner visibility, and lock state, and migrates schema v1/v2 through the
  Generic Document lifecycle. Duplicate
  handles, missing parents, and hierarchy cycles are reported before the
  Outliner consumes the model.
- Phase 10-I adds the World Outliner and the domain-neutral World mutation
  boundary. The panel renders cached hierarchy queries with search/type and
  runtime/missing filters, multi-selection, visibility, lock, inline rename,
  duplicate/delete confirmation, and capability-driven drag reparent. Viewport
  picking and Diagnostics publish the same canonical persistent handles to
  `EditorSelection`, which remains the sole Details selection source. Provider
  mutation plans are validated and applied through an immutable World command;
  undo/redo uses an execution service, while failed apply/refresh/history
  publication rolls the provider state back. Course schema v3 persists
  visibility and lock state; runtime VFX objects remain explicitly read-only.
- Phase 10-J promotes the Course transform gizmo from pixel-delta debug input
  to a production manipulation path. Domain-neutral Gizmo Math owns ray-plane,
  ray-axis, signed-angle, basis projection, and snap calculations; all pointer
  input is converted through the formal viewport coordinate contract. The
  viewport exposes axis, plane, and uniform handles with World/Local space and
  Active/Median/Individual pivot modes. Same-domain multi-selection is captured
  in one property edit session, previews through the same accessor used by
  Details, commits as one grouped command, and restores every begin value on
  Escape, viewport loss, or Play/Sim authoring lock.

### Phase 11: Extensibility And Tool Registration

Deliverables:

- Stabilize provider interfaces for commands, panels, asset adapters, property
  adapters, validation adapters, runtime inspectors, and menu/toolbar sections.
- Add versioned registration descriptors so tools can be added without editing
  `AppImGuiLayer` directly.
- Add feature flags for experimental tools and migration fallback paths.
- Add command categories, shortcut conflict detection, palette search metadata,
  and menu/toolbar placement metadata.

Success criteria:

- A new VFX or Course tool can register a panel, commands, validation, and
  details support from its own module.
- Shortcut conflicts are reported before registration succeeds.
- Disabling an experimental tool leaves the rest of the editor stable.

Current staged implementation:

- Phase 11-A introduces `EditorToolRegistry` and versioned descriptors for
  command, panel, and toolbar registration. Built-in, Course, and Asset command
  providers now route command registration through the tool descriptor path when
  an editor context provides it. `AppImGuiLayer` owns a per-frame tool registry,
  panel registration is wrapped by `EditorPanelRegistrationDescriptor`, and the
  editor toolbar is no longer a hard-coded draw-time command list; it is
  populated by `EditorToolbarItemDescriptor` entries and validated against the
  active command registry. Registration diagnostics now cover unsupported
  descriptor versions, duplicate command/panel/toolbar IDs, shortcut conflicts,
  and toolbar entries that reference missing commands.
- Phase 11-B extends the same registration path to menu sections/items and
  feature gates. `EditorMenuBar` now consumes `EditorMenuSectionDescriptor` and
  `EditorMenuItemDescriptor` entries from `EditorToolRegistry`; the former
  command-category menu is rebuilt as default menu descriptors each frame.
  Command, panel, toolbar, and menu descriptors share `EditorToolFeatureGate`
  state so production, experimental, hidden, and disabled tools have one
  consistent registration policy. Disabled tools become safe no-ops; hidden
  tools can keep internal registrations while staying out of panel hosts,
  toolbar, and menu presentation. Diagnostics now also cover duplicate menu
  sections/items, missing menu commands, and menu items targeting hidden or
  unknown sections.
- Phase 11-C extends provider registration beyond UI contributions. Asset
  providers, property accessors, validation adapters, and Runtime Watch
  providers now have versioned descriptors and the same feature-gate behavior
  as commands, panels, menus, and toolbar entries. `AppImGuiLayer` no longer
  directly composes Details property accessors, validation adapters, or Runtime
  Watch rows; it registers those providers through `EditorToolRegistry`, then
  consumes the composed accessor, validation service, and runtime watch build
  path. The default Runtime Watch builder is now an appendable provider so
  future modules can contribute rows without editing the central builder.
- Phase 11-D adds a tool module boundary and startup/frame registration
  pipeline. `EditorToolModuleRegistry` owns versioned module descriptors,
  load-order sorting, duplicate detection, feature-gated no-op modules, and
  module diagnostics that flow into `EditorToolRegistry`. `AppImGuiLayer`
  registers built-in asset, property, validation, and Runtime Watch
  contributions as editor modules, then executes the module pipeline with a
  shared registration context instead of directly wiring each provider at the
  call site. This establishes the boundary future Course, VFX, Asset, and
  gameplay tools can use to publish their editor capabilities from their own
  module startup code.
- Phase 11-E extracts the built-in App editor contributions into
  `AppEditorToolModules`. Startup modules now initialize built-in property
  descriptors and custom Details section providers through the same module
  pipeline, frame provider modules publish asset, property, validation, and
  Runtime Watch providers, and command modules register Course, Built-in,
  Asset, menu, and toolbar contributions in load-order. `AppImGuiLayer` now
  runs startup, provider, and command module pipelines instead of directly
  wiring those registration details at the call site.

### Phase 12: Commercial Hardening And Automation

Deliverables:

- Add automated tests for editor services: selection, transactions, asset
  registry, folder indexing, property descriptors, validation aggregation,
  command enablement, save/apply policy, document lifecycle, viewport transform
  coordinate mapping, and Feature Guard checks.
- Add smoke tests or scripted runs that open the editor, show developer tools,
  inspect VFX, open diagnostics, index assets, select course objects, edit,
  undo, redo, save, reload, and enter/exit play.
- Add performance budgets for asset indexing, validation, details rendering,
  diagnostics table rendering, and viewport resize.
- Add crash-safe persistence for layout and editor preferences.
- Add release checklist documentation and migration notes for legacy panels.

Success criteria:

- A broken core editor service fails a test or Feature Guard check before it
  reaches daily authoring use.
- The editor can recover from invalid layout files, missing assets, failed
  validation, and interrupted play sessions.
- Daily production workflows are stable enough for team use without relying on
  hidden debug panels or manual state repair.

Current staged implementation:

- Phase 12-A introduces `EditorAutomationGate`, a commercial readiness gate
  runner that aggregates the existing Editor Core regression suite and Editor
  Smoke Run behind the `--editor-commercial-gates` command-line entry point.
  The runner records each gate's id, category, pass/fail result, exit code,
  duration, warning budget, artifact path, and status message, then writes both
  `logs/editor_automation_report.json` and
  `logs/editor_automation_report.md`. This creates the first structured
  preflight artifact for PR/build review while preserving the existing
  `--editor-core-regression` and `--editor-smoke-run` entry points.
- Phase 12-B expands the commercial gate into recovery scenario coverage.
  The structured report now uses `editor.commercialAutomation.v2` and records
  check counts, owner area, critical status, and recovery action per gate.
  `--editor-commercial-gates` now includes independent recovery gates for
  corrupt layout fallback, asset GUID/dependency mutation recovery, Details
  transaction/validation recovery, and Play/Sim snapshot/apply recovery. Each
  scenario writes a focused artifact under `logs/editor_recovery_*.log` so a
  failed PR/build can point directly at the broken commercial workflow.
- Phase 12-C integrates Feature Guard and performance budgets into the same
  commercial gate. The structured report now uses
  `editor.commercialAutomation.v3` and records budget kind, measured time,
  sample count, blocked checks, and attention checks. The new
  `editor.featureGuard` gate builds a representative editor startup context and
  fails on blocked Feature Guard checks, while `editor.performanceBudget`
  measures asset indexing, validation aggregation, Details descriptor traversal,
  diagnostics data preparation, and viewport resize mapping against defined
  service budgets. The new artifacts are
  `logs/editor_feature_guard_report.log` and
  `logs/editor_performance_budget_report.log`.
- Phase 12-D closes the Phase D editor integration milestone. The runner now
  emits `editor.commercialCompletion.v4`; `commercialCompletionReady` is true
  only when every gate passes and both blocked and attention counts are zero.
  `editor.phaseDIntegration` composes Material, VFX, Animation, and Gameplay
  tools in one Document Manager, Transaction Stack, and Execution Context, then
  verifies deterministic compilation, cross-domain Undo/Redo, atomic Save All,
  Content Browser classification, Autosave, and Recovery. The independent
  `editor.northStarWorkflow` gate verifies durable asset selection, viewport
  drop, shared world selection, Scene Entity/Component authoring, transaction-
  backed transform mutations, Scene Save/Reload, Play isolation, selective
  Apply, Undo Apply, and interrupted-save recovery.

### Phase 13: Viewport Correctness And Authoring Parity

Goal: make the editor viewport a formal coordinate contract instead of a set of
parallel formulas owned by HUD, picking, gizmo, and input code.

Current staged implementation:

- Phase 13-A introduces `EditorViewportCoordinateService` as the shared
  conversion path for display-space, viewport-local, render-space, NDC, and
  world-ray coordinates. `EditorViewportInteractionService` now obtains mouse
  render coordinates through this service, `EditorViewportOverlayScope` uses it
  for HUD/debug overlay placement and scaling, and App-level screen-ray helpers
  use the same world-ray construction used by future picking/gizmo code.
- The commercial automation runner now includes `editor.viewportCorrectness`.
  This gate validates center/edge coordinate mapping, NDC orientation, identity
  view-projection world ray construction, render-to-display scale parity, and
  out-of-viewport rejection. Its artifact is
  `logs/editor_viewport_correctness_report.log`.
- Phase 13-B extends the same contract to world projection. The coordinate
  service now owns world-to-NDC, world-to-render, world-to-display, depth,
  behind-camera, and onscreen classification. Rail/HUD overlay projection now
  goes through this service instead of a local formula, viewport client-point
  conversion shares the same display-to-render path, and
  `EditorTransformGizmoService` requires a connected viewport projection
  contract before reporting manipulations ready. The viewport correctness gate
  now verifies world projection, offscreen-but-in-depth projection, and gizmo
  projection connectivity.
- Phase 13-C completes the B-4 viewport overlay boundary. Domain producers now
  submit render-space primitives and labels through eight stable overlay layers;
  only `EditorViewportOverlayService::Render` receives `ImDrawList`. Gameplay HUD
  and editor overlays have independent visibility, settings round-trip through
  atomic layout persistence, and clean screenshot suppression hides configured
  editor layers. Object labels use selection/distance/zoom filters, bounded
  overlap placement, and icon fallback. The commercial performance gate covers
  10,000 label candidates with a 50 ms Debug budget and a 128-result visual cap.
- Phase 13-D completes the B-5 Scene/Entity/Component authoring foundation.
  `EditorScene` is the versioned Scene Document live model; stable Entity GUIDs,
  parent GUID hierarchy, required Transform, typed Components, and Entity/Asset
  references are validated before deterministic serialization. The Scene World
  provider routes Outliner creation/hierarchy and Details Component changes
  through generic World transactions. Content Browser Asset drag payloads are
  accepted by the Viewport to create typed Scene Entities. Regression verifies
  Undo/Redo plus File Transaction Save/Reload identity preservation.
- Phase 13-E completes C-1 Durable Asset Identity. Imported and migrated assets
  receive independent durable GUID metadata through atomic file transactions;
  registry audits expose coverage, missing metadata, and duplicate GUIDs.
  Project asset metadata is version-controlled and the headless
  `--editor-asset-meta-migrate` entry point migrates legacy content and fails
  when coverage is incomplete or duplicate GUIDs remain.
  Rename/Move requires unique durable identity and persists old ID/path aliases
  in the project redirect table. Dependency scanning separates canonical GUID
  references from path-only references, and the transaction-backed repair
  command rewrites resolvable paths while preserving Undo/Redo. Diagnostics,
  Details asset pickers, and missing-reference repair share the same
  GUID/path/redirect resolver.
- Phase 13-F completes C-2 Content Browser production workflows. A versioned,
  atomically persisted state model restores folder, filters, view mode,
  collection/favorite membership, and durable-GUID selection across sessions.
  Folder Tree and combined search/kind/tag/collection filters feed both Grid and
  List views. Both views emit the same GUID drag payload accepted by Viewport and
  compatible Details AssetRef fields. Provider-backed SCM/dirty/cook columns and
  dependency/reference inspection expose production status without guessing a
  source-control backend. Duplicate joins Rename/Move/Delete/Repair in the Asset
  Mutation Core and atomically creates a new source plus durable metadata with
  Undo/Redo.
- Phase 13-G completes C-3 Right Inspector evolution. Details is the default
  Inspector tab and legacy VFX Inspector state migrates to it. The former
  monolithic panel is split into VFX Details, VFX Runtime, Scene Lighting, Post
  Process, Render Debug Views, and Performance responsibilities. Details owns an
  atomically persisted search/category/favorite/changed view state, highlights
  default and transaction deltas, evaluates descriptor edit conditions, and
  binds validation issues to individual property rows. Array/Map/Struct kinds,
  Asset and World Object pickers, and a provider-neutral Prefab override/revert
  contract extend the descriptor architecture without coupling the editor core
  to a future Prefab backend.
- Phase 13-G validation passes Debug v143 build, 30/30 regression cases, and
  10/10 smoke steps. The optimized Development v143 commercial run passes all
  9 gates and 124 checks, including all six service performance budgets.
- Phase 13-H completes C-4 Bottom Dock evolution. Panel descriptors expose a
  four-area classification, pin/close capabilities, and warning/error badge
  providers. The Bottom Dock host adds compact area navigation, search,
  overflow, Pin/Close/Reopen, context move, drag-to-area, and a persisted
  Developer-panel visibility boundary. Layout schema v2 atomically restores
  active area/tab, search, visibility, pin, and group overrides while retaining
  v1 compatibility. Performance moves from the Inspector into Profiling.
- Phase 13-H validation passes Debug v143 build, an 8-second live-frame probe,
  31/31 regression cases, and 10/10 smoke steps. Development v143 passes all 9
  commercial gates and 124 checks with zero failed, warning, or blocked checks;
  Feature Guard separately reports one pre-existing Property-accessor attention.
- Phase 13-I completes C-5 Menu/Toolbar/Status Bar evolution. Menu registration
  now maps commands into a stable File/Edit/Window/Tools/Build/Play/Help model,
  while document-type metadata hides Course actions outside the active Course
  document. The responsive toolbar prioritizes Save, Undo/Redo, transform and
  Play controls, measures button labels, and moves the remaining commands into
  an overflow popup. Transform mode, space, and snap labels reflect the shared
  gizmo state and execute exclusively through the command registry.
- Phase 13-I also separates `EditorStatusBarSnapshot` from ImGui presentation.
  It aggregates validation, dirty/autosave state, preview/GPU background work,
  active document/session, latest command, and provider-backed SCM/cook status.
  Missing Shader Compile and Memory providers remain explicitly `Unbound`; a
  responsive compact row preserves the highest-priority health signals and a
  details popup exposes the complete status model.
- Phase 13-I validation passes Debug v143 build, an 8-second live-frame probe,
  32/32 regression cases, and 10/10 smoke steps. Development v143 passes all 9
  commercial gates and 124 checks with zero failed, warning, or blocked checks.
- Phase 13-J completes D-1 Course Timeline Track Provider evolution. A
  Course-independent `EditorSequencerService` owns provider registration,
  key selection, interactive preview/commit/cancel, snapping, clipboard,
  scrub preview, and generic transaction commands. Mutation application is
  rollback-safe across providers and a failed transaction registration restores
  the pre-preview state.
- `CourseSequencerTrackProvider` adapts persistent Course object GUIDs into
  Event, Placement, Camera, Lighting, Material, VFX, and Gameplay Trigger
  tracks. The provider owns Course-specific capture, move, remove, insert, and
  duplicate behavior while the timeline UI consumes only generic track/key
  records. Scrubbing routes through an explicit callback to the Course runtime
  preview instead of coupling Sequencer Core to runtime classes.
- Phase 13-J validation passes Debug v143 build, an 8-second live-frame probe,
  33/33 regression cases, and 10/10 smoke steps. Development v143 passes all 9
  commercial gates and 124 checks with zero failed, warning, or blocked checks.
- Phase 13-K completes D-2 Prefab authoring. `.prefab` is a durable Asset and a
  generic Document type with validated v2 serialization. Scene schema v2 stores
  persistent Prefab instance identity, source-to-instance Entity bindings, and
  property/structural overrides while migrating legacy Scene v1 documents.
- `EditorPrefabService` instantiates Asset templates, enforces an eight-level
  nested depth and cycle rejection, creates recoverable Missing Asset
  placeholders, and exposes Apply, Revert, and recovery through Details and
  Viewport/Content Browser workflows. World Outliner distinguishes connected
  instances, members, and missing sources.
- Every Prefab mutation uses a `prefab` generic command. Apply atomically
  snapshots both Scene and source Prefab Asset, so Undo/Redo and failed command
  registration restore both sides together. Property rows consume the existing
  provider-neutral override contract backed by the production Prefab service.
- Phase 13-K validation passes Debug and Development v143 builds, an 8-second
  live-frame probe, 34/34 regression cases, and 10/10 smoke steps. Development
  passes all 9 commercial gates and 124 checks with zero failed, warning, or
  blocked checks and all six performance budgets within limits.
- Phase 13-L completes D-3 Material Graph foundation. `EditorGraph` is a
  domain-independent typed graph core with stable node/link identity, pin
  cardinality, conversion rules, cycle rejection, and bounded node/link counts.
  Material-specific node definitions and compilation remain in the Material
  module rather than leaking into Graph Core.
- `.material`/`.materialgraph` are durable `MaterialGraph` Assets and generic
  Documents. The v2 format persists graph/settings identity, supports v1
  migration, remains saveable while authoring compile errors exist, and uses
  the existing atomic Save/Autosave/Recovery lifecycle.
- `EditorMaterialGraphService` creates snapshot Commands in the shared
  Transaction stack. Add/remove/connect/disconnect/property/move operations,
  global Undo/Redo, Dirty state, and notifications have no private history or
  save system. The deterministic compiler emits fingerprinted HLSL source and
  retains the last successful artifact when a later authoring compile fails.
- The Authoring Bottom Dock provides the Material canvas, typed connection
  controls, node property editing, compile diagnostics, and generated HLSL.
  Unified validation resolves Texture references by durable Asset GUID.
- Phase 13-L validation passes Debug/Development v143 builds with zero
  warnings/errors, an 8-second live-frame probe, 35/35 Editor Core regression
  cases, and 10/10 smoke steps.
  Development passes all 9 commercial gates and 124 checks with zero failed,
  warning, or blocked checks; all performance budgets remain within limits.
- Phase 13-M completes D-3 Advanced VFX Graph. The domain-independent
  `EditorGraph` core is reused by a VFX schema containing System Output,
  Emitter, Spawn Rate/Burst, Initialize Velocity, Gravity/Drag, and
  Sprite/Ribbon/Beam Render modules. VFX types do not leak into Graph Core.
- `.vfxgraph`/`.vfxsystem` are durable `VfxGraph` Assets and generic Documents.
  Schema v2 persists CPU/GPU target, bounded particle capacity, fixed time
  step, stable graph identity, and supports v1 migration through existing
  Atomic Save/Autosave/Recovery services.
- The deterministic compiler produces a fingerprinted staged execution program,
  simulation HLSL, emitter runtime descriptors, and durable dependencies. The
  service keeps the last successful artifact when later edits fail validation.
- All graph/settings mutations use the `vfx-graph` snapshot Command domain and
  shared global Undo/Redo, Dirty, notification, and validation services. Preview
  apply resolves Material/Texture GUIDs and publishes only a successful artifact
  to the existing EffectRuntime, preventing partial authoring state from leaking.
- The VFX Bottom Dock provides typed connections, full node-property selection,
  simulation settings, compile diagnostics, generated program/HLSL inspection,
  and explicit runtime preview apply. Regression coverage increases to 36 cases.
- Phase 13-M validation passes Debug/Development v143 builds with zero
  warnings/errors, 36/36 Editor Core regression cases, and 10/10 smoke steps.
- Phase 13-N completes D-3 Animation State Machine. `EditorGraphSchema` now
  carries a domain-neutral cycle policy: Material/VFX remain acyclic while
  Animation State transitions can form valid loops without weakening other
  graph validation contracts.
- Runtime `AnimationStateMachineInstance` evaluates Bool/Float/Int/Trigger
  parameters, priority, thresholds, normalized exit time, loop/speed, and
  cross-fade progress deterministically. `ApplyAnimationBlend` blends per-joint
  translation, rotation, and scale before the existing skeleton update path.
- `.animsm`/`.animstate` are durable `AnimationStateMachine` Assets and generic
  Documents. Schema v2 persists typed Parameters and stable Entry/State/
  Transition nodes, remains saveable with authoring diagnostics, supports v1
  migration, and uses common Atomic Save/Autosave/Recovery.
- State, Transition, Parameter, property, connection, and move mutations use
  the `animation-state-machine` snapshot Command domain with global Undo/Redo,
  Dirty state, notifications, and last-known-good runtime programs.
- The Animation Bottom Dock provides cyclic graph authoring, all-node property
  editing, Parameter creation/removal and typed Preview values, step/reset,
  compile diagnostics, and generated-program inspection. Unified diagnostics
  resolves animation source GUIDs to durable skinned Mesh Assets.
- Phase 13-N validation passes Debug/Development v143 builds with zero
  warnings/errors, 37/37 Editor Core regression cases, and 10/10 smoke steps.
  The tiered validation policy intentionally defers the Development Commercial
  Gate and live-frame probe to the release/integration checkpoint.
- Phase 13-O completes D-3 Gameplay Visual Scripting. The editor-independent
  `GameplayVisualScriptInstance` executes compiled BeginPlay/Tick programs with
  Bool/Float/Int/String values, typed variables, expressions, Branch, Set,
  Print, Emit Event, Return, deterministic trace, and host callbacks. Runtime
  execution is bounded by a per-Asset instruction budget and expression-depth
  guard, so cyclic Exec flow cannot hang the engine frame.
- Gameplay Schema reuses the domain-independent Graph Core and distinguishes
  typed Data pins from Exec pins. Data-expression cycles are rejected by the
  compiler while intentional Exec cycles and flow merges remain valid under the
  Runtime budget. Stable Node IDs produce a fingerprinted, deterministic
  expression/instruction/event-entry program.
- `.gameplay`/`.visualscript` are durable `GameplayVisualScript` Assets and
  generic Documents. Schema v2 persists typed variables, execution budget,
  nodes/properties/links, supports v1 migration, and uses shared Atomic Save,
  Autosave, Recovery, Dirty, notification, and global Undo/Redo services.
- The Gameplay Bottom Dock provides typed connection authoring, all-node
  property editing, variable/budget controls, BeginPlay/Tick preview, execution
  trace/output, diagnostics, and generated-program inspection. Failed edits keep
  the last successful Runtime Program instead of publishing partial state.
- Phase 13-O validation passes Debug/Development v143 builds with zero
  warnings/errors, 38/38 Editor Core regression cases, and 10/10 smoke steps.
  The tiered validation policy defers Commercial Gate and live-frame probing to
  the Phase D integration checkpoint.
- Phase 13-P is an independent Editor Font Foundation inserted before further
  Gameplay Visual Scripting evolution. `EditorFontService` owns a versioned
  regular/monospace font preference, pixel sizes, UI scale, and Japanese glyph
  policy without coupling those settings to Gameplay, Graph, or runtime code.
  Font files are accepted only from `Resources/Editor/Fonts`, must be relative
  `.ttf`/`.otf`/`.ttc` paths inside that root, and are bounded to 64 MiB.
- Font preferences are persisted through `EditorFileTransaction`; invalid,
  missing, or unloadable custom fonts fall back to ImGui's built-in font so an
  editor restart cannot make the UI unusable. The D3D12 font atlas is rebuilt
  only during ImGui initialization, while the Bottom Dock `Editor Fonts` panel
  records changes for the next restart and reports pending state explicitly.
- Phase 13-P validation passes Debug/Development v143 builds with zero
  warnings/errors, 39/39 Editor Core regression cases, and 10/10 smoke steps.
  A normal Debug editor frame remained stable for an 8-second D3D12/ImGui font
  atlas probe. This independent insertion does not move the active milestone
  away from the Phase D integration/commercial completion gate.
- Phase 13-Q completes Phase D integration. Scene Entity handles are now valid
  Production Gizmo targets, and `EditorWorldMutationKind::SetComponentProperty`
  carries component type, property name, and serialized value through the
  domain-neutral World mutation plan. `SceneWorldObjectProvider` applies these
  changes from immutable before/after snapshots, so translation, rotation, and
  scale are atomic and globally undoable. Feature Guard attention is now a hard
  completion failure rather than an informational result.
- Development x64 validation passes 39/39 Editor Core regression cases and the
  complete commercial runner with 11/11 gates, 155/155 checks, zero blocked,
  zero attention, and zero performance warnings. This completes the editor
  milestone; the engine-wide Commercial Release Gate still requires shipping
  packaging, runtime/editor target separation, soak/GPU coverage, crash
  reporting, and licensing checks.

## Commercial Completion Scorecard

Use this scorecard to track the path to 100%. Percentages are intentionally
workflow-based, not file-count based.

| Area | Current target after Phase 5 | 100% requirement |
| --- | ---: | --- |
| Core service architecture | 80% | All authoring mutation uses shared context, command, transaction, dirty, validation, and notification services. |
| Existing feature protection | 85% | Feature Guard covers every migration-sensitive VFX, Course, RenderGraph, runtime, asset, and viewport workflow. |
| Workspace layout | 70% | Persistent dock layout, classified production Bottom Dock, panel search/overflow/pin/close/reopen/move, Developer isolation, badges, restored tabs, default layouts, and overlap-free viewport. |
| Viewport authoring | 80% | Accurate render target, picking, gizmo manipulation, HUD composition, input capture, and play locks. |
| Property/details | 82% | Search/category/favorite persistence, container descriptors, Asset/Object pickers, edit conditions, row validation, changed highlights, Prefab override provider, multi-selection, and transaction-backed edits. |
| Asset/content browser | 82% | Durable GUID/meta, persistent Folder/Grid/List/filter/collection workflows, dependency graph, provider-backed SCM/cook status, thumbnails, drag/drop, and atomic mutation commands. |
| Diagnostics/profiling | 70% | Unified diagnostics with source navigation, profiling views, severity filters, and domain adapters. |
| Play/simulate isolation | 60% | Runtime clone/snapshot boundary, explicit apply changes, pause/step, and safe stop restoration. |
| Extensibility | 80% | Versioned registration APIs and App-level module startup/frame pipelines for commands, panels, assets, properties, validation, Details sections, menus, toolbar entries, and Runtime Watch providers. |
| Automation/hardening | 60% | Structured commercial gate runner, unit/smoke/regression aggregation, recovery scenario gates, Feature Guard gates, performance budgets, viewport correctness gates, and recovery tests covering critical editor workflows. |

The editor should not be called commercial-ready until every area is at least
90%, no critical area is below 85%, and Feature Guard reports no blocked
checks in the default editor startup flow.

## Testing Strategy

Every phase should have a small regression checklist:

- Build Debug x64.
- Open editor with developer tools visible.
- Confirm VFX Inspector and VFX Diagnostics open.
- Confirm Effect Assets tab still lists loader diagnostics.
- Spawn or inspect a VFX instance.
- Confirm Runtime Queues and RenderGraph tabs still show data.
- Open Course Timeline.
- Add/edit/remove a terrain placement or rock cluster.
- Undo and redo that edit.
- Save and reload the course.
- Confirm validation results still appear.

For code-level tests, start with small pure C++ tests where practical:

- Transaction stack push/undo/redo.
- Object handle equality and invalidation.
- Property descriptor lookup.
- Asset registry indexing from sample paths.
- Diagnostic severity/count aggregation.

Commercial-readiness testing should add the following gates:

- Layout persistence: save layout, restart, restore panels, recover from invalid
  layout data.
- Viewport correctness: resize the editor window and verify render target size,
  camera aspect, picking ray, gizmo position, HUD composition, and reticle/mouse
  alignment.
- Asset safety: index folders, detect missing references, validate path-only
  references, rename/move GUID-backed assets, and repair references.
- Details editing: edit scalar/vector/enum/asset reference properties, verify
  transaction creation, undo, redo, dirty state, validation, save, and reload.
- Play/simulate isolation: edit authoring data, enter play, mutate runtime,
  stop, verify authoring restoration, then explicitly apply runtime changes
  through a command.
- Feature Guard: startup must report no blocked core services and must keep
  existing VFX inspector, VFX diagnostics, RenderGraph diagnostics, and Course
  authoring paths usable.
- Performance: asset indexing, validation, diagnostics rendering, details
  drawing, and viewport resize should remain within defined frame or startup
  budgets for a representative project.

Release candidates must pass the manual checklist, automated editor-service
tests, Feature Guard checks, and at least one smoke run that covers VFX,
Course, asset browser, diagnostics, viewport selection, transform editing, and
play/simulate lifecycle.

## First Implementation Target

The first useful implementation should be small:

1. Add `application/editor/EditorObjectHandle.h`.
2. Add `application/editor/EditorSelection.h/.cpp`.
3. Add `application/editor/EditorTransactionStack.h/.cpp` with snapshot
   transactions only.
4. Bridge the current course object undo/redo path to the shared transaction
   stack.
5. Add a tiny "Editor Core" debug section that shows current selection and
   undo/redo depth.

This target creates real value without touching VFX runtime/render code. After
that, VFX can be connected through descriptors and adapters while preserving the
typed storage and renderer input boundaries described in `docs/VfxEngineFlow.md`.

## Current Next Implementation Targets

After the current Editor Core foundation is in place, implementation should move
in this order:

1. Expand `EditorPanelRegistry` and `EditorPanelHost` to support viewport,
   left, right, bottom, content, and diagnostics host areas.
2. Add layout persistence for panel visibility, active tabs, split ratios, and
   default workspace presets.
3. Upgrade `EditorTransformGizmoService` from state synchronization to
   transaction-backed transform manipulation.
4. Extend `EditorAssetRecord` with GUID, logical path, dependency, missing,
   dirty, tag, and metadata fields while keeping path fallback.
5. Add editor-service tests for transaction, property, asset registry,
   validation, command enablement, viewport coordinate mapping, and Feature
   Guard reports.

These targets move the project from "foundation complete" toward "production
beta". They should be implemented before attempting larger systems such as a
node graph, material editor, advanced VFX graph, sequencer, or scripting layer.
