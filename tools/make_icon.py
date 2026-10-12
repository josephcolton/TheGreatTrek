"""Draws the game icon: an ox wagon on Karoo brown.

The icon is pixel art on a 32x32 grid, drawn here from simple shapes so it can be changed and
redrawn. It writes assets/icon/icon.png (256x256), assets/icon/icon.ico (16 to 256) and the 32x32
master, icon_32.png. icon.png is also the window icon. The outputs are committed, so building the game does not need Python.
Usage: python tools/make_icon.py   (needs Pillow)
"""
import os
from PIL import Image

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "assets", "icon")
N = 32

PAL = {
    "rim": (58, 34, 20),       # outline of the tile
    "sky": (176, 104, 54),     # Karoo brown, lighter above
    "sky2": (156, 88, 44),
    "veld": (124, 68, 34),     # the ground, darker
    "veld2": (104, 56, 28),
    "sun": (236, 196, 112),
    "ink": (34, 24, 18),       # outlines of the wagon
    "canvas": (242, 234, 212),
    "canvas2": (214, 202, 174),
    "hoop": (186, 170, 140),
    "wood": (146, 98, 56),
    "wood2": (98, 64, 36),
    "hub": (196, 150, 92),
}


def tile():
    """The brown background: rounded square, lighter sky over darker veld, a low sun."""
    px = {}
    r = 4
    for y in range(N):
        for x in range(N):
            # rounded corners
            cx = min(max(x, r), N - 1 - r)
            cy = min(max(y, r), N - 1 - r)
            d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
            if d > r + 0.35:
                continue
            if d > r - 0.65 or x in (0, N - 1) and r <= y <= N - 1 - r or y in (0, N - 1) and r <= x <= N - 1 - r:
                px[(x, y)] = "rim"
            elif y < 23:
                px[(x, y)] = "sky" if y < 12 else "sky2"
            else:
                px[(x, y)] = "veld" if y < 27 else "veld2"
    # a low sun behind the wagon, upper right
    for y in range(N):
        for x in range(N):
            if px.get((x, y)) in ("sky", "sky2") and (x - 25) ** 2 + (y - 7) ** 2 <= 10.6:
                px[(x, y)] = "sun"
    return px


def disc(px, cx, cy, r, key, only=None):
    for y in range(N):
        for x in range(N):
            if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                if only is None or px.get((x, y)) in only:
                    px[(x, y)] = key


def ring(px, cx, cy, r_out, r_in, key):
    for y in range(N):
        for x in range(N):
            d2 = (x - cx) ** 2 + (y - cy) ** 2
            if r_in * r_in < d2 <= r_out * r_out:
                px[(x, y)] = key


def rect(px, x0, y0, x1, y1, key):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            px[(x, y)] = key


def wagon(px):
    """A Cape ox wagon facing right: canvas tent, wooden bed, big back wheel, small front wheel."""
    # tent: rows 6..16, cols 5..22, rounded at the top corners, outlined
    x0, x1, y0, y1 = 5, 22, 6, 16
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            corner = (y == y0 and x in (x0, x0 + 1, x1 - 1, x1)) or (y == y0 + 1 and x in (x0, x1))
            if corner:
                continue
            edge = (y == y0 or x in (x0, x1) or (y == y0 + 1 and x in (x0 + 1, x1 - 1)))
            px[(x, y)] = "ink" if edge else ("canvas2" if y >= y1 - 2 else "canvas")
    # the hoops under the canvas
    for hx in (9, 13, 17):
        for y in range(y0 + 1, y1 + 1):
            px[(hx, y)] = "hoop"
    # bed of the wagon
    rect(px, 3, 17, 24, 17, "ink")
    rect(px, 3, 18, 24, 19, "wood")
    rect(px, 3, 20, 24, 20, "wood2")
    px[(3, 18)] = px[(3, 19)] = px[(3, 20)] = "ink"
    px[(24, 18)] = px[(24, 19)] = px[(24, 20)] = "ink"
    # disselboom (the pole) to the right
    rect(px, 25, 19, 30, 19, "ink")
    # wheels: back (left) big, front (right) smaller: iron tyre, wooden felloe, spokes, hub
    for cx, cy, r, n in ((8.5, 22.5, 5.6, 8), (21.0, 23.5, 4.6, 4)):
        ring(px, cx, cy, r, r - 1.0, "ink")
        ring(px, cx, cy, r - 1.0, r - 2.0, "hub")
        for y in range(N):
            for x in range(N):
                dx, dy = x - cx, y - cy
                if dx * dx + dy * dy <= (r - 2.0) ** 2:
                    on = abs(dx) < 0.5 or abs(dy) < 0.5
                    if n == 8:
                        on = on or abs(abs(dx) - abs(dy)) < 0.5
                    if on:
                        px[(x, y)] = "hub"
        disc(px, cx, cy, 1.1, "ink")


RIM = {xy for xy, key in tile().items() if key == "rim"}


def tile_rim(x, y):
    return (x, y) in RIM


def render():
    px = tile()
    inside = set(px)
    wagon(px)
    img = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    for (x, y) in inside:
        # the rim stays on top of anything that runs over the edge; nothing is drawn outside the tile
        key = "rim" if tile_rim(x, y) else px[(x, y)]
        img.putpixel((x, y), PAL[key] + (255,))
    return img


def main():
    os.makedirs(OUT, exist_ok=True)
    img = render()
    img.save(os.path.join(OUT, "icon_32.png"))
    big = img.resize((256, 256), Image.NEAREST)
    big.save(os.path.join(OUT, "icon.png"))
    sizes = []
    for s in (16, 24, 32, 48, 64, 128, 256):
        if s % N == 0:
            sizes.append(img.resize((s, s), Image.NEAREST))
        else:
            sizes.append(big.resize((s, s), Image.LANCZOS))
    sizes[-1].save(os.path.join(OUT, "icon.ico"), format="ICO", sizes=[(s.width, s.height) for s in sizes],
                   append_images=sizes[:-1])
    print("wrote", OUT)


if __name__ == "__main__":
    main()
