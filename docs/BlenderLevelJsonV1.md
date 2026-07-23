# Blender Level JSON v1

`BlenderLevel.schema.json` is the machine-readable contract between the
Blender level exporter and GE3. The interchange file is editor input; the
native `.scene` document remains the authoring source consumed by the existing
Scene pipeline after import.

## Root contract

- `schema_version`: must be `1`.
- `scene_guid`: persistent lowercase 32-digit hexadecimal Blender Scene ID.
- `name`: Blender Scene name.
- `coordinate_system`: declares Blender source coordinates explicitly.
- `objects`: root objects. Children are nested recursively.

Blender data is right-handed, Z-up, forward `-Y`. Transforms are local to the
parent. Translation and scale are numeric triples; rotation is XYZ Euler in
degrees. `unit_scale_meters` is copied from Blender Scene unit settings.

## Object contract

Every exported object has:

- `guid`: persistent lowercase 32-digit hexadecimal Blender Object ID.
- `type`: Blender object type such as `EMPTY` or `MESH`.
- `name`: Blender object name.
- `spawn_kind`: `NONE`, `PLAYER`, or `ENEMY`.
- `transform`: local translation, rotation, and scaling.

An `ENEMY` object must additionally have `enemy_type`, currently one of
`DRONE`, `TURRET`, or `BOSS`.

Existing optional `file_name`, BOX `collider`, and recursive `children` fields
remain supported.

## Identity and reimport

The exporter creates `ge3_guid` custom properties for the Blender Scene and
every Object. Missing, invalid, or duplicated Object IDs are repaired during
export. Importers must use `(scene_guid, object guid)` as source identity and
must not use the display name as identity.

## Validation policy

The exporter rejects unsupported spawn kinds, unsupported enemy types, more
than one Player marker, invalid floating-point values, and write failures. A
scene without a Player marker remains exportable for backward compatibility,
but produces a warning. The C++ gameplay-level importer must require exactly
one Player marker when importing a playable level.

The authoritative schema is:

`Resources/Levels/Blender/BlenderLevel.schema.json`

The fixed loader fixture is:

`Resources/Levels/Blender/sample_level_v1.json`

## C++ loader

`application/level/BlenderLevelJsonLoader.h` exposes the dependency-free
`ge3::level::BlenderLevelJsonLoader`.

Use `LoadFile(path)` for an exported level file. `LoadJsonString(json, source)`
is available for tests and in-memory input. A successful result contains a
typed `BlenderLevelData`; a failed result contains an error code, source path,
JSON path, line, column, and message.

The loader rejects malformed UTF-8/JSON, duplicate JSON members, unknown v1
properties, unsupported enum values, invalid or duplicate GUIDs, non-finite or
out-of-range values, and configured resource-limit violations. It validates
the interchange contract only. Playability rules such as requiring exactly
one Player marker belong to the later scene-import service.

## EditorScene Import and Reimport

`application/editor/scene/EditorBlenderSceneImportService.h` connects the
loader output to the native `EditorScene` model. `ImportFile`, `ReimportFile`,
and `ImportOrReimportFile` are the file-facing entry points. Equivalent typed
data entry points are available for tests and higher-level tools.

Each imported Blender Scene receives a deterministic anchor Entity with an
`editor.blender-scene-source` Component. Every source Object becomes an Entity
with an `editor.blender-object-source` Component. Reimport matches these
records by `(scene_guid, object_guid)` instead of display name.

The conversion is:

- Blender `(X, Y, Z)` position to GE3 `(X, Z, -Y)`.
- Blender XYZ degrees to basis-converted GE3 XYZ radians.
- Blender scale `(X, Y, Z)` to GE3 `(X, Z, Y)`.
- Position and BOX Collider dimensions are multiplied by
  `unit_scale_meters * worldUnitsPerMeter`.
- `PLAYER`/`ENEMY` metadata becomes `gameplay.spawn-point`.
- BOX Collider metadata becomes `engine.box-collider`.
- A resolvable `file_name` becomes an `engine.mesh-renderer` Asset reference.

Import and Reimport operate on a Scene copy and publish it only after native
Scene validation succeeds. Reimport updates Blender-managed data, creates new
source Objects, removes missing source Objects, and preserves user Components
and user-owned child Entities. The default playable-level policy requires
exactly one Player spawn.
