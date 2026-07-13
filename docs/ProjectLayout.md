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
- `application/editor/EditorRuntimeWatchBuilder.*`: read-only Runtime Watch row
  builder for Play/Sim state, selection, Course runtime, VFX, gameplay systems,
  and RenderGraph coverage.
- `application/editor/EditorAutomationGate.*`: commercial editor readiness gate
  runner that aggregates regression/smoke checks, commercial recovery scenarios,
  Feature Guard checks, performance budgets, and structured JSON/Markdown
  automation reports under `logs/`.
- `application/editor/EditorToolRegistration.*`: versioned editor tool
  descriptor registry for commands, panels, menu sections/items, and toolbar
  entries, asset providers, property accessors, validation adapters, and
  Runtime Watch providers, plus tool module startup/frame registration,
  feature gates, registration diagnostics, and command reference validation.
- `application/AppEditorToolModules.*`: App-level built-in editor module
  pipeline that registers startup descriptors, Details sections, provider
  adapters, Runtime Watch providers, command providers, menus, and toolbar
  contributions without keeping those wiring details in `AppImGuiLayer`.
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
