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
- Full rename/move/delete safety, dependency graph navigation, thumbnail
  generation, and automated asset migration tests remain future Phase 8 work.

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

## Commercial Completion Scorecard

Use this scorecard to track the path to 100%. Percentages are intentionally
workflow-based, not file-count based.

| Area | Current target after Phase 5 | 100% requirement |
| --- | ---: | --- |
| Core service architecture | 80% | All authoring mutation uses shared context, command, transaction, dirty, validation, and notification services. |
| Existing feature protection | 85% | Feature Guard covers every migration-sensitive VFX, Course, RenderGraph, runtime, asset, and viewport workflow. |
| Workspace layout | 45% | Persistent dock layout, panel host areas, restored tabs, default layouts, and overlap-free viewport. |
| Viewport authoring | 60% | Accurate render target, picking, gizmo manipulation, HUD composition, input capture, and play locks. |
| Property/details | 65% | Full descriptor coverage, multi-selection, validation hints, reset/copy/paste, and transaction-backed edits. |
| Asset/content browser | 50% | GUID/meta, dependency graph, thumbnails where useful, safe rename/move/delete, and repair commands. |
| Diagnostics/profiling | 70% | Unified diagnostics with source navigation, profiling views, severity filters, and domain adapters. |
| Play/simulate isolation | 60% | Runtime clone/snapshot boundary, explicit apply changes, pause/step, and safe stop restoration. |
| Extensibility | 50% | Versioned registration APIs for commands, panels, assets, properties, validation, menus, and toolbar entries. |
| Automation/hardening | 25% | Unit, smoke, regression, performance, and recovery tests covering critical editor workflows. |

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
