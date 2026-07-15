# Project Layout

This repository is organized around the following top-level areas:

- `application/`: application-level systems, panels, render pipelines, runtime state, and VFX code.
- `application/editor/EditorAssetPreviewJobQueue.*`: budgeted preview job
  queue that decouples Asset Browser thumbnail state from expensive preview
  generation.
- `application/editor/EditorAssetGpuThumbnailRenderer.*`: budgeted GPU-backed
  thumbnail render queue that turns ready preview metadata into resident
  thumbnail handles for Asset Browser display.
- `application/editor/EditorAssetD3D12ThumbnailGpuBackend.*`: D3D12 SRV
  allocation backend used by the editor thumbnail renderer to expose
  shader-visible `ImTextureID` handles; it also owns renderer-backed mesh
  thumbnail draw resources, contiguous material texture SRV tables, and
  fence-aware preview texture upload lifetime.
- `application/editor/EditorAssetThumbnailCache.*`: stable thumbnail cache key,
  memory/disk cache policy, LRU eviction, and cache telemetry for generated
  preview payloads.
- `application/editor/EditorAssetMeshThumbnailPreviewRenderer.*`: deterministic
  mesh/material thumbnail preview payload generation used before GPU upload.
- `application/editor/EditorAssetPreviewRenderTarget.*`: D3D12 offscreen
  preview render target pool used for direct material thumbnail SRV
  publication.
- `Resources/EditorAssetThumbnailPreview.*.hlsl`: dedicated thumbnail preview
  mesh shaders for renderer-backed mesh/material asset thumbnails, including
  UV propagation, multi-material texture sampling, and lightweight PBR-style
  preview shading.
- `application/editor/EditorAssetPreviewSceneRenderer.*`: formal preview-scene
  pass boundary used by mesh/material thumbnails before the D3D12 upload path.
- `application/editor/EditorThumbnailUploadRetirementQueue.*`: fence-aware
  upload buffer lifetime queue for thumbnail GPU copy resources.
- `application/editor/EditorAssetFallbackIconAtlas.*`: production fallback icon
  atlas pixel generation for non-texture assets and decode-failure fallbacks.
- `application/editor/EditorAssetThumbnailTextureLoader.*`: DirectXTex-backed
  texture thumbnail decoder that normalizes source images into small RGBA8
  upload payloads.
- `application/editor/EditorAssetPreviewProvider.*`: rich asset preview
  metadata generation used by the thumbnail cache and Asset Browser; mesh
  previews use production Assimp inspection where possible to extract source
  bounds, material slots, material texture stamps, camera distance, and light
  preset inputs for preview-scene thumbnail binding.
- `application/editor/EditorCompositePropertyAccessor.*`: Details mutation
  gateway that composes domain-specific property accessors behind the official
  `EditorPropertyEditService` transaction path.
- `application/editor/EditorPropertyClipboardService.*`: typed Details
  property clipboard that stores formatted property values and validates paste
  compatibility before routing edits through the official mutation path.
- `application/editor/EditorDetailsSectionProvider.*`: extension point for
  domain-specific Details sections that still route mutations through the
  official property accessor/service/transaction path.
- `application/editor/EditorBuiltinDetailsSectionProviders.*`: built-in custom
  Details sections for production VFX, post-process, course event dispatch, and
  render preset authoring coverage.
- `application/editor/EditorPlaySessionLifecycleService.*`: official
  Play/Simulate lifecycle path that owns snapshot capture/restore and session
  state transitions.
- `application/editor/EditorPlaySessionRuntimeControlService.*`: official
  Play/Sim runtime control path for pause, resume, single-step, and snapshot
  reset without bypassing the Play/Sim isolation boundary.
- `application/editor/EditorRuntimeAuthoringApplyService.*`: confirmed
  runtime-to-authoring apply path that records transactions, marks dirty state,
  and updates the active Play/Sim restore snapshot.
- `application/editor/play/IEditorPlayIsolationProvider.h`: domain-neutral
  Capture/Restore/fingerprint/Runtime ChangeSet contract.
- `application/editor/play/EditorPlayIsolationRegistry.*`: deterministic
  provider composition with atomic Capture publication and rollback-safe
  multi-provider Restore.
- `application/editor/play/EditorPlaySnapshot.*`: type-erased per-provider
  authoring snapshots bound to a Play session serial.
- `application/editor/play/EditorRuntimeChangeSet.*` and
  `EditorRuntimeChangeSetPanel.*`: selective Keep Changes model and UI; selected
  providers are committed as one undoable grouped transaction.
- `application/editor/play/EditorBuiltinPlayIsolationProviders.*`: production
  Course, Terrain/Gameplay Tuning, VFX Authoring Asset, and Post-process
  isolation providers with stable fingerprints.
- `application/editor/play/EditorPlayMutationGuard.h`: intent-aware guard that
  rejects direct Authoring writes during Play/Sim while allowing Runtime,
  Keep Changes, and isolation restore paths.
- `application/editor/asset/EditorAssetMutationChange.h`,
  `IEditorAssetExecutionService.h`, and `EditorAssetMutationUndoCommand.*`:
  Asset-domain command data, execution boundary, and disk-backed history
  ownership kept outside the shared Transaction Stack.
- `application/editor/play/EditorRuntimeApplyChange.h`,
  `IEditorRuntimeApplyExecutionService.h`, `EditorRuntimeApplyExecutionService.*`,
  and `EditorRuntimeApplyUndoCommand.*`: selected-provider Runtime Apply delta
  command and target-bound execution adapter.
- `application/editor/documents/EditorDocumentId.h`,
  `IEditorDocumentProvider.h`, `EditorDocumentRegistry.*`, and
  `EditorDocumentManager.*`: stable IDs, provider discovery, and multi-document
  lifecycle.
- `application/editor/documents/EditorDocumentSaveService.*`,
  `EditorAutosaveService.*`, `EditorExternalChangeMonitor.*`, and
  `EditorDocumentRecoveryService.*`: Atomic Save All, external conflict
  protection, revisioned autosave, crash recovery, and schema migration
  artifacts.
- `application/editor/documents/EditorCourseDocumentProvider.*` and
  `EditorTextDocumentProvider.*`: live Course integration plus the shared
  Scene, Effect/VFX, Material/Render Preset, and Project Settings provider
  contract.
- `application/editor/world/EditorWorldObjectRecord.*`,
  `IEditorWorldObjectProvider.h`, `EditorWorldObjectRegistry.*`, and
  `EditorWorldModel.*`: domain-neutral persistent world identity, deterministic
  provider aggregation, cached hierarchy queries, stable resolve, and
  structural diagnostics consumed by the World Outliner.
- `application/editor/world/CourseWorldIdentity.*` and
  `CourseWorldObjectProvider.*`: Course schema-v3 persistent GUID,
  visibility/lock state, hierarchical records, and capability-driven mutation
  plans for terrain placements, rock clusters, camera keys, and event markers.
- `application/editor/world/VfxWorldObjectProvider.*`: read-only VFX effect
  asset records and explicit locked/runtime-only effect instance records.
- `application/editor/world/EditorWorldMutation.*`,
  `IEditorWorldMutationProvider.h`, `IEditorWorldMutationExecutionService.h`,
  `EditorWorldMutationService.*`, and `EditorWorldMutationUndoCommand.*`:
  domain-neutral validation, provider snapshot planning, rollback-safe apply,
  transaction memory preflight, and Undo/Redo execution for World edits. The
  component-property mutation path carries Scene Transform translation,
  rotation, and scale without introducing Scene payloads into Transaction Core.
- `application/editor/world/EditorWorldOutlinerPanel.*`: hierarchy/search/type
  filters, multi-selection, Runtime/Missing presentation, visibility/lock,
  inline rename, duplicate/delete, drag reparent, and selection-scroll UI.
- `application/editor/EditorRuntimeWatchBuilder.*`: read-only Runtime Watch row
  builder for Play/Sim state, selection, Course runtime, VFX, gameplay systems,
  and RenderGraph coverage.
- `application/editor/EditorAutomationGate.*`: commercial editor readiness gate
  runner that aggregates regression/smoke checks, commercial recovery scenarios,
  Feature Guard checks, performance budgets, Phase D four-tool integration, and
  the North-star end-to-end workflow. It emits strict
  `editor.commercialCompletion.v13` JSON/Markdown reports under `logs/`; the E-2
  Production Placement gate exercises click placement, a multi-entity brush
  stroke, durable references, Undo/Redo, Cancel, unsupported-Asset rejection,
  and Play locking. The E-3 Production Terrain gate adds preview isolation,
  bounded Sculpt/Paint strokes, local dirty ranges, Undo/Redo, Course
  persistence, and runtime consumption. The E-4 Production Geometry gate adds
  stable element identity, closed-manifold topology edits, Scene-bound preview
  isolation, compact Transactions, collision source hashes, Undo/Redo, and Play
  locking. The E-5 Production Mesh Bake gate adds deterministic Renderer LODs,
  checksummed Physics collision, atomic four-file publication, durable rebake
  identity, runtime resource views, Undo/Redo, Cancel, and Play locking. Any
  failed gate, blocked check, or attention check prevents readiness. The E-6
  Production Scene Instance gate additionally executes WARP D3D12 upload,
  draw-packet creation, LOD/frustum policy, Box Physics queries, and
  completed-fence resource retirement. The E-7 Material/Lighting gate adds
  versioned Material Instance inheritance, slot binding, deterministic Light
  priority, WARP Material/Light CBVs, hot reload, fallback, and fence retirement.
  The E-8 Texture Residency gate adds durable albedo/normal handoff, physical
  mip-tail allocation under a GPU budget, isolated real SRVs, cache reuse,
  hot-reload descriptor swaps, missing fallback, deterministic LRU eviction,
  and completed-fence resource/descriptor retirement.
  The E-9 Shader Variant gate executes generated Material Graph HLSL on DXC,
  creates a normal-map PSO on WARP, verifies non-blocking fallback and
  last-known-good hot reload, fence retirement, and Pipeline Library reuse
  after restart. The E-10 Multi-Light/Shadow gate verifies priority-bounded
  Scene Light collection, logarithmic Scene View clusters, overflow and shadow
  budget fallback, the extended Main root contract, WARP structured buffers,
  D32 texture-array atlas/PSO creation, and command-list execution.
- `application/editor/EditorAssetRegistry.*`: C-1 durable Asset identity index,
  metadata coverage and duplicate-GUID audits, GUID/path/redirect reference
  resolution, dependency classification, and persistent project redirect table.
- `application/editor/EditorContentBrowserState.*`: C-2 versioned session state,
  Folder/Kind/Tag/Search filtering, Favorite/Collection membership, durable-GUID
  selection restore, atomic persistence, and provider-backed SCM/dirty/cook
  status registry.
- `application/editor/EditorDetailsViewState.*`: C-3 atomic Details UI state for
  search, Category visibility, Favorite and Changed filters, plus the
  provider-neutral object/property Prefab override query/revert contract.
- `application/editor/EditorDetailsPanel.*`: C-3 production Details table with
  persisted filtering, changed highlights, per-property validation, Edit
  Conditions, Array/Map/Struct editors, Asset/World Object pickers, and Prefab
  override presentation.
- `application/editor/EditorPanelRegistry.*` and `EditorPanelHost.*`: C-4
  production Bottom Dock descriptors and host for Output/Profiling/Authoring/
  Developer classification, compact navigation, search/overflow,
  Pin/Close/Reopen, drag/context area movement, and live severity badges.
- `application/editor/EditorLayoutPersistenceService.*`: atomic layout schema v2
  persistence for Bottom Dock active area/tab, search, pin, close, group
  overrides, and Developer visibility with v1 compatibility and safe recovery.
- `application/editor/EditorMenuBar.*`: C-5 stable File/Edit/Window/Tools/Build/
  Play/Help command routing and active-document contextual menu visibility.
- `application/editor/EditorToolbar.*`: C-5 responsive primary workflow toolbar,
  transform-state presentation, Course document context, and command overflow.
- `application/editor/EditorStatusBar.*`: C-5 UI-independent project-health
  snapshot plus compact/overflow presentation for validation, autosave,
  background tasks, document/session, SCM/cook, GPU, and provider availability.
- `application/editor/EditorAssetBrowserPanel.*`: C-2 Folder Tree, Grid/List,
  combined filters, Context Menu, Reference/Dependency inspection, production
  status columns, and GUID drag source for Viewport/Details targets.
- `application/editor/EditorAssetImportService.*`: import/reimport plus atomic
  durable `.meta` creation and batch migration from legacy/provisional identity.
- `application/AppCommandLineRunner.*`: `--editor-asset-meta-migrate` headless
  migration/CI entry point that requires 100% durable coverage and no duplicate
  GUIDs after indexing `Resources`.
- `application/editor/EditorAssetMutationSafety.*` and
  `EditorAssetMutationExecutor.*`: unique durable-GUID preflight for Rename/Move,
  atomic source/meta mutation, redirect maintenance, and transaction-backed
  path-only reference repair with Undo/Redo.
- `application/editor/EditorAssetReferenceDiagnosticsAdapter.*`: missing or
  duplicate GUID, path-only reference, unresolved dependency, and stale redirect
  diagnostics consumed by the shared Diagnostics panel.
- `application/editor/EditorViewportCoordinateService.*`: formal editor
  viewport coordinate contract for display-space, viewport-local,
  render-space, NDC, world-ray, and world-projection conversion shared by
  input, HUD overlays, picking, and transform gizmo paths.
- `application/editor/EditorViewportOverlay.*`: B-4 production viewport overlay
  pipeline with eight stable layers, provider/command submission, clip-only
  rendering, selection/distance/zoom filters, bounded label collision layout,
  icon fallback, screenshot suppression, and per-layer frame statistics.
- `application/editor/scene/EditorScene.*`: B-5 versioned Scene authoring model
  with stable Entity GUIDs, Transform hierarchy, typed Components, object
  references, validation, and mutation primitives.
- `application/editor/documents/EditorSceneDocumentProvider.*`: B-5 Scene
  Document provider for deterministic serialization, schema validation,
  Generic Document Save/Reload, autosave, and recovery integration.
- `application/editor/prefab/EditorPrefab.*` and
  `application/editor/documents/EditorPrefabDocumentProvider.*`: D-2 validated
  Prefab Asset model, explicit nested references, versioned serialization,
  generic Document lifecycle, and v1-to-v2 schema migration.
- `application/editor/prefab/EditorPrefabService.*`: D-2 Scene instantiation,
  persistent source binding, Property/Structural Override, Apply/Revert,
  Missing Asset recovery, nested cycle/depth policy, and atomic Scene+Asset
  Undo/Redo service.
- `application/editor/graph/EditorGraph.*`: D-3 domain-independent typed-node,
  pin, link, conversion, cardinality, safety-limit, and acyclic graph core.
- `application/editor/material/EditorMaterialGraph.*` and
  `application/editor/documents/EditorMaterialGraphDocumentProvider.*`: D-3
  Material schema, deterministic HLSL artifact compiler, durable `.material`
  Asset/Document model, v1-to-v2 migration, and shared Transaction integration.
- `application/editor/material/EditorMaterialGraphPanel.*` and
  `EditorMaterialGraphDiagnosticsAdapter.*`: Material authoring canvas, typed
  connection workflow, generated-source/compile diagnostics, and durable
  Texture GUID validation in the common Bottom Dock/Diagnostics systems.
- `application/editor/vfx/EditorVfxGraph.*` and
  `application/editor/documents/EditorVfxGraphDocumentProvider.*`: D-3
  Advanced VFX schema, deterministic Spawn/Initialize/Update/Render program
  compiler, durable `.vfxgraph`/`.vfxsystem` Asset/Document model, v1-to-v2
  migration, last-known-good artifacts, and shared Transaction integration.
- `application/editor/vfx/EditorVfxGraphPanel.*` and
  `EditorVfxGraphDiagnosticsAdapter.*`: typed VFX node canvas, simulation
  settings, runtime preview apply, generated execution/HLSL inspection, and
  durable Material/Texture GUID validation in common editor services.
- `application/AnimationStateMachine.*` and `application/Skeleton.*`: runtime
  typed-parameter state-machine interpreter, prioritized conditions, trigger
  consumption, exit-time transitions, deterministic cross-fades, and blended
  skeleton pose application.
- `application/editor/animation/EditorAnimationStateMachine.*` and
  `application/editor/documents/EditorAnimationStateMachineDocumentProvider.*`:
  D-3 cyclic State/Transition schema, deterministic runtime-program compiler,
  durable `.animsm`/`.animstate` Asset/Document model, v1-to-v2 migration, and
  shared Transaction integration.
- `application/editor/animation/EditorAnimationStateMachinePanel.*` and
  `EditorAnimationStateMachineDiagnosticsAdapter.*`: Animation Bottom Dock,
  typed Parameter preview, state/transition authoring, generated-program view,
  and durable skinned-Mesh source GUID validation.
- `application/GameplayVisualScript.*`: editor-independent typed Gameplay VM,
  deterministic event execution, expression evaluation, variable storage,
  trace/output callbacks, and bounded instruction/depth safety contracts.
- `application/editor/gameplay/EditorGameplayVisualScript.*` and
  `application/editor/documents/EditorGameplayVisualScriptDocumentProvider.*`:
  D-3 typed Event/Exec/Data graph schema, deterministic runtime-program
  compiler, durable `.gameplay`/`.visualscript` Asset/Document model, v1-to-v2
  migration, last-known-good programs, and shared Transaction integration.
- `application/editor/gameplay/EditorGameplayVisualScriptPanel.*` and
  `EditorGameplayVisualScriptDiagnosticsAdapter.*`: Gameplay Bottom Dock,
  typed node/variable authoring, BeginPlay/Tick preview, execution trace,
  generated-program inspection, and unified compile diagnostics.
- `application/editor/EditorFontService.*`: editor-wide versioned font
  preferences, project-contained font discovery, strict path/size validation,
  atomic settings persistence, Japanese glyph-range selection, and safe ImGui
  default-font fallback.
- `application/editor/EditorFontSettingsPanel.*`: independent Bottom Dock font
  settings UI for regular/monospace faces, pixel sizes, UI scale, glyph policy,
  preview, explicit discovery refresh, and restart-required state.
- `Resources/Editor/Fonts/`: project-local home for explicitly licensed
  `.ttf`, `.otf`, and `.ttc` editor fonts. The SIL OFL 1.1 licensed M PLUS
  Rounded 1c Medium face is bundled as the Japanese-capable default, with the
  Regular face retained as an option, together with their license and
  source/checksum record.
- `application/editor/world/SceneWorldObjectProvider.*`: B-5 World Model and
  Transaction provider for Scene Entity creation, hierarchy, visibility/lock,
  duplication/deletion, and Component add/remove Undo/Redo.
- `application/editor/EditorTransformGizmoService.*` and
  `EditorTransformGizmoMath.*`: production gizmo state plus domain-neutral
  ray-axis/ray-plane constraints, signed rotation, World/Local basis
  projection, snapping, plane/uniform handles, pivot modes, and multi-selection
  telemetry.
- `application/editor/io/EditorProjectPathPolicy.*`: canonical project-root
  boundary that rejects traversal and external file mutation targets.
- `application/editor/io/EditorAtomicFileWriter.*`: same-directory durable
  staging, read-back validation, atomic replacement, backup, and rollback.
- `application/editor/io/EditorFileTransaction*`: multi-file Prepared/Commit
  journal and two-phase transaction boundary used by Asset, Course, and Layout
  persistence paths.
- `application/editor/io/EditorTrashService.*`: transaction-scoped on-disk
  trash used by large Asset delete Undo/Redo without retaining binary snapshots.
- `application/editor/io/EditorFileRecoveryService.*`: startup recovery for
  interrupted Prepared transactions and cleanup of committed write artifacts.
- `application/editor/EditorToolRegistration.*`: versioned editor tool
  descriptor registry for commands, panels, menu sections/items, and toolbar
  entries, asset providers, property accessors, validation adapters, and
  Runtime Watch providers, plus tool module startup/frame registration,
  feature gates, registration diagnostics, and command reference validation.
- `application/editor/tools/EditorInteractiveTool.h`, `EditorModeRegistry.*`,
  and `EditorToolManager.*`: E-1 domain-neutral Editor Mode and Interactive Tool
  framework. It owns mode/tool discovery, `Activate -> Preview -> Accept/Cancel`
  lifecycle, exactly-one-command authoring commits, and automatic cancellation
  across Selection, Document, Play, viewport, authoring-lock, and external
  Transaction boundaries.
- `application/editor/tools/EditorModePanels.*`: E-1 Tool Palette and Tool
  Properties surfaces. The same active-mode/tool state is also exposed through
  the main toolbar, viewport authoring overlay, and status bar.
- `application/editor/tools/EditorPlacementQueryService.*`: E-2 display-to-ray
  placement query for XZ/XY/YZ fallback planes and grid snapping, plus bounded
  distance-based brush stroke sampling.
- `application/editor/tools/EditorPlacementTools.*`: E-2 Place mode tools for
  empty Scene Entities, selected durable Assets, and multi-sample placement
  strokes. Preview submits overlay markers; Accept returns one prepared World
  command and Cancel never mutates Authoring state.
- `application/editor/terrain/EditorTerrainBrushTools.*`: E-3 Terrain mode and
  Sculpt, Smooth, Flatten, and Paint tools. Dragging writes only a transient
  preview layer; release returns one bounded stroke command and Cancel is
  authoring-mutation free.
- `application/editor/terrain/EditorTerrainSurfaceQuery.*`: shared viewport-ray
  to procedural-SDF hit query publishing rail distance, wrapped surface angle,
  world position, and normal for Terrain tools.
- `application/editor/terrain/EditorTerrainEditCommand.*`: compact generic
  Terrain stroke Undo command and execution service binding Course documents to
  the shared Transaction stack.
- `application/terrain/TerrainEditLayer.*`: durable non-destructive Terrain
  stamp storage, validation, evaluation, dirty-range calculation, per-chunk
  content hashing, and filtered asynchronous job snapshots.
- `application/terrain/TerrainVolumeField.*` and `TerrainChunkManager.*`: E-3
  runtime integration for authored/preview radial displacement, four-layer
  procedural material variation, and overlap-only chunk regeneration.
- `application/editor/geometry/EditorGeometryMesh.*`: E-4 bounded editable
  Geometry model with stable vertex/triangle GUIDs, deterministic compact
  serialization, validation, content hashing, box primitives, normals, face
  deletion, region extrusion, and source-hashed box collision generation.
- `application/editor/geometry/EditorGeometryWorkspace.*`: active Scene Mesh
  Entity binding, authoritative/preview isolation, world-transform evaluation,
  stable face selection, viewport triangle ray picking, and wire overlay data.
- `application/editor/geometry/EditorGeometryTools.*`: Modeling mode and Face
  Select, Make Editable Box, Extrude, Delete, Recalculate Normals, and Generate
  Box Collision tools using the shared Accept/Cancel lifecycle.
- `application/editor/geometry/EditorGeometryEditCommand.*`: compact property
  state command and execution service that atomically applies/undoes editable
  Geometry and generated collision on a stable Scene Entity.
- `application/editor/mesh/EditorProductionMeshAsset.*`: E-5 versioned Mesh
  source, deterministic LOD cooker, checksummed Renderer/Physics artifacts,
  strict loaders, source/build hash validation, and shared runtime resource
  cache views.
- `application/editor/mesh/EditorMeshBakePipeline.*`: side-effect-free bake
  preparation plus one rollback-safe File/Registry/Scene command for atomic new
  bake, GUID-preserving rebake, and global Undo/Redo.
- `application/editor/mesh/EditorMeshBakeTools.*`: Modeling mode Asset Pipeline
  tool exposing Asset Name, LOD count/ratios, collision mode, artifact metrics,
  Accept/Cancel, and Play/Sim locking.
- `application/editor/scene/EditorProductionScenePipeline.*`: E-6 transient
  Scene Entity render/physics instance bridge. It resolves durable Mesh GUIDs,
  evaluates hierarchy transforms, selects LODs with hysteresis, performs AABB
  frustum culling, uploads immutable LOD buffers, maintains per-instance
  transform CBVs, retires resources by frame fence, emits RenderGraph draw
  packets, and provides Box/Triangle raycast plus AABB overlap queries.
- `application/editor/material/EditorProductionMaterialPipeline.*`: E-7
  versioned Material Instance codec and transient Scene binding bridge. It
  resolves durable parent Material Graphs, material-slot overrides and shader
  variant fingerprints, owns mapped Material/Directional/Point/Spot CBVs,
  applies deterministic Light priority, preserves fallback draws, hot reloads
  timestamp changes, and retires replaced resources by frame fence.
- `application/editor/texture/EditorProductionTexturePipeline.*`: E-8 bounded
  2D Texture decode/mip generation and the transient GPU residency bridge. It
  owns shared SRV indices 3712-4031, builds physical mip-tail resources under a
  configurable budget, resolves color/normal descriptors, preserves fallback
  draws, performs timestamp hot reload and deterministic inactive LRU eviction,
  and returns resources, upload buffers, and descriptor slots only after the
  scheduled frame fence completes.
- `application/editor/shader/EditorProductionShaderPipeline.*`: E-9 stable
  Surface shader permutation identity, atomic generated HLSL, asynchronous DXC
  jobs, frame-thread PSO construction, persistent D3D12 Pipeline Library,
  Opaque/Masked/Translucent and Lit/Unlit state, E-8 normal-map specialization,
  last-known-good promotion, bounded inactive eviction, and fence-safe PSO
  retirement.
- `Resources/ProductionMaterial.PS.hlsl`: E-9 Object3D pixel-shader template
  with a generated Material Graph insertion point, normal-map derivative TBN,
  blend/shading compile switches, and the E-10 clustered Light/shadow sampling
  contract.
- `application/editor/lighting/EditorProductionLightingPipeline.*`: E-10
  Scene-View-owned transient lighting bridge. It priority-sorts and bounds all
  Directional/Point/Spot components, builds 64-pixel tile and logarithmic depth
  cluster index buffers, publishes root-SRV/CBV addresses, allocates a
  budgeted D32 texture-array shadow atlas at shared SRV index 4032, renders
  Directional/Spot casters, and exposes overflow/budget telemetry.
- `Resources/ProductionShadow.VS.hlsl`: E-10 depth-only Production Scene shadow
  vertex shader using per-slice light view-projection constants.
- `application/editor/visibility/EditorProductionGpuDrivenPipeline.*`: E-11
  deterministic Production Scene mesh/material batching, bounded instance and
  batch policy, triple-buffered fence-safe instance/argument/count/readback
  resources, compute visibility dispatch, `ExecuteIndirect`, command-layout
  validation, CPU overflow fallback, and Runtime Watch telemetry.
- `Resources/ProductionVisibility.CS.hlsl`: E-11 batch-count reset, GPU sphere
  frustum and conservative Terrain Hi-Z occlusion tests, visible-command
  compaction, and the 32-byte Transform-CBV + DrawIndexed command contract.
- `application/AppEditorToolModules.*`: App-level built-in editor module
  pipeline that registers startup descriptors, Details sections, provider
  adapters, Runtime Watch providers, command providers, menus, and toolbar
  contributions without keeping those wiring details in `AppImGuiLayer`.
- `application/editor/sequencer/EditorSequencer.*`: D-1 domain-independent
  Sequencer track/key model and service for provider registration, multi-key
  selection, interactive preview/commit/cancel, snapping, clipboard,
  runtime-preview scrubbing, and rollback-safe generic Undo/Redo transactions.
- `application/editor/course/CourseSequencerTrackProvider.*`: D-1 Course adapter
  exposing Event, Placement, Camera, Lighting, Material, VFX, and Gameplay
  Trigger tracks through persistent editor GUIDs without leaking Course types
  into Sequencer Core.
- `application/editor/EditorProductionPropertyAdapter.*`: production Details
  adapter for non-Course-object authoring targets such as VFX assets,
  post-process passes, course camera keys, terrain generation, camera rig, and
  gameplay tuning.
- `engine/`: reusable engine modules split into `include/` and `src/`.
- `Resources/`: runtime assets, shaders, models, textures, audio, and effect definitions.
- `externals/`: vendored third-party dependencies.
- `lib/`: small library integrations that are kept separate from the engine/application layers.
- `docs/`: project notes and technical documentation.
- `docs/EditorCoreDesign.md`: shared editor architecture plan for incremental
  Unreal-style editor features while preserving existing VFX and authoring
  tools.
- `docs/debug/`: debugging notes and investigation records.
- `capture/screenshots/`: UI and runtime verification screenshots.
- `capture/media/`: captured or sample media files used during experiments.
- `logs/`: local runtime logs and telemetry output. This directory is ignored by Git.
- `generated/`: generated verification outputs that were intentionally kept in the repository.
- `実装メモ/`: implementation notes.

## Notes

Several legacy/prototype `.cpp` and `.h` files still live at the repository root because `GE3.vcxproj` currently compiles them from there. Move these in a separate refactor so the Visual Studio project file, filters, and include paths can be updated and tested together.
