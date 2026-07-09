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
