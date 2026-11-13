#!/usr/bin/env python3
"""Generate original geometric test images; no user photos are used.

Use --output data for optional demo photos in a new installation.
"""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw
from prepare_images import encode


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=Path('test/fixtures'))
    args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    palettes=[('#ddecf4','#005d83'),('#f4e7d1','#d86718'),('#e9e9f5','#50518d'),('#e3f3e6','#22684b'),('#f5e4ec','#9b2759')]
    for i,(bg,fg) in enumerate(palettes):
        w,h=(320,240) if i==3 else (240,320)
        image=Image.new('RGB',(w,h),bg);draw=ImageDraw.Draw(image)
        draw.rectangle((16,16,w-17,h-17),outline=fg,width=4)
        draw.ellipse((w//2-54,h//2-54,w//2+54,h//2+54),fill=fg)
        draw.text((28,32),f'CYD GALLERY {i+1:02}',fill=fg)
        draw.text((28,h-48),f'{w} x {h} / RGB565',fill=fg)
        data,preview=encode(image,w,h,False)
        (args.output/f'p{i}.cyd').write_bytes(data)
        preview.save(args.output/f'p{i}.png')
    print('Generated 5 original demo fixtures in',args.output)


if __name__=='__main__':main()
