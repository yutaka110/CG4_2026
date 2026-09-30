"""Original rock mountain with an irregular straight, sealed cave interior.
Geometry is original; the runtime reuses the gameplay Rocks016 albedo.
OBJ Z is negated for Assimp LH. UVs encode wall depth for interior shading.
"""
import math
import uuid
from generate_title_landscape import Mesh, OUT, ROOT, cross

m = Mesh('TitleTunnel')
rings = []
uvs = []
segments=24
for step in range(41):
    depth=step*2.0
    row = []
    for interior in (True,False):
        for i in range(segments+3):
            a=math.pi*(1-max(0,min(segments,i-1))/segments)
            if interior:
                # Always preserve the camera corridor; broad facets rather than rings.
                radius=8.4+0.7*math.sin(3*a+0.12*depth)+0.4*math.sin(5*a-0.09*depth)
                x=radius*math.cos(a)+0.5*math.sin(depth*0.08)
                y=3.0+radius*math.sin(a)*1.06+0.45*math.sin(a*4+depth*0.15)*math.sin(a)
                lip=1.5*math.sin(3*a+0.8)+0.65*math.cos(5*a)
            else:
                # A large asymmetric ridge surrounds the opening, not a parallel shell.
                radius=50+5*math.sin(a*3+0.035*depth)+3*math.sin(depth*0.06+a)
                x=radius*math.cos(a)+4*math.sin(a)
                y=3+42*max(0,math.sin(a))**0.65+3*math.sin(3*a+depth*0.04)*math.sin(a)
                lip=-3+2*math.sin(a*2+0.3)
            if i in (0,segments+2): y=-4.0
            s=depth+lip*math.exp(-depth/10)
            # Leave a solid rear wall; no far opening can leak the bright sky.
            if interior: s-=2.0*(depth/80.0)**8
            row.append(m.vertex((x,y,-s)))
            uvs.append((1.0 if interior else 0.0,depth))
    rings.append(row)
n = segments+3
# Subdivide the cliff face into broad ledges. A single inner-to-outer fan
# creates long radial facets that read as a constructed tunnel mouth.
front = [rings[0][:n]]
for band in range(1,8):
    t = band/8.0
    row = []
    for i in range(n):
        inside,outside = m.vertices[rings[0][i]],m.vertices[rings[0][n+i]]
        x,y,z = (inside[k]*(1-t)+outside[k]*t for k in range(3))
        z += math.sin(math.pi*t)*(2*math.sin(x*.16+y*.05)+1.2*math.sin(y*.23-x*.07))
        row.append(m.vertex((x,y,z)))
        uvs.append((1-t,0.0))
    front.append(row)
front.append(rings[0][n:])
for step,(a,b) in enumerate(zip(rings,rings[1:])):
    for i in range(n-1):
        m.face(a[i],b[i],b[i+1],a[i+1])
        m.face(a[n+i+1],b[n+i+1],b[n+i],a[n+i])
    if step == 0:
        # Share the subdivided front boundary with the buried base faces.
        m.face(b[n],b[0],a[0])
        m.face(b[n-1],b[2*n-1],a[2*n-1])
        for inner,outer in zip(front,front[1:]):
            m.face(b[n],inner[0],outer[0])
            m.face(b[n-1],outer[-1],inner[-1])
    else:
        m.face(a[n],b[n],b[0],a[0])
        m.face(a[n-1],b[n-1],b[2*n-1],a[2*n-1])
for inner,outer in zip(front,front[1:]):
    for i in range(n-1):
        m.face(inner[i],inner[i+1],outer[i+1],outer[i])
b=rings[-1]
for interior in (True,False):
    section=b[:n] if interior else b[n:]
    center=m.vertex(tuple(sum(m.vertices[v][axis] for v in section)/n for axis in range(3)))
    uvs.append((1.0 if interior else 0.0,78.0 if interior else 80.0))
    for i in range(n):
        j=(i+1)%n
        m.face(center,section[j],section[i]) if interior else m.face(center,section[i],section[j])
m.face(b[n-1],b[0],b[n],b[2*n-1])
volume=sum(sum(m.vertices[f[0]][i]*cross(m.vertices[f[1]],m.vertices[f[2]])[i]
               for i in range(3))/6 for f in m.faces)
if volume<0:
    m.faces=[tuple(reversed(f)) for f in m.faces]
m.validate()
asset=OUT/'TitleTunnel.obj'
# Split normals at rock ledges/lip, retaining welded geometry for a closed solid.
face_normals=[]
adjacent=[[] for _ in m.vertices]
for face in m.faces:
    a,b,c=[m.vertices[i] for i in face]
    normal=cross(tuple(b[i]-a[i] for i in range(3)),tuple(c[i]-a[i] for i in range(3)))
    length=math.sqrt(sum(v*v for v in normal))
    normal=tuple(v/length for v in normal)
    face_normals.append(normal)
    for i in face: adjacent[i].append(normal)
normals=[]
for face,normal in zip(m.faces,face_normals):
    for vertex in face:
        neighbors=[v for v in adjacent[vertex] if sum(v[i]*normal[i] for i in range(3))>0.75]
        averaged=tuple(sum(v[i] for v in neighbors) for i in range(3))
        length=math.sqrt(sum(v*v for v in averaged))
        normals.append(tuple(v/length for v in averaged))
lines=['# Original asymmetric mountain and excavated passage.',
       'mtllib title_sandstone.mtl','o TitleCaveMountain','usemtl title_sandstone']
lines+=['v %.7f %.7f %.7f'%p for p in m.vertices]
lines+=['vt %.6f %.6f'%uv for uv in uvs]
lines+=['vn %.7f %.7f %.7f'%normal for normal in normals]
lines+=['f '+' '.join(f'{v+1}/{v+1}/{i*3+k+1}' for k,v in enumerate(face)) for i,face in enumerate(m.faces)]
asset.write_text('\n'.join(lines)+'\n',encoding='ascii')
print(f'Cave mountain: {len(m.vertices)} positions, {len(m.faces)} triangles; closed shell and crease normals verified')
meta=asset.with_suffix('.obj.meta')
meta.write_text(f'guid={uuid.uuid5(uuid.NAMESPACE_URL,asset.relative_to(ROOT).as_posix()).hex}\nlogicalPath={asset.relative_to(ROOT).as_posix()}\n',encoding='ascii')
