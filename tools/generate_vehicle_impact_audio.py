"""Generate an original mono PCM vehicle impact; no sampled audio is used."""
from pathlib import Path
import math, random, struct, wave
rate = 44100
rng = random.Random(290926)
values = []
for i in range(int(rate * 0.32)):
    t = i / rate
    ring = sum(a * math.sin(2 * math.pi * f * t) * math.exp(-t / decay)
               for f, a, decay in [(237,.4,.048),(713,.28,.075),(1193,.16,.051),(1879,.11,.038),(2911,.05,.022)])
    strike = rng.uniform(-1,1) * (.48 * math.exp(-t/.009) + .08 * math.exp(-t/.045))
    envelope = min(1,t/.0015) * min(1,(.32-t)/.018)
    values.append((ring + strike) * envelope)
peak = max(map(abs,values))
path = Path(__file__).resolve().parents[1] / 'Resources/audio/vehicle_metal_impact.wav'
path.parent.mkdir(parents=True, exist_ok=True)
with wave.open(str(path), 'wb') as out:
    out.setnchannels(1); out.setsampwidth(2); out.setframerate(rate)
    out.writeframes(b''.join(struct.pack('<h', round(v/peak*.82*32767)) for v in values))
