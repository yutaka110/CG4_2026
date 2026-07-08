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

## Non-goals

- Do not replace the current ImGui editor windows in one large rewrite.
- Do not rewrite `EffectSystem`, `EffectRuntime`, or VFX renderer inputs as part
  of the initial Editor Core.
- Do not force all assets into a new serialized format immediately.
- Do not build Blueprint, Niagara, Sequencer, or a full material graph before the
  editor foundations are stable.
- Do not make runtime systems depend on editor-only APIs.

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

Docking can be introduced later. Fixed ImGui windows are acceptable while the
core services are being validated.

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
