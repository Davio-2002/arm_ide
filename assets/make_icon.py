import os, struct, sys, zlib

N = 32
img = [[(0, 0, 0, 0)] * N for _ in range(N)]


def hexc(h):
    h = h.lstrip("#")
    return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), 255)


def put(x, y, c):
    img[y][x] = hexc(c)


# background: dark rounded square, slightly lighter at the top, dithered at the seams
R = 5
def inside(x, y):
    cx = min(max(x, R), N - 1 - R)
    cy = min(max(y, R), N - 1 - R)
    return (x - cx) ** 2 + (y - cy) ** 2 <= R * R + 1

steps = ["#32302f", "#282828", "#1d2021", "#181a1b"]
for y in range(N):
    for x in range(N):
        if not inside(x, y):
            continue
        t = y / (N - 1) * (len(steps) - 1)
        i = min(int(t), len(steps) - 2)
        threshold = 0.6 if (x + y) % 2 else 0.85
        put(x, y, steps[i + 1] if t - i > threshold else steps[i])
        if not all(inside(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
            put(x, y, "#0f1011")
for x in range(R + 1, N - R - 1):
    put(x, 1, "#504945")

# assembly code as syntax-coloured bars: a label, then indented mnemonic + operands,
# and a breakpoint dot in the gutter
label, mnemonic, operand, comment = "#fabd2f", "#fe8019", "#8ec07c", "#665c54"
bars = [
    (6, 9, 16, label),
    (10, 12, 15, mnemonic), (10, 17, 21, operand), (10, 23, 25, operand),
    (14, 12, 14, mnemonic), (14, 16, 22, operand),
    (18, 12, 15, mnemonic), (18, 17, 19, operand), (18, 21, 25, comment),
]
for y, x1, x2, _ in bars:
    for x in range(x1, x2 + 1):
        put(x, y + 2, "#111213")
for y, x1, x2, colour in bars:
    for x in range(x1, x2 + 1):
        put(x, y, colour)
        put(x, y + 1, colour)
for x, y in ((5, 14), (6, 14), (5, 15), (6, 15)):
    put(x, y, "#fb4934")
put(5, 14, "#fe9a8a")

# "ARM" label, centred under the prompt
font = {
    "A": [".#.", "#.#", "###", "#.#", "#.#"],
    "R": ["##.", "#.#", "##.", "#.#", "#.#"],
    "M": ["#...#", "##.##", "#.#.#", "#...#", "#...#"],
}
x0 = 10
for ch in "ARM":
    for dy, row in enumerate(font[ch]):
        for dx, bit in enumerate(row):
            if bit == "#":
                put(x0 + dx, 24 + dy + 1, "#111213")
                put(x0 + dx, 24 + dy, "#a89984")
    x0 += len(font[ch][0]) + 1

out = sys.argv[1]
os.makedirs(out, exist_ok=True)


def save(path, scale):
    s = N * scale
    rows = b"".join(
        b"\0" + b"".join(bytes(img[y // scale][x // scale]) for x in range(s)) for y in range(s))
    def chunk(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d))
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", s, s, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    open(path, "wb").write(png)


for sc in (1, 2, 4, 8, 16):
    save(f"{out}/icon_{N * sc}.png", sc)
open(f"{out}/icon_{N}.rgba", "wb").write(b"".join(bytes(p) for row in img for p in row))
