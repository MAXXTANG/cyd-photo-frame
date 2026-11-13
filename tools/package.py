#!/usr/bin/env python3
"""Package locally built firmware, filesystem and SHA-256 manifests."""
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OFFSETS = {"bootloader.bin": "0x1000", "partitions.bin": "0x8000", "firmware.bin": "0x10000", "littlefs.bin": "0x190000"}


def main():
    for variant in ("cyd-st7789", "cyd-ili9341"):
        target = ROOT / "firmware" / variant
        target.mkdir(parents=True, exist_ok=True)
        for name in OFFSETS:
            source = ROOT / ".pio/build" / variant / name
            if name == "littlefs.bin":
                source = ROOT / ".pio/build/cyd-st7789/littlefs.bin"
            shutil.copyfile(source, target / name)
        manifest = {"version": "0.2.0", "variant": variant, "chip": "esp32", "flash_mb": 4,
                    "offsets": OFFSETS, "sha256": {name: hashlib.sha256((target/name).read_bytes()).hexdigest() for name in OFFSETS}}
        (target / "manifest.json").write_text(json.dumps(manifest, indent=2)+"\n")
        print(variant, (target / "firmware.bin").stat().st_size, "app bytes")


if __name__ == "__main__":
    main()
