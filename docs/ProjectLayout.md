# Project Layout

This repository is organized around the following top-level areas:

- `application/`: application-level systems, panels, render pipelines, runtime state, and VFX code.
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
