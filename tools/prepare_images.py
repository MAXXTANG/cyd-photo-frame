#!/usr/bin/env python3
"""Convert user-supplied images to the same CYD1 RGB565 format as the browser."""
import argparse
import hashlib
import json
import struct
import zlib
from pathlib import Path
from PIL import Image, ImageOps, ImageDraw


def encode(image, width=240, height=320, cover=False):
    image = ImageOps.exif_transpose(image).convert("RGBA")
    background = Image.new("RGBA", image.size, "black")
    background.alpha_composite(image)
    image = background.convert("RGB")
    if cover:
        result = ImageOps.fit(image, (width, height), method=Image.Resampling.LANCZOS)
    else:
        result = ImageOps.pad(image, (width, height), method=Image.Resampling.LANCZOS, color="black")
    pixels = bytearray()
    quantized = []
    for r, g, b in result.getdata():
        p = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        pixels += struct.pack("<H", p)
        quantized.append((round((p >> 11)*255/31), round(((p >> 5)&63)*255/63), round((p&31)*255/31)))
    preview = Image.new("RGB", (width, height))
    preview.putdata(quantized)
    return struct.pack("<4sHHII", b"CYD1", width, height, len(pixels), zlib.crc32(pixels)) + pixels, preview


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("images", nargs="+")
    p.add_argument("--output", type=Path, default=Path("data"))
    p.add_argument("--previews", type=Path, default=Path("docs/previews"))
    p.add_argument("--landscape", action="store_true")
    p.add_argument("--portrait", action="store_true")
    p.add_argument("--cover", action="store_true", help="Crop to fill instead of preserving the whole image")
    args = p.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    args.previews.mkdir(parents=True, exist_ok=True)
    seen, records, thumbs = set(), [], []
    for source in args.images:
        with Image.open(source) as image:
            digest = hashlib.sha256(image.convert("RGBA").tobytes()).hexdigest()
            if digest in seen:
                continue
            seen.add(digest)
            oriented=ImageOps.exif_transpose(image)
            landscape=args.landscape or (not args.portrait and oriented.width>oriented.height)
            width, height = (320, 240) if landscape else (240, 320)
            data, preview = encode(image, width, height, args.cover)
        index = len(records)
        if index >= 12:
            raise SystemExit("At most 12 distinct images")
        target = args.output / f"p{index}.cyd"
        target.write_bytes(data)
        preview.save(args.previews / f"p{index}.png")
        records.append({"id": index, "source": Path(source).name, "width":width,"height":height,"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
        thumbs.append(preview)
    (args.previews / "manifest.json").write_text(json.dumps(records, indent=2, ensure_ascii=False)+"\n")
    sheet = Image.new("RGB", (len(thumbs)*340+20, 380), "#f5f1e8")
    draw = ImageDraw.Draw(sheet)
    for i, im in enumerate(thumbs):
        sheet.paste(im, (20+340*i+(320-im.width)//2, 20+(320-im.height)//2))
        draw.text((20+340*i, 350), f"{i+1:02} / RGB565 / {im.width} x {im.height}", fill="#24211c")
    sheet.save(args.previews.parent / "sample-contact-sheet.png")
    print(f"Prepared {len(records)} unique images; total {sum(r['bytes'] for r in records)} bytes")


if __name__ == "__main__":
    main()
