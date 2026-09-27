"""Create the neutral material palette without any input image or sample asset."""
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[1]


def generate():
    # White lets each authored material/particle supply its own colour. No logo,
    # borrowed pattern, or baked lighting is applied to the vehicle and enemies.
    size = 8
    pixels = bytes([255, 255, 255]) * size * size
    header = struct.pack('<2sIHHI', b'BM', 54 + len(pixels), 0, 0, 54)
    header += struct.pack('<IiiHHIIiiII', 40, size, size, 1, 24, 0, len(pixels), 2835, 2835, 0, 0)
    target = ROOT / 'Resources/common/neutralSurface.bmp'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(header + pixels)


if __name__ == '__main__':
    generate()
