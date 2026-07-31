# Asset Placement and Loader Guide

Last verified: 2026-07-31

## Purpose

This guide is the recording-ready procedure for bringing an external asset into
the current GE3 editor, converting a source mesh to the runtime format, and
placing it in an Editor Scene. It also records which loader owns each stage so
that `Import`, `Import & Bake Mesh`, and `Place Selected Asset` are not confused.

## Current flow

```text
External source
  -> Content Browser / Production Import
  -> Resources/<Destination>/<source file> + .meta
  -> Asset Registry (durable GUID)
  -> Mesh Production Import (Assimp + bake)
  -> Resources/Generated/Imported/<name>_production.mesh
       + .mesh.cooked
       + .mesh.collision
       + .mesh.meta
  -> Place Selected Asset
  -> Scene Entity
       + engine.transform
       + engine.mesh-renderer (production Mesh GUID)
  -> Production Mesh Runtime Cache
  -> Production Scene renderer / physics
```

The source file is an authoring input. The generated `.mesh` and its cooked
artifacts are the durable runtime input used by production Scene placement.

## Recording procedure: import and place a mesh

Use `.glb` for the shortest and least error-prone demonstration because it can
contain mesh data and textures in one file.

1. Start the Development editor with a working directory that contains the
   matching `Resources/` folder. The build output is
   `../generated/outputs/Development/GE3.exe`.
2. Open and activate a Scene document. Placement is disabled when the active
   document is not a Scene or while authoring is locked by Play/Sim.
3. Open `Content Browser`, then locate the `Production Import` section.
4. Set `Destination` to a relative folder such as `Imported/Demo`.
   Do not enter `Resources/` in this field; the editor adds that root.
5. Keep `Collision` at `Rename` for a safe recording. `Skip` rejects an existing
   destination and `Overwrite` replaces the copied source file.
6. Press `Import...`, choose an `.obj`, `.gltf`, `.glb`, or `.fbx`, and confirm.
   The editor copies the source to `Resources/<Destination>/` and creates a
   sidecar `.meta` containing a durable GUID.
7. In Content Browser, select the imported source Mesh.
8. Press `Import & Bake Mesh`. The selected source is converted into a durable
   production Mesh under `Resources/Generated/Imported/`. The generated Mesh is
   selected automatically. This explicit step is best for a video because its
   success notification and generated asset are visible.
9. Switch to `Place` mode from `Tool Palette`, or press `Shift+3`.
10. Activate `Place Selected Asset`. Set `Grid Snap`, `Placement Plane`,
    `Grid Size`, `Max Distance`, `Rotation Y`, and `Uniform Scale` as needed.
11. Click once in the Viewport. One Entity is committed as one undoable
    transaction. `Esc` cancels without changing the Scene.
12. Check the new Entity in World Outliner and confirm in Details that it has
    `Transform` and `Mesh Renderer` components.
13. Press the toolbar `Save` button (`Save All`, `Ctrl+Shift+S`) before Play.

`Place Selected Asset` can also accept the source OBJ/glTF/GLB/FBX directly.
On activation it looks for the matching generated production Mesh and reuses
it, or runs the same import-and-bake bridge automatically. The explicit bake in
step 8 is retained in the recording procedure because it makes the two phases
observable and makes failures easier to diagnose.

For repeated placement, activate `Placement Brush` instead. Dragging in the
Viewport samples positions; releasing commits the whole stroke as one
transaction. A single stroke is limited to 1024 Entities.

## What each button does

| Operation | Input | Output / responsibility |
| --- | --- | --- |
| `Import...` / `Import` | External asset file | Copies one selected file into `Resources/<Destination>/`, creates or preserves `.meta`, and registers it in Asset Registry. It does not cook a source mesh. |
| `Reimport` | Selected registered asset | Re-reads the in-project source and preserves its durable GUID. |
| `Batch Meta` | Legacy assets under `Resources/` | Creates durable `.meta` files for provisional assets. |
| `Import & Bake Mesh` | Registered OBJ/glTF/GLB/FBX source Mesh | Uses Assimp, converts to editable geometry, builds up to 3 LODs by default, builds Box collision by default, and writes the production `.mesh` artifacts. |
| `Place Selected Asset` | Selected durable Mesh/Effect/Audio/BehaviorTree asset | Creates an Entity with Transform plus the component mapped to that asset kind. Source meshes are first resolved to a production `.mesh`. |

The current Scene component mappings are:

| Asset kind | Scene component |
| --- | --- |
| `Mesh` | `engine.mesh-renderer` |
| `Effect` | `engine.vfx` |
| `Audio` | `engine.audio-source` |
| `BehaviorTree` | `editor.ai-agent` |

Textures, Course assets, and arbitrary JSON files are visible to Asset Registry
but are not directly placeable because no Scene component mapping exists for
those kinds.

## Loader ownership

### Mesh source loader

- Entry point: `LoadObjFile_Assimp(directory, filename)` in
  `application/ModelLoaderAssimp.cpp`.
- Source formats accepted by the production bridge: OBJ, glTF, GLB, and FBX.
- Assimp processing currently triangulates, converts to left-handed coordinates,
  and generates smooth normals.
- The production bridge is
  `application/editor/mesh/EditorObjProductionImportBridge.cpp`.
- Runtime validation and loading of `.mesh`, `.mesh.cooked`, and
  `.mesh.collision` is owned by `EditorProductionMeshRuntimeCache` in
  `EditorProductionMeshAsset.cpp`.

The Asset Registry recognizing a source format is not the same as the runtime
being able to draw that source directly. Production placement requires a
durable `.mesh` with all matching artifacts.

### Effect loader

- Put authored effect definitions directly under `Resources/effects/` with the
  `.effect` extension.
- `VfxEngine` scans that single directory at startup and calls
  `EffectAssetLoader::LoadFile` for each file.
- Effect authoring samples are skipped unless their authoring-sample policy is
  enabled.
- Runtime file polling is off by default. Set `GE3_EFFECT_HOT_RELOAD=1` before
  launch to reload changed and newly added `.effect` files.
- A `.meta` file makes an Effect a durable Asset Registry entry for Scene
  placement, but `EffectAssetLoader` reads the `.effect` definition itself.

An effect texture name is resolved from the VFX resource cache. Merely importing
a PNG into Content Browser does not automatically add it to that cache. The
current named VFX texture list is declared in `AppSceneResources.cpp`; a new
runtime texture must be added to that load list (or registered through a future
dynamic resource path) before an effect can refer to it by name.

### Texture and audio loaders

- Asset Registry recognizes PNG, BMP, DDS, JPG/JPEG, and TGA as Texture assets.
- WIC is used for common image formats; DDS uses the DirectXTex DDS path.
- Asset Registry recognizes WAV, MP3, and OGG as Audio assets. The native import
  dialog also displays FLAC, but the current classifier does not accept FLAC;
  do not use FLAC in the recording.

### Blender Level JSON loader

Blender Level JSON is a Scene-import workflow, not `Place Selected Asset`.

1. Export schema v1 JSON and keep it under a known project location such as
   `Resources/Levels/Blender/`.
2. Open and activate a Scene document.
3. Run `Scene > Import Blender Level JSON...`.
4. Use `Scene > Reimport Blender Level JSON...` for later updates.

`BlenderLevelJsonLoader` validates JSON and the v1 interchange schema. The
Scene import service then creates an anchor Entity, child Entities, transforms,
spawn data, colliders, and resolvable Mesh references. Reimport identity is
`(scene_guid, object_guid)`, not an object display name. A playable import must
contain exactly one Player spawn.

## Dependency placement rules

The external import operation copies only the selected main file and an
existing `.meta` sidecar. It does not recursively copy model dependencies.

- OBJ: place the `.obj`, referenced `.mtl`, and referenced textures so their
  relative paths remain valid.
- glTF: place the `.gltf`, referenced `.bin`, and referenced image files so
  their relative paths remain valid.
- GLB: preferred for recording because external binary dependencies can be
  embedded.
- FBX: embedded media is safest; otherwise copy referenced media manually.

If a multi-file asset is outside the project, first copy its whole directory
tree under `Resources/Imported/<AssetName>/`, then restart the editor so the
startup folder indexer registers it. Use the Content Browser `Import...` button
only when the selected file is self-contained or its dependencies have already
been placed at the destination.

## Failure checklist

- Asset does not appear: confirm it is under the active `Resources/` root and
  restart the editor; the folder index is built during editor initialization.
- `Place Selected Asset` is disabled: activate a Scene document, stop Play/Sim,
  and select a referenceable Asset with a durable GUID.
- Source Mesh cannot be placed: use `Batch Meta`, then run
  `Import & Bake Mesh`; confirm all four generated files exist.
- Mesh is empty or untextured: confirm the source dependencies and their
  relative paths. The production bake retains geometry, UVs, normals, and
  material-slot indices, but material/texture authoring is a separate pipeline.
- Effect loads but uses a fallback texture: confirm the texture name is present
  in the VFX resource cache, not only in Content Browser.
- Effect edit does not reload: relaunch, or launch with
  `GE3_EFFECT_HOT_RELOAD=1`.
- Blender JSON import is disabled: open a Scene document first and stop Play/Sim.

## Verification status

The Development solution built successfully with the installed Visual Studio 18
v145 MSBuild and produced `GE3.exe` plus the three engine libraries.

The official `tools/build.ps1` prerequisite gate currently stops before build
because the pinned dependency lock does not match the installed MSVC servicing
binary and the current Assimp/DirectXTex license-file hashes. The dependency
lock was not changed during this verification.

Relevant tests on this working tree:

- PASS: Effect authoring smoke.
- PASS: production placement brush tool pack.
- PASS: asset import/reimport pipeline.
- PASS: OBJ production import/bake bridge.
- PASS: multi-material model loading.
- FAIL: production Mesh editable-source loader retained-geometry regression.
- FAIL: hand-particle attachment regression.
- FAIL: multi-material showcase presentation defaults.

The standard source import, production bake, and placement path is covered by
passing tests, but the full Editor Core suite is not currently clean. Do not
present the three failing optional demonstrations as verified until their
regressions are resolved.
