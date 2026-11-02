from __future__ import annotations

from pathlib import Path

SIZE = 64
INV_SIZE = 1.0 / float(SIZE)


def unorm8(value: float) -> int:
    scaled = value * 255.0 + 0.5
    return max(0, min(255, int(scaled)))


def build() -> bytes:
    pixels = bytearray(SIZE * SIZE * 4)
    for y in range(SIZE):
        for x in range(SIZE):
            uv_x = (x + 0.5) * INV_SIZE
            uv_y = (y + 0.5) * INV_SIZE
            index = (y * SIZE + x) * 4
            pixels[index] = unorm8(uv_x)
            pixels[index + 1] = unorm8(uv_y)
            pixels[index + 2] = unorm8(0.25)
            pixels[index + 3] = unorm8(1.0)
    return bytes(pixels)


def main() -> int:
    target = Path(__file__).resolve().parent / "gradient.rgba8"
    data = build()
    target.write_bytes(data)
    print(f"wrote {target} ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
