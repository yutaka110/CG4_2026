# Submission obstacle assets

Regenerate the three authored meshes and their palette textures with:

```text
python tools/generate_submission_hazards.py
```

- `rail_hazard_block`: chipped stone with dark cracks and amber fracture seams; breakable obstacles.
- `rail_hazard_solid`: grey stone without fracture seams; non-breakable ceiling and roadside collision placements.
- `combat_turret`: octagonal base, twin barrels and inset amber muzzles; stationary enemies and the larger gatekeeper.

Both stone models retain local bounds [-1, 1] on every axis. Runtime half-extents determine their size. Small corner bevels use the existing conservative box collision; positions, health, damage and firing timing are unchanged. The turret is authored facing +Z and imported by Assimp into the game's -Z-facing convention. It includes its barrels, so the generic drone pod duplication is skipped.

Every submesh has an explicit OBJ/MTL/BMP material. These models do not depend on sample textures. Palette bands have padding for linear filtering. The main geometry shader's specular mode 2 keeps diffuse lighting and disables specular highlights; course stone also disables environment reflections so its cracks remain visible.

Validation: `--editor-core-regression`, case `submission obstacle readability and clearance`, covers asset/material loading, local bounds, enemy model references, actual obstacle firing rays, destruction, warning removal and safe passage. Visual inspection must additionally check the models in the running game.
