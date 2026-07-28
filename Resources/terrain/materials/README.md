# Terrain PBR material set

`default.terrainmaterialset` lists exactly three runtime layers in this order:

1. dry wall rock
2. wet or cavity rock
3. upward-facing floor sediment

Each `.terrainmaterial` can reference PNG, JPEG, BMP, or DDS files. Relative
paths are resolved from the material definition file. Runtime loading converts
every source to a 512 x 512 mipmapped texture and packs matching maps into a
three-slice `Texture2DArray`.

Map conventions:

- `baseColor`: sRGB color without baked lighting
- `normal`: linear tangent-space normal, +Y convention by default
- `orm`: linear channels R=ambient occlusion, G=roughness, B=metallic
- `height`: linear height in the red channel, neutral height 0.5

Surface-mask input is selected per material:

- `ormInputMode=packed` reads the `orm` texture directly.
- `ormInputMode=separate` reads the red channels of `ambientOcclusion`,
  `roughness`, and `metallic`, then packs them into ORM during import.
- Empty separate inputs use AO=1, roughness=1, and metallic=0.

Definitions without `ormInputMode` remain backward compatible and use packed
ORM. Separate source maps and packed ORM are retained together so artists can
switch modes in the Terrain PBR Materials panel without losing either setup.

Missing maps do not prevent startup. Base color receives a layer-specific rock
fallback, normal receives a flat normal, ORM receives AO=1/roughness=1/metal=0,
and height receives 0.5. This makes it safe to replace maps independently while
authoring.

The checked-in ORM and height assets are regenerated deterministically from the
authored albedo sources with `python tools/generate_terrain_pbr_maps.py`.

For shipping assets, prefer BC7 sRGB for base color, BC5 for normal, BC7 or BC1
for ORM, and BC4 for height. DDS files should include production mipmaps.
