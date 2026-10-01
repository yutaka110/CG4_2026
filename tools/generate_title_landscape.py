"""Original continuous title terrain; no sampled textures or random microgeometry.

OBJ uses right-handed coordinates; the existing Assimp importer converts to LH.
All meshes are closed, share seam vertices, and carry area-weighted normals.
"""
from pathlib import Path
from collections import Counter
import math
import struct
import uuid

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Resources/course_meshes/TitleLandscape'


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


class Mesh:
    def __init__(self, name):
        self.name, self.vertices, self.faces = name, [], []

    def vertex(self, p):
        self.vertices.append(p)
        return len(self.vertices)-1

    def face(self, *indices):
        for i in range(1, len(indices)-1):
            self.faces.append((indices[0], indices[i], indices[i+1]))

    def validate(self):
        edges = Counter()
        balance = Counter()
        volume = 0.0
        for face in self.faces:
            a, b, c = [self.vertices[i] for i in face]
            n = cross(tuple(b[i]-a[i] for i in range(3)), tuple(c[i]-a[i] for i in range(3)))
            assert sum(x*x for x in n) > 1e-14, 'degenerate face'
            volume += sum(a[i]*cross(b,c)[i] for i in range(3))/6
            for u,v in zip(face,face[1:]+face[:1]):
                key = tuple(sorted((u,v)))
                edges[key] += 1
                balance[key] += 1 if u<v else -1
        assert all(count==2 for count in edges.values()), 'open seam or non-manifold edge'
        assert all(value==0 for value in balance.values()), 'inconsistent winding'
        assert volume>0, 'inward-facing shell'

    def save(self):
        self.validate()
        normals = [[0.0]*3 for _ in self.vertices]
        for face in self.faces:
            a,b,c = [self.vertices[i] for i in face]
            n = cross(tuple(b[i]-a[i] for i in range(3)), tuple(c[i]-a[i] for i in range(3)))
            for vertex in face:
                for axis in range(3): normals[vertex][axis] += n[axis]
        # The importer uses vertex normals to repair winding. Reject source
        # normals that would cause it to reverse isolated triangles in a solid.
        for face in self.faces:
            a,b,c = [self.vertices[i] for i in face]
            n = cross(tuple(b[i]-a[i] for i in range(3)), tuple(c[i]-a[i] for i in range(3)))
            authored = [sum(normals[j][axis]/math.sqrt(sum(v*v for v in normals[j])) for j in face) for axis in range(3)]
            assert sum(n[axis]*authored[axis] for axis in range(3)) > 0, 'normal/winding disagreement'
        lines = ['# Original title scenery; closed shared-vertex topology.',
                 'mtllib title_sandstone.mtl', f'o {self.name}', 'usemtl title_sandstone']
        lines += ['v %.7f %.7f %.7f'%p for p in self.vertices]
        lines += ['vt 0.5 0.5' for _ in self.vertices]
        for n in normals:
            length = math.sqrt(sum(x*x for x in n))
            lines.append('vn %.7f %.7f %.7f'%tuple(x/length for x in n))
        lines += ['f '+' '.join(f'{i+1}/{i+1}/{i+1}' for i in face) for face in self.faces]
        path = OUT/(self.name+'.obj')
        path.write_text('\n'.join(lines)+'\n',encoding='ascii')
        print(f'{self.name}: {len(self.vertices)} vertices, {len(self.faces)} triangles; closed/winding verified')


def ground_height(radius, angle):
    # The sleeper underside is -0.28 m. The broad rail corridor stays flat,
    # including the curve's chord and the full start-camera orbit footprint.
    if 60 <= radius <= 260:
        return -0.30
    influence = min(1.0, max(0.0, (60-radius)/20 if radius<60 else (radius-260)/40))
    influence = influence*influence*(3-2*influence)
    broad = 0.65*math.sin(angle*2+0.4) + 0.40*math.cos(angle*3-radius/80)
    return -0.30 + influence*broad - 9*max(0,(radius-220)/80)**2


def ground():
    mesh = Mesh('TitleGround')
    segments = 192
    center = mesh.vertex((0,ground_height(0,0),0))
    rings = []
    for radius in (12,25,40,52,60,64,67,70,73,76,80,88,92,105,130,160,200,250,260,300):
        rings.append([mesh.vertex((radius*math.cos(i*math.tau/segments),
            ground_height(radius,i*math.tau/segments),radius*math.sin(i*math.tau/segments))) for i in range(segments)])
    for i in range(segments):
        j=(i+1)%segments
        mesh.face(center,rings[0][j],rings[0][i])
    for inner,outer in zip(rings,rings[1:]):
        for i in range(segments):
            j=(i+1)%segments
            mesh.face(inner[i],inner[j],outer[j],outer[i])
    bottom=[mesh.vertex((300*math.cos(i*math.tau/segments),-24,300*math.sin(i*math.tau/segments))) for i in range(segments)]
    cap=mesh.vertex((0,-24,0))
    for i in range(segments):
        j=(i+1)%segments
        mesh.face(rings[-1][i],rings[-1][j],bottom[j],bottom[i])
        mesh.face(cap,bottom[i],bottom[j])
    return mesh


def rock(name, boulder=False):
    mesh = Mesh(name)
    segments=24 if boulder else 40
    levels = [(0,0.85),(0.15,1),(0.55,0.92),(0.87,0.72),(1,0.36)] if boulder else [
        (0,1),(0.08,0.99),(0.38,0.88),(0.68,0.83),(0.92,0.73),(1,0.66)]
    rings=[]
    for height,radius in levels:
        row=[]
        for i in range(segments):
            a=i*math.tau/segments
            outline=1+0.075*math.sin(3*a+0.4)+0.045*math.cos(5*a)
            y=height*(1+0.05*math.sin(2*a)+0.025*math.cos(3*a))
            row.append(mesh.vertex((radius*outline*math.cos(a)+0.09*height,y,radius*outline*math.sin(a))))
        rings.append(row)
    for lower,upper in zip(rings,rings[1:]):
        for i in range(segments):
            j=(i+1)%segments
            mesh.face(lower[i],upper[i],upper[j],lower[j])
    low=mesh.vertex((0,0,0)); high=mesh.vertex((0.09,1,0))
    for i in range(segments):
        j=(i+1)%segments
        mesh.face(low,rings[0][i],rings[0][j])
        mesh.face(high,rings[-1][j],rings[-1][i])
    return mesh


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    for mesh in (ground(),rock('TitleCliff'),rock('TitleBoulder',True)):
        mesh.save()
    # Quiet, solid sandstone albedo; variation comes from broad geometry/light.
    pixels=bytes((124,155,180))*64
    header=struct.pack('<2sIHHI',b'BM',54+len(pixels),0,0,54)+struct.pack('<IiiHHIIiiII',40,8,8,1,24,0,len(pixels),2835,2835,0,0)
    (OUT/'title_sandstone.bmp').write_bytes(header+pixels)
    (OUT/'title_sandstone.mtl').write_text('newmtl title_sandstone\nKd 1 1 1\nKs 0 0 0\nNs 1\nmap_Kd title_sandstone.bmp\n',encoding='ascii')
    for asset in OUT.iterdir():
        if asset.suffix not in ('.obj','.bmp','.mtl'): continue
        meta=asset.with_suffix(asset.suffix+'.meta')
        if not meta.exists():
            meta.write_text(f'guid={uuid.uuid5(uuid.NAMESPACE_URL,asset.relative_to(ROOT).as_posix()).hex}\nlogicalPath={asset.relative_to(ROOT).as_posix()}\n',encoding='ascii')


if __name__=='__main__': main()
