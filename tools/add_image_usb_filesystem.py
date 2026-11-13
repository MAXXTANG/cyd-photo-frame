#!/usr/bin/env python3
"""Add an image to an existing frame via a backed-up, verified LittleFS update.

Firmware, NVS and partition table are not written. Existing file contents must
survive a pack/unpack SHA-256 comparison before any flash write occurs.
"""
import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from flash import ROOT, run, target_ok
from replace_usb_image import validate

SIZE=0x270000


def hashes(folder):
    return {str(p.relative_to(folder)):hashlib.sha256(p.read_bytes()).hexdigest()
            for p in folder.rglob('*') if p.is_file()}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True)
    parser.add_argument('--expected-mac',required=True)
    parser.add_argument('--esptool',type=Path,required=True)
    parser.add_argument('--mklittlefs',type=Path,required=True)
    parser.add_argument('--image',type=Path,required=True)
    parser.add_argument('--report',type=Path,required=True)
    args=parser.parse_args()
    image=args.image.read_bytes();width,height,crc=validate(image)
    base=[sys.executable,str(args.esptool),'--port',args.port,'--baud','460800','--chip','esp32']
    identity=run(base+['--after','no_reset','flash_id'],capture=True)
    if not target_ok(identity) or not re.search(r'MAC: '+re.escape(args.expected_mac)+r'(?:\r?\n|$)',identity,re.I):
        raise RuntimeError('Device identity mismatch; nothing written')
    folder=ROOT/'.flash-backups'/('add-image-'+datetime.now().strftime('%Y%m%d-%H%M%S'))
    folder.mkdir(mode=0o700,parents=True)
    table=folder/'partitions.bin'
    run(base+['--after','no_reset','read_flash','0x8000','0x1000',str(table)])
    if table.stat().st_size!=4096 or not table.read_bytes().startswith((ROOT/'firmware/cyd-st7789/partitions.bin').read_bytes()):
        raise RuntimeError('Partition table mismatch; nothing written')
    backup=folder/'before-littlefs.bin'
    run(base+['--after','no_reset','read_flash','0x190000',hex(SIZE),str(backup)])
    if backup.stat().st_size!=SIZE:raise RuntimeError('Incomplete backup; nothing written')
    contents=folder/'contents';contents.mkdir()
    mk=[str(args.mklittlefs),'-b','4096','-p','256','-s',str(SIZE)]
    run(mk+['-u',str(contents),str(backup)])
    before=hashes(contents)
    for name in before:
        if re.fullmatch(r'p\d+\.cyd',name):validate((contents/name).read_bytes())
    occupied={int(m.group(1)) for name in before if (m:=re.fullmatch(r'p(\d+)\.cyd',name))}
    free=[i for i in range(12) if i not in occupied]
    if not free:raise RuntimeError('Album full; nothing written')
    if hashlib.sha256(image).hexdigest() in before.values():raise RuntimeError('Image already exists; nothing written')
    slot=free[0];name=f'p{slot}.cyd';(contents/name).write_bytes(image)
    after=hashes(contents)
    rebuilt=folder/'after-littlefs.bin'
    run(mk+['-a','-c',str(contents),str(rebuilt)])
    check=folder/'verified';check.mkdir()
    run(mk+['-u',str(check),str(rebuilt)])
    if rebuilt.stat().st_size!=SIZE or hashes(check)!=after or any(after.get(name)!=digest for name,digest in before.items()):
        raise RuntimeError('Existing image preservation check failed; nothing written')
    report={'slot':slot,'width':width,'height':height,'crc':f'{crc:08x}',
            'before_images':len(occupied),'after_images':len(occupied)+1,
            'existing_files_preserved':True,'new_image_sha256':hashlib.sha256(image).hexdigest(),
            'backup_sha256':hashlib.sha256(backup.read_bytes()).hexdigest()}
    print('Pre-write verification:',json.dumps(report),flush=True)
    run(base+['write_flash','--flash_mode','keep','--flash_freq','keep','--flash_size','keep','0x190000',str(rebuilt)])
    for file in folder.rglob('*'):
        if file.is_file():os.chmod(file,0o600)
    args.report.write_text(json.dumps(report,indent=2)+'\n')
    print('Added image:',json.dumps(report),flush=True)


if __name__=='__main__':
    try:main()
    except (RuntimeError,OSError,ValueError,subprocess.CalledProcessError) as error:
        print('Stopped:',error,file=sys.stderr);sys.exit(1)
