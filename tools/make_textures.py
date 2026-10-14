"""Generate the procedural textures used by the scene (our own artwork, no external source).

Usage: python tools/make_textures.py        (writes into Textures/)
"""
import os
import random

from PIL import Image, ImageDraw, ImageFilter

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Textures")
random.seed(7)


def noise(img, amount):
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            n = random.randint(-amount, amount)
            r, g, b = px[x, y][:3]
            px[x, y] = (max(0, min(255, r + n)), max(0, min(255, g + n)), max(0, min(255, b + n)))
    return img


def sidewalk():
    # Thai pavement: square concrete blocks, a few reddish ones, dark grout
    s, n = 256, 4
    img = Image.new("RGB", (s, s), (70, 66, 62))
    d = ImageDraw.Draw(img)
    cell = s // n
    for j in range(n):
        for i in range(n):
            base = (178, 92, 78) if (i + j) % 3 == 0 else (168, 164, 156)
            v = random.randint(-10, 10)
            c = tuple(max(0, min(255, k + v)) for k in base)
            d.rectangle([i * cell + 3, j * cell + 3, (i + 1) * cell - 3, (j + 1) * cell - 3], fill=c)
    noise(img, 10).save(os.path.join(OUT, "sidewalk.png"))


def asphalt():
    s = 256
    img = noise(Image.new("RGB", (s, s), (58, 58, 62)), 18).filter(ImageFilter.GaussianBlur(0.6))
    img.save(os.path.join(OUT, "asphalt.png"))


def curb():
    # red / white painted kerb (Thai no-parking marking)
    w, h = 256, 32
    img = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(img)
    for i in range(4):
        d.rectangle([i * 64, 0, i * 64 + 63, h], fill=(200, 40, 40) if i % 2 == 0 else (235, 235, 230))
    noise(img, 8).save(os.path.join(OUT, "curb.png"))


def shophouse():
    # one shophouse unit: ground floor roller shutter, upper floor with lit windows
    w, h = 256, 384
    img = Image.new("RGB", (w, h), (226, 204, 168))
    d = ImageDraw.Draw(img)
    # upper floor windows (warm lit)
    for x0 in (36, 146):
        d.rectangle([x0, 60, x0 + 74, 170], fill=(90, 70, 50))
        d.rectangle([x0 + 5, 65, x0 + 69, 165], fill=(255, 214, 140))
        d.line([x0 + 37, 65, x0 + 37, 165], fill=(90, 70, 50), width=4)
        d.rectangle([x0 - 4, 172, x0 + 78, 180], fill=(190, 170, 140))
    # floor divider
    d.rectangle([0, 205, w, 222], fill=(170, 150, 120))
    # roller shutter (closed)
    d.rectangle([14, 236, w - 14, h], fill=(150, 152, 156))
    for y in range(240, h, 7):
        d.line([14, y, w - 14, y], fill=(118, 120, 126), width=2)
    # pillars between units
    d.rectangle([0, 0, 10, h], fill=(200, 180, 146))
    d.rectangle([w - 10, 0, w, h], fill=(200, 180, 146))
    noise(img, 6).save(os.path.join(OUT, "shophouse.png"))


def sky():
    # sunset gradient: deep purple (top) -> pink -> orange (horizon)
    w, h = 4, 256
    stops = [(0.0, (52, 44, 96)), (0.45, (186, 92, 120)), (0.75, (246, 146, 86)), (1.0, (255, 196, 120))]
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        t = y / (h - 1)
        for (t0, c0), (t1, c1) in zip(stops, stops[1:]):
            if t0 <= t <= t1:
                k = (t - t0) / (t1 - t0)
                c = tuple(int(a + (b - a) * k) for a, b in zip(c0, c1))
        for x in range(w):
            px[x, y] = c
    img.save(os.path.join(OUT, "sky.png"))


def stainless():
    s = 64
    img = Image.new("RGB", (s, s), (196, 198, 202))
    px = img.load()
    for y in range(s):
        n = random.randint(-12, 12)
        for x in range(s):
            v = 196 + n + random.randint(-4, 4)
            px[x, y] = (v, v + 2, v + 6)
    img.save(os.path.join(OUT, "stainless.png"))


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for f in (sidewalk, asphalt, curb, shophouse, sky, stainless):
        f()
        print("wrote", f.__name__)
