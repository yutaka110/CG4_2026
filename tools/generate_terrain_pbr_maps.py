"""Generate deterministic terrain ORM and height maps from the authored albedo set.

The generated files are runtime assets, not shader fallbacks:
  ORM.R = ambient occlusion
  ORM.G = perceptual roughness
  ORM.B = metallic
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import cv2
import numpy as np


ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "Resources" / "course_meshes" / "materials"
OUTPUT_DIR = ROOT / "Resources" / "terrain" / "materials" / "textures"
SIZE = 512


@dataclass(frozen=True)
class MaterialRecipe:
    name: str
    source: str
    base_roughness: float
    height_contrast: float
    strata: float
    seed: float


RECIPES = (
    MaterialRecipe("dry_strata", "organic_rock_albedo.bmp", 0.79, 0.74, 0.25, 1.7),
    MaterialRecipe("wet_organic", "root_rock_albedo.bmp", 0.38, 0.88, 0.12, 4.3),
    MaterialRecipe("floor_sand", "rib_rock_albedo.bmp", 0.91, 0.52, 0.07, 8.1),
)


def normalize(values: np.ndarray) -> np.ndarray:
    low = float(np.percentile(values, 1.0))
    high = float(np.percentile(values, 99.0))
    return np.clip((values - low) / max(high - low, 1.0e-6), 0.0, 1.0)


def periodic_detail(recipe: MaterialRecipe) -> np.ndarray:
    axis = np.arange(SIZE, dtype=np.float32) / float(SIZE)
    u, v = np.meshgrid(axis, axis)
    tau = np.float32(np.pi * 2.0)
    detail = (
        np.sin(tau * (u * 5.0 + v * 2.0) + recipe.seed) * 0.31
        + np.sin(tau * (u * 13.0 - v * 7.0) + recipe.seed * 2.3) * 0.19
        + np.cos(tau * (u * 29.0 + v * 17.0) - recipe.seed * 1.1) * 0.10
        + np.sin(tau * (u * 61.0 - v * 47.0) + recipe.seed * 3.7) * 0.05
    )
    if recipe.strata > 0.0:
        warped_v = v + np.sin(tau * u * 3.0 + recipe.seed) * 0.018
        strata = np.cos(tau * warped_v * 18.0)
        detail += np.sign(strata) * np.power(np.abs(strata), 5.0) * recipe.strata
    return detail


def build_maps(recipe: MaterialRecipe) -> tuple[np.ndarray, np.ndarray]:
    source = cv2.imread(str(SOURCE_DIR / recipe.source), cv2.IMREAD_COLOR)
    if source is None:
        raise FileNotFoundError(SOURCE_DIR / recipe.source)

    source = cv2.resize(source, (SIZE, SIZE), interpolation=cv2.INTER_CUBIC)
    linear_rgb = np.power(source[..., ::-1].astype(np.float32) / 255.0, 2.2)
    luminance = (
        linear_rgb[..., 0] * 0.2126
        + linear_rgb[..., 1] * 0.7152
        + linear_rgb[..., 2] * 0.0722
    )
    broad = cv2.GaussianBlur(
        luminance, (0, 0), 18.0, borderType=cv2.BORDER_REFLECT_101
    )
    medium = cv2.GaussianBlur(
        luminance, (0, 0), 4.0, borderType=cv2.BORDER_REFLECT_101
    )
    height = (
        broad * 0.48
        + (luminance - broad) * recipe.height_contrast
        + (luminance - medium) * 0.28
        + periodic_detail(recipe)
    )
    height = normalize(height)

    local_ao = np.ones_like(height)
    for sigma, weight in ((2.0, 0.36), (7.0, 0.32), (20.0, 0.22)):
        neighborhood = cv2.GaussianBlur(
            height, (0, 0), sigma, borderType=cv2.BORDER_REFLECT_101
        )
        local_ao -= np.maximum(neighborhood - height, 0.0) * weight * 3.4
    local_ao = np.clip(local_ao, 0.28, 1.0)

    micro = cv2.Laplacian(height, cv2.CV_32F, ksize=3)
    roughness = (
        recipe.base_roughness
        + np.abs(micro) * 0.22
        + (periodic_detail(recipe) * 0.5 + 0.5) * 0.07
    )
    if recipe.name == "wet_organic":
        roughness -= np.maximum(height - 0.52, 0.0) * 0.22
    roughness = np.clip(roughness, 0.16, 0.98)

    height_rgba = np.empty((SIZE, SIZE, 4), dtype=np.uint8)
    height_byte = np.rint(height * 255.0).astype(np.uint8)
    height_rgba[..., :3] = height_byte[..., None]
    height_rgba[..., 3] = 255

    orm = np.empty((SIZE, SIZE, 4), dtype=np.uint8)
    orm[..., 0] = np.rint(local_ao * 255.0).astype(np.uint8)
    orm[..., 1] = np.rint(roughness * 255.0).astype(np.uint8)
    orm[..., 2] = 0
    orm[..., 3] = 255
    return orm, height_rgba


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    for recipe in RECIPES:
        orm, height = build_maps(recipe)
        # OpenCV writes BGRA. ORM must remain RGB = AO, roughness, metallic.
        cv2.imwrite(str(OUTPUT_DIR / f"{recipe.name}_orm.png"), orm[..., [2, 1, 0, 3]])
        cv2.imwrite(str(OUTPUT_DIR / f"{recipe.name}_height.png"), height)
        print(f"generated {recipe.name}: {SIZE}x{SIZE} ORM + height")


if __name__ == "__main__":
    main()
