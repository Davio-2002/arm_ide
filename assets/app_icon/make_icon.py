import os

import gi

gi.require_version("GdkPixbuf", "2.0")
from gi.repository import GdkPixbuf

here = os.path.dirname(os.path.abspath(__file__))


def render(size):
    source = "icon_small.svg" if size <= 32 else "icon.svg"
    return GdkPixbuf.Pixbuf.new_from_file_at_size(f"{here}/{source}", size, size)


for size in (32, 64, 128, 256, 512):
    render(size).savev(f"{here}/icon_{size}.png", "png", [], [])

with open(f"{here}/icon.rgba", "wb") as out:
    for size in (32, 64, 128, 256):
        image = render(size)
        assert image.get_has_alpha() and image.get_n_channels() == 4
        data = image.get_pixels()
        stride = image.get_rowstride()
        for y in range(size):
            out.write(data[y * stride : y * stride + size * 4])
