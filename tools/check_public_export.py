#!/usr/bin/env python3
"""Check tracked/staged paths for local artifacts before publishing."""
from pathlib import Path
import re
import subprocess

root=Path(__file__).resolve().parents[1]
files=subprocess.check_output(['git','ls-files','-z'],cwd=root).decode().split('\0')
blocked=[]
for name in filter(None,files):
    path=Path(name)
    private=any(p in {'.flash-backups','.pio','.venv','node_modules','private'} for p in path.parts)
    private|=name.startswith(('firmware/','docs/previews/'))
    private|=name.startswith('data/') and name!='data/README.md'
    private|=path.suffix=='.log'
    if private:blocked.append(name);continue
    data=(root/path).read_bytes()
    if path.suffix.lower() in {'.png','.jpg','.jpeg','.cyd'}:continue
    if re.search(rb'(?:gh[pousr]_[A-Za-z0-9]{30,}|sk-proj-[A-Za-z0-9_-]{20,}|-----BEGIN (?:RSA |OPENSSH )?PRIVATE KEY-----)',data):blocked.append(name)
    if b'/Users/' in data and path.name!='check_public_export.py':blocked.append(name)
if blocked:
    raise SystemExit('Private artifacts found: '+', '.join(sorted(set(blocked))))
print('Public export check passed:',len(list(filter(None,files))),'tracked files')
