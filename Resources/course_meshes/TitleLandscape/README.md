# Original title landscape

Generated from `tools/generate_title_landscape.py`; no external art or sampled textures.

- `TitleGround.obj`: one connected closed disk, radius 300 m. The rail/camera corridor (radius 60–92 m) is flat at y=-0.30 m, just below the sleepers. Broad shallow undulation appears away from the track. The outer skirt closes underneath the ground.
- `TitleCliff.obj`: a closed plateau with broad irregular contours and gentle taper, reused at three clearly separated locations.
- `TitleBoulder.obj`: a closed rounded rock used at five sparse locations away from the track and camera.
- `title_sandstone.bmp`: original solid sandstone albedo. The title materials disable specular/environment highlights to avoid a noisy surface.

The generator verifies every edge is shared by exactly two oppositely wound faces, rejects degenerate triangles, and verifies outward-facing shells. Shared vertices and area-weighted normals preserve continuous surfaces at the radial seam.

Regenerate from the repository root: `python tools/generate_title_landscape.py`.
The C++ regression imports these exact OBJ assets and checks ground support around both rails plus camera visibility over a full lap and repeated start transitions.

## Imported surface and title shading checks

Title meshes are audited after Assimp import as well: welded-position edges must form closed, consistently oriented solids without duplicate or degenerate triangles. Invalid title meshes are rejected before GPU upload. The generator also checks face normals against the stored smooth normals, preventing isolated winding repairs.

Material mode 6 is reserved for title landscape: diffuse multiplier 0.62–0.82, no point/spot/specular/environment contribution, and smooth distance haze between 40 and 220 m. Haze blends towards linear RGB (0.30, 0.36, 0.40), matching the title background. Cart materials retain their normal lighting path. The title restores gameplay lighting/background on handoff.
