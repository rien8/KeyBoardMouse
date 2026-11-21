from PIL import Image, ImageDraw

S = 256
M = 40

ARROW = [(0, 0), (0, 138), (27, 111), (45, 156), (68, 146), (50, 100), (88, 100)]

def base(fill, outline):
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    pts = [(x + M, y + M) for x, y in ARROW]
    d.polygon(pts, fill=fill, outline=outline, width=8)
    return img

on = base((76, 195, 138, 255), (27, 39, 51, 255))
off = base((138, 148, 160, 255), (27, 39, 51, 255))
d = ImageDraw.Draw(off)
d.line([(M - 14, M - 14), (S - M + 24, S - M + 24)], fill=(214, 69, 65, 255), width=16)

sizes = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64)]
on.save("res/mousekey_on.ico", sizes=sizes)
off.save("res/mousekey_off.ico", sizes=sizes)
print("icons written")
