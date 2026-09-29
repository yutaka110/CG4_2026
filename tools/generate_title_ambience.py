"""Generate original periodic rail rumble, wheel joints and soft wind (no samples)."""
from pathlib import Path
import math
import random
import struct
import wave

RATE = 22050
DURATION = 4
rng = random.Random(92403)
samples = [0.0] * (RATE * DURATION)
# Integer Fourier bins ensure a continuous waveform at the loop boundary.
for _ in range(96):
    frequency = rng.randrange(160, 6800) / DURATION
    phase = rng.random() * math.tau
    amplitude = 0.025 / (1 + frequency / 220)
    for i in range(len(samples)):
        t = i / RATE
        samples[i] += amplitude * math.sin(math.tau * frequency * t + phase)
for i in range(len(samples)):
    t = i / RATE
    rumble = 0.12 * math.sin(math.tau * 48 * t) + 0.055 * math.sin(math.tau * 73 * t)
    joint_age = t % 0.20
    attack = min(1, joint_age / 0.004)
    joint = 0.10 * attack * math.exp(-joint_age * 70) * (
        math.sin(math.tau * 340 * joint_age) + 0.3 * math.sin(math.tau * 705 * joint_age))
    samples[i] += rumble + joint
peak = max(abs(x) for x in samples)
pcm = b''.join(struct.pack('<h', round(x / peak * 0.65 * 32767)) for x in samples)
target = Path(__file__).resolve().parents[1] / 'Resources/audio/title_rail_ambience.wav'
with wave.open(str(target), 'wb') as out:
    out.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
    out.writeframes(pcm)
print(f'{target.name}: {DURATION}s, peak=0.65, {len(pcm)} PCM bytes')
