#!/usr/bin/env python3
"""Generate ACE OS boot artwork as PNGs — no image libraries required.

  python3 tools/make-art.py config/includes.chroot

Writes:
  usr/share/backgrounds/aceos/grub.png            GRUB background (1920x1080)
  usr/share/plymouth/themes/ace/background.png    boot splash
  usr/share/plymouth/themes/ace/logo.png          pulsing mark

Everything is drawn procedurally: dark ground, faint grid, the ACE sigil,
and a drift of the countdown digits, matching the desktop wallpaper.
"""

import math
import struct
import sys
import zlib
from pathlib import Path

DIGIT_GLYPHS = {
    "0": ("01110", "10001", "10011", "10101", "11001", "10001", "01110"),
    "1": ("00100", "01100", "00100", "00100", "00100", "00100", "01110"),
    "2": ("01110", "10001", "00001", "00010", "00100", "01000", "11111"),
    "3": ("11111", "00010", "00100", "00010", "00001", "10001", "01110"),
    "4": ("00010", "00110", "01010", "10010", "11111", "00010", "00010"),
    "5": ("11111", "10000", "11110", "00001", "00001", "10001", "01110"),
    "6": ("00110", "01000", "10000", "11110", "10001", "10001", "01110"),
    "7": ("11111", "00001", "00010", "00100", "01000", "01000", "01000"),
    "8": ("01110", "10001", "10001", "01110", "10001", "10001", "01110"),
    "9": ("01110", "10001", "10001", "01111", "00001", "00010", "01100"),
    "A": ("01110", "10001", "10001", "11111", "10001", "10001", "10001"),
    "C": ("01110", "10001", "10000", "10000", "10000", "10001", "01110"),
    "E": ("11111", "10000", "10000", "11110", "10000", "10000", "11111"),
}

DIGITS = "090807060504030201"


class Canvas:
    def __init__(self, width, height):
        self.width = width
        self.height = height
        self.pixels = bytearray(width * height * 3)

    def set(self, x, y, colour, alpha=1.0):
        x, y = int(x), int(y)
        if not (0 <= x < self.width and 0 <= y < self.height):
            return
        index = (y * self.width + x) * 3
        for channel in range(3):
            existing = self.pixels[index + channel]
            self.pixels[index + channel] = int(
                existing * (1 - alpha) + colour[channel] * alpha)

    def fill_gradient(self, inner, outer):
        cx, cy = self.width / 2, self.height * 0.44
        longest = math.hypot(cx, cy)
        for y in range(self.height):
            for x in range(self.width):
                distance = math.hypot(x - cx, y - cy) / longest
                blend = min(1.0, distance ** 0.85)
                index = (y * self.width + x) * 3
                for channel in range(3):
                    self.pixels[index + channel] = int(
                        inner[channel] * (1 - blend) + outer[channel] * blend)

    def grid(self, step, colour, alpha):
        for y in range(0, self.height, step):
            for x in range(self.width):
                self.set(x, y, colour, alpha)
        for x in range(0, self.width, step):
            for y in range(self.height):
                self.set(x, y, colour, alpha)

    def line(self, x0, y0, x1, y1, colour, width=2, alpha=1.0):
        steps = int(max(abs(x1 - x0), abs(y1 - y0))) + 1
        for step in range(steps):
            t = step / max(1, steps - 1)
            x = x0 + (x1 - x0) * t
            y = y0 + (y1 - y0) * t
            for dx in range(-width // 2, width // 2 + 1):
                for dy in range(-width // 2, width // 2 + 1):
                    self.set(x + dx, y + dy, colour, alpha)

    def circle(self, cx, cy, radius, colour, width=2, alpha=1.0):
        steps = int(2 * math.pi * radius) + 8
        for step in range(steps):
            angle = 2 * math.pi * step / steps
            for ring in range(width):
                self.set(cx + (radius + ring) * math.cos(angle),
                         cy + (radius + ring) * math.sin(angle), colour, alpha)

    def glyph(self, char, x, y, scale, colour, alpha=1.0):
        rows = DIGIT_GLYPHS.get(char.upper())
        if not rows:
            return
        for row_index, row in enumerate(rows):
            for col_index, bit in enumerate(row):
                if bit == "1":
                    for dx in range(scale):
                        for dy in range(scale):
                            self.set(x + col_index * scale + dx,
                                     y + row_index * scale + dy, colour, alpha)

    def text(self, string, x, y, scale, colour, spacing=1, alpha=1.0):
        cursor = x
        for char in string:
            self.glyph(char, cursor, y, scale, colour, alpha)
            cursor += (5 + spacing) * scale
        return cursor

    def png(self):
        rows = []
        stride = self.width * 3
        for y in range(self.height):
            rows.append(b"\x00" + bytes(self.pixels[y * stride:(y + 1) * stride]))
        raw = b"".join(rows)

        def chunk(tag, data):
            body = tag + data
            return (struct.pack(">I", len(data)) + body
                    + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF))

        return (b"\x89PNG\r\n\x1a\n"
                + chunk(b"IHDR", struct.pack(">IIBBBBB", self.width, self.height,
                                             8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 9))
                + chunk(b"IEND", b""))


BLOOD = (225, 15, 40)
RUST = (122, 13, 24)
BONE = (242, 242, 242)
INNER = (26, 4, 7)
OUTER = (5, 5, 5)


def digit_drift(canvas, columns, seed=7):
    """A sparse fall of countdown digits, densest near the edges."""
    state = seed
    cell = max(8, canvas.width // columns)
    for column in range(columns):
        state = (state * 1103515245 + 12345) & 0x7FFFFFFF
        start = (state % canvas.height) - canvas.height // 2
        length = 6 + (state >> 8) % 14
        centre_bias = abs(column / columns - 0.5) * 2
        alpha_scale = 0.10 + 0.55 * centre_bias
        for step in range(length):
            y = start + step * cell
            if not (0 <= y < canvas.height):
                continue
            fade = (step / length) * alpha_scale
            colour = BONE if step == length - 1 else BLOOD
            canvas.glyph(DIGITS[(column + step) % len(DIGITS)],
                         column * cell, y, max(1, cell // 8), colour, fade)


def sigil(canvas, cx, cy, radius):
    points = []
    for index in range(6):
        angle = math.pi / 2 + index * math.pi / 3
        points.append((cx + radius * math.cos(angle) * 0.87,
                       cy - radius * math.sin(angle)))
    for index in range(6):
        x0, y0 = points[index]
        x1, y1 = points[(index + 1) % 6]
        canvas.line(x0, y0, x1, y1, BLOOD, 3, 0.9)
    for index in range(6):
        x0, y0 = points[index]
        x1, y1 = points[(index + 3) % 6]
        canvas.line(x0, y0, x1, y1, RUST, 1, 0.45)
    canvas.circle(cx, cy, radius * 0.24, BLOOD, 3, 0.95)
    canvas.circle(cx, cy, radius * 0.06, BLOOD, 4, 1.0)


def build_splash(width, height, title_scale, with_drift=True):
    canvas = Canvas(width, height)
    canvas.fill_gradient(INNER, OUTER)
    canvas.grid(max(24, width // 40), BLOOD, 0.045)
    if with_drift:
        digit_drift(canvas, max(30, width // 26))
    sigil(canvas, width / 2, height * 0.40, min(width, height) * 0.17)

    label = "ACE"
    label_width = len(label) * 6 * title_scale
    canvas.text(label, (width - label_width) / 2, height * 0.63,
                title_scale, BONE, alpha=0.97)

    bar_width = int(width * 0.22)
    bar_x = (width - bar_width) // 2
    bar_y = int(height * 0.82)
    for x in range(bar_width):
        canvas.set(bar_x + x, bar_y, BLOOD, 0.5)
    return canvas


def main():
    target = Path(sys.argv[1] if len(sys.argv) > 1 else "config/includes.chroot")
    grub_dir = target / "usr/share/backgrounds/aceos"
    theme_dir = target / "usr/share/plymouth/themes/ace"
    grub_dir.mkdir(parents=True, exist_ok=True)
    theme_dir.mkdir(parents=True, exist_ok=True)

    splash = build_splash(1920, 1080, 22)
    (grub_dir / "grub.png").write_bytes(splash.png())
    print(f"wrote {grub_dir / 'grub.png'}")

    boot = build_splash(1024, 768, 12)
    (theme_dir / "background.png").write_bytes(boot.png())
    print(f"wrote {theme_dir / 'background.png'}")

    logo = Canvas(320, 200)
    logo.fill_gradient((10, 3, 4), (5, 5, 5))
    sigil(logo, 160, 78, 56)
    logo.text("ACE", 160 - (3 * 6 * 6) / 2, 140, 6, BONE, alpha=0.95)
    (theme_dir / "logo.png").write_bytes(logo.png())
    print(f"wrote {theme_dir / 'logo.png'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
