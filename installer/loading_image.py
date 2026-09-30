"""Original dependency-free grayscale art for the guest's 24-bit TGA loader."""
import math
import struct

WIDTH, HEIGHT = 320, 240
# Original 3x5 pixel lettering; no imported font or game artwork.
FONT = {
    "N": ("101", "111", "111", "111", "101"),
    "X": ("101", "101", "010", "101", "101"),
    "H": ("101", "101", "111", "101", "101"),
    "A": ("010", "101", "111", "101", "101"),
    "L": ("100", "100", "100", "100", "111"),
    "O": ("111", "101", "101", "101", "111"),
    "D": ("110", "101", "101", "101", "110"),
    "I": ("111", "010", "010", "010", "111"),
    "G": ("111", "100", "101", "101", "111"),
}


def make_loading_tga() -> bytes:
    """320x240, uncompressed 24-bit BGR, descriptor0 (bottom row first)."""
    pixels = bytearray(WIDTH * HEIGHT)
    for y in range(HEIGHT):
        shade = 8 + (24 * y // (HEIGHT - 1))
        pixels[y * WIDTH:(y + 1) * WIDTH] = bytes([shade]) * WIDTH

    def put(x, y, shade):
        if 0 <= x < WIDTH and 0 <= y < HEIGHT:
            pixels[y * WIDTH + x] = shade

    for y in range(66, 143):
        for x in range(123, 198):
            radius = math.hypot(x - 160, y - 104)
            if 33 <= radius < 35:
                put(x, y, 105)
            elif 20 <= radius < 21:
                put(x, y, 55)
    for offset in range(-24, 25):
        put(160 + offset, 104, 150 if abs(offset) < 7 else 45)
        put(160, 104 + offset, 150 if abs(offset) < 7 else 45)

    def text(value, y, scale, shade):
        width = (len(value) * 4 - 1) * scale
        start = (WIDTH - width) // 2
        for index, char in enumerate(value):
            for row, bits in enumerate(FONT[char]):
                for col, bit in enumerate(bits):
                    if bit == "1":
                        for dy in range(scale):
                            for dx in range(scale):
                                put(start + index * 4 * scale + col * scale + dx,
                                    y + row * scale + dy, shade)

    text("NXHALO", 34, 3, 210)
    text("LOADING", 164, 2, 165)
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0,
                         WIDTH, HEIGHT, 24, 0)
    body = bytearray()
    for y in range(HEIGHT - 1, -1, -1):
        for intensity in pixels[y * WIDTH:(y + 1) * WIDTH]:
            body.extend((intensity, intensity, intensity))
    return header + bytes(body)
