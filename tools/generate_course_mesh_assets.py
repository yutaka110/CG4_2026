from __future__ import annotations

import math
import os
import random
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MESH_ROOT = ROOT / "Resources" / "course_meshes"


def normalize(v):
    x, y, z = v
    length = math.sqrt(x * x + y * y + z * z)
    if length <= 1.0e-6:
        return (0.0, 1.0, 0.0)
    return (x / length, y / length, z / length)


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def noise2(x, y, seed=0):
    value = math.sin(x * 12.9898 + y * 78.233 + seed * 37.719) * 43758.5453
    return value - math.floor(value)


class Mesh:
    def __init__(self, name: str, material: str):
        self.name = name
        self.material = material
        self.vertices = []
        self.uvs = []
        self.normals = []
        self.faces = []

    def add_vertex(self, pos, uv, normal):
        self.vertices.append(pos)
        self.uvs.append(uv)
        self.normals.append(normalize(normal))
        return len(self.vertices)

    def add_face(self, a, b, c):
        self.faces.append((a, b, c))

    def add_grid(self, fn, u_count: int, v_count: int, flip=False):
        ids = []
        positions = []
        for y in range(v_count + 1):
            row = []
            pos_row = []
            v = y / v_count
            for x in range(u_count + 1):
                u = x / u_count
                pos = fn(u, v)
                row.append(None)
                pos_row.append(pos)
            ids.append(row)
            positions.append(pos_row)

        for y in range(v_count + 1):
            for x in range(u_count + 1):
                xm = max(0, x - 1)
                xp = min(u_count, x + 1)
                ym = max(0, y - 1)
                yp = min(v_count, y + 1)
                du = sub(positions[y][xp], positions[y][xm])
                dv = sub(positions[yp][x], positions[ym][x])
                normal = normalize(cross(dv, du) if flip else cross(du, dv))
                ids[y][x] = self.add_vertex(
                    positions[y][x],
                    (x / u_count, 1.0 - y / v_count),
                    normal,
                )

        for y in range(v_count):
            for x in range(u_count):
                a = ids[y][x]
                b = ids[y][x + 1]
                c = ids[y + 1][x + 1]
                d = ids[y + 1][x]
                if flip:
                    self.add_face(a, c, b)
                    self.add_face(a, d, c)
                else:
                    self.add_face(a, b, c)
                    self.add_face(a, c, d)

    def write(self, directory: Path, filename: str, mtl_name: str):
        directory.mkdir(parents=True, exist_ok=True)
        path = directory / filename
        with path.open("w", encoding="utf-8", newline="\n") as out:
            out.write(f"mtllib {mtl_name}\n")
            out.write(f"o {self.name}\n")
            for x, y, z in self.vertices:
                out.write(f"v {x:.6f} {y:.6f} {z:.6f}\n")
            for u, v in self.uvs:
                out.write(f"vt {u:.6f} {v:.6f}\n")
            for x, y, z in self.normals:
                out.write(f"vn {x:.6f} {y:.6f} {z:.6f}\n")
            out.write(f"usemtl {self.material}\n")
            for a, b, c in self.faces:
                out.write(f"f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c}\n")
        return path


def write_bmp(path: Path, width: int, height: int, sampler):
    path.parent.mkdir(parents=True, exist_ok=True)
    row_stride = ((width * 3 + 3) // 4) * 4
    pixel_data = bytearray()
    for y in range(height - 1, -1, -1):
        row = bytearray()
        for x in range(width):
            r, g, b = sampler(x / max(1, width - 1), y / max(1, height - 1))
            row += bytes((max(0, min(255, int(b))), max(0, min(255, int(g))), max(0, min(255, int(r)))))
        row += b"\x00" * (row_stride - width * 3)
        pixel_data += row

    header_size = 14 + 40
    file_size = header_size + len(pixel_data)
    with path.open("wb") as out:
        out.write(b"BM")
        out.write(struct.pack("<IHHI", file_size, 0, 0, header_size))
        out.write(struct.pack("<IIIHHIIIIII", 40, width, height, 1, 24, 0, len(pixel_data), 2835, 2835, 0, 0))
        out.write(pixel_data)


def rock_sampler(base):
    def sample(u, v):
        strata = 0.5 + 0.5 * math.sin(v * 54.0 + math.sin(u * 18.0) * 2.0)
        grain = noise2(u * 31.0, v * 37.0, 3)
        cavity = noise2(u * 92.0, v * 88.0, 9)
        shade = 0.68 + strata * 0.18 + grain * 0.12 - cavity * 0.10
        cool = 0.10 * (1.0 - v)
        return (
            base[0] * shade + 18 * cool,
            base[1] * shade + 26 * cool,
            base[2] * shade + 34 * cool,
        )
    return sample


def write_mtl(directory: Path, name: str, texture: str):
    with (directory / f"{name}.mtl").open("w", encoding="utf-8", newline="\n") as out:
        out.write(f"newmtl {name}\n")
        out.write("Ka 0.18 0.16 0.14\n")
        out.write("Kd 0.78 0.66 0.54\n")
        out.write("Ks 0.10 0.09 0.08\n")
        out.write("Ns 24.0\n")
        out.write(f"map_Kd ../materials/{texture}\n")


def eroded(u, v, strength=0.035, seed=0):
    return (noise2(u * 6.0, v * 9.0, seed) - 0.5) * strength


def make_organic_arch():
    mesh = Mesh("OrganicArchLarge_HD", "organic_rock")
    outer, inner, base_y, depth = 1.05, 0.42, -0.72, 0.24

    def arch_surface(z, flip):
        mesh.add_grid(
            lambda u, v: (
                math.cos(math.pi * (1.0 - u)) * (inner + (outer - inner) * v),
                base_y + math.sin(math.pi * (1.0 - u)) * (inner + (outer - inner) * v),
                z + eroded(u, v, 0.04, 11),
            ),
            80,
            14,
            flip=flip,
        )

    arch_surface(-depth, False)
    arch_surface(depth, True)

    for side_x0, side_x1 in [(-outer, -inner), (inner, outer)]:
        mesh.add_grid(
            lambda u, v, x0=side_x0, x1=side_x1: (
                x0 + (x1 - x0) * u,
                -1.0 + (base_y + 0.10) * v,
                -depth + depth * 2.0 * (0.5 + eroded(u, v, 0.08, 13)),
            ),
            12,
            32,
        )

    for x in (-outer, outer):
        mesh.add_grid(
            lambda u, v, sx=x: (
                sx + eroded(u, v, 0.03, 17),
                -1.0 + 2.0 * v,
                -depth + 2.0 * depth * u,
            ),
            8,
            50,
        )
    return mesh


def make_rib_tunnel():
    mesh = Mesh("RibTunnelWall_HD", "rib_rock")
    length = 2.0
    mesh.add_grid(
        lambda u, v: (
            math.cos(math.radians(18.0 + 144.0 * u)) * (0.74 + 0.10 * math.sin(v * math.pi * 11.0)),
            -0.80 + math.sin(math.radians(18.0 + 144.0 * u)) * (0.74 + 0.10 * math.sin(v * math.pi * 11.0)),
            -length * 0.5 + length * v + eroded(u, v, 0.025, 21),
        ),
        72,
        96,
    )
    return mesh


def make_root_spire():
    mesh = Mesh("RootSpireColumn_HD", "root_rock")
    rings, slices = 86, 56
    ids = []
    for i in range(rings + 1):
        t = i / rings
        y = -1.0 + 2.0 * t
        twist = t * 2.4
        radius = 0.30 * (1.0 - t) + 0.14 * t + 0.035 * math.sin(t * 31.0)
        row = []
        for j in range(slices):
            a = (j / slices) * math.tau + twist
            lobe = 1.0 + 0.18 * math.sin(a * 3.0 + t * 9.0) + 0.08 * math.sin(a * 7.0)
            r = radius * lobe
            x = math.cos(a) * r
            z = math.sin(a) * r
            n = normalize((math.cos(a), 0.12 * math.sin(t * math.pi), math.sin(a)))
            row.append(mesh.add_vertex((x, y, z), (j / slices, 1.0 - t), n))
        ids.append(row)
    for i in range(rings):
        for j in range(slices):
            a = ids[i][j]
            b = ids[i][(j + 1) % slices]
            c = ids[i + 1][(j + 1) % slices]
            d = ids[i + 1][j]
            mesh.add_face(a, b, c)
            mesh.add_face(a, c, d)
    for root_index, angle in enumerate((0.35, 2.45, 4.35)):
        mesh.add_grid(
            lambda u, v, a=angle: (
                math.cos(a + u * 0.45) * (0.20 + v * 0.78),
                -0.92 + u * 0.55,
                math.sin(a + u * 0.45) * (0.20 + v * 0.78),
            ),
            36,
            5,
        )
    return mesh


def make_curved_wall():
    mesh = Mesh("CurvedCanyonWall_HD", "organic_rock")
    mesh.add_grid(
        lambda u, v: (
            -1.0 + 2.0 * u,
            -1.0 + 2.0 * v,
            0.28 * math.cos((u - 0.5) * math.pi) + eroded(u, v, 0.08, 31) + 0.03 * math.sin(v * 50.0),
        ),
        96,
        80,
    )
    return mesh


def make_vista_hole_wall():
    mesh = Mesh("VistaHoleWall_HD", "vista_rock")
    for x0, x1, y0, y1 in [
        (-1.30, -0.36, -1.00, 1.00),
        (0.36, 1.30, -1.00, 1.00),
        (-0.36, 0.36, 0.48, 1.00),
        (-0.36, 0.36, -1.00, -0.48),
    ]:
        mesh.add_grid(
            lambda u, v, ax=x0, bx=x1, ay=y0, by=y1: (
                ax + (bx - ax) * u,
                ay + (by - ay) * v,
                eroded(u + ax, v + ay, 0.06, 41),
            ),
            40,
            40,
        )
    return mesh


def make_broken_bridge():
    mesh = Mesh("SpireBrokenBridgeArc_HD", "root_rock")
    mesh.add_grid(
        lambda u, v: (
            -1.0 + 2.0 * u,
            -0.10 + 0.20 * v,
            0.25 * math.sin(u * math.pi * 0.85) + 0.10 * math.sin(u * math.pi * 3.0) + eroded(u, v, 0.035, 51),
        ),
        90,
        10,
    )
    mesh.add_grid(
        lambda u, v: (
            -1.0 + 2.0 * u,
            -0.26 + 0.08 * v,
            0.25 * math.sin(u * math.pi * 0.85) + 0.10 * math.sin(u * math.pi * 3.0) - 0.18,
        ),
        90,
        4,
        flip=True,
    )
    return mesh


def main():
    material_dir = MESH_ROOT / "materials"
    write_bmp(material_dir / "organic_rock_albedo.bmp", 256, 256, rock_sampler((142, 104, 78)))
    write_bmp(material_dir / "rib_rock_albedo.bmp", 256, 256, rock_sampler((118, 95, 86)))
    write_bmp(material_dir / "root_rock_albedo.bmp", 256, 256, rock_sampler((112, 80, 60)))
    write_bmp(material_dir / "vista_rock_albedo.bmp", 256, 256, rock_sampler((154, 139, 132)))

    specs = [
        ("OrganicArchLarge", "OrganicArchLarge.obj", "organic_rock", "organic_rock_albedo.bmp", make_organic_arch()),
        ("RibTunnelWall", "RibTunnelWall.obj", "rib_rock", "rib_rock_albedo.bmp", make_rib_tunnel()),
        ("RootSpireColumn", "RootSpireColumn.obj", "root_rock", "root_rock_albedo.bmp", make_root_spire()),
        ("CurvedCanyonWall", "CurvedCanyonWall.obj", "organic_rock", "organic_rock_albedo.bmp", make_curved_wall()),
        ("VistaHoleWall", "VistaHoleWall.obj", "vista_rock", "vista_rock_albedo.bmp", make_vista_hole_wall()),
        ("SpireBrokenBridgeArc", "SpireBrokenBridgeArc.obj", "root_rock", "root_rock_albedo.bmp", make_broken_bridge()),
    ]

    for folder, filename, material, texture, mesh in specs:
        directory = MESH_ROOT / folder
        directory.mkdir(parents=True, exist_ok=True)
        write_mtl(directory, material, texture)
        path = mesh.write(directory, filename, f"{material}.mtl")
        print(f"{path.relative_to(ROOT)}: {len(mesh.vertices)} verts, {len(mesh.faces)} tris")


if __name__ == "__main__":
    random.seed(1234)
    main()
