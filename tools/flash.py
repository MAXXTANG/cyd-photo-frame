#!/usr/bin/env python3
"""Install a CYD Photo Frame release with identity checks and full backup.

The --install flag explicitly opts into replacing the current application and
image partition. Other programs' NVS keys are left in place. No skip-backup flag.
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

ROOT = Path(__file__).resolve().parents[1]
OFFSETS = {"bootloader.bin": "0x1000", "partitions.bin": "0x8000", "firmware.bin": "0x10000", "littlefs.bin": "0x190000"}


def target_ok(identity):
    match = re.search(r"Chip is (ESP32[^\r\n]*)", identity)
    chip = match.group(1) if match else ""
    classic = chip == "ESP32" or chip.startswith("ESP32-D") or chip.startswith("ESP32-PICO")
    return classic and bool(re.search(r"Detected flash size: 4MB(?:\r?\n|$)", identity))


def check_package(package):
    manifest = json.loads((package / "manifest.json").read_text())
    if manifest.get("chip") != "esp32" or manifest.get("flash_mb") != 4 or manifest.get("offsets") != OFFSETS:
        raise RuntimeError("Package is not the expected ESP32/4MB photo frame layout")
    for name in OFFSETS:
        data = (package / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != manifest["sha256"].get(name):
            raise RuntimeError("Package checksum mismatch: " + name)
    if (package / "littlefs.bin").stat().st_size != 0x270000:
        raise RuntimeError("Filesystem size mismatch")


def run(command, capture=False):
    try:
        result = subprocess.run(command, check=True, text=True, stdout=subprocess.PIPE if capture else None, stderr=subprocess.STDOUT if capture else None)
    except subprocess.CalledProcessError as error:
        if capture and error.stdout:
            print(error.stdout, end="", flush=True)
        raise
    if capture:
        print(result.stdout, end="", flush=True)
        return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="Confirmed CYD USB serial port")
    parser.add_argument("--variant", choices=["cyd-st7789", "cyd-ili9341"], default="cyd-st7789")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--install", action="store_true", help="Replace current app and image storage with this release")
    mode.add_argument("--update-app", action="store_true", help="Back up and update only app; requires matching partition table, preserves images")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--esptool", type=Path, help="Path to an existing esptool.py; otherwise use the installed module")
    args = parser.parse_args()
    package = ROOT / "firmware" / args.variant
    check_package(package)
    if not args.install and not args.update_app:
        raise RuntimeError("Package checks passed. Add --install for a full installation or --update-app to preserve existing images.")
    tool = [sys.executable, str(args.esptool)] if args.esptool else [sys.executable, "-m", "esptool"]
    base = tool + ["--port", args.port, "--baud", str(args.baud)]
    identity = run(base + ["--after", "no_reset", "flash_id"], capture=True)
    if not target_ok(identity):
        raise RuntimeError("Target is not a classic ESP32 with 4MB flash; nothing written")
    backup_dir = ROOT / ".flash-backups"
    backup_dir.mkdir(mode=0o700, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    if args.update_app:
        table = backup_dir / (stamp + "-partitions.bin")
        fd = os.open(table, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        os.close(fd)
        run(base + ["--chip", "esp32", "--after", "no_reset", "read_flash", "0x8000", "0x1000", str(table)])
        expected = (package / "partitions.bin").read_bytes()
        actual = table.read_bytes()
        if len(actual) != 0x1000 or not actual.startswith(expected):
            raise RuntimeError("Partition table differs; app-only update refused")
    backup_size = 0x180000 if args.update_app else 0x400000
    backup_offset = "0x10000" if args.update_app else "0"
    backup = backup_dir / (stamp + ("-before-app.bin" if args.update_app else "-before-photo-frame.bin"))
    # Create with private permissions before esptool opens the file.
    fd = os.open(backup, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    os.close(fd)
    run(base + ["--chip", "esp32", "--after", "no_reset", "read_flash", backup_offset, hex(backup_size), str(backup)])
    if backup.stat().st_size != backup_size:
        raise RuntimeError("Backup incomplete; nothing written")
    print("Original app backup:" if args.update_app else "Full original flash backup:", backup, flush=True)
    pairs = []
    for name, offset in OFFSETS.items():
        if args.update_app and name != "firmware.bin":
            continue
        pairs += [offset, str(package / name)]
    run(base + ["--chip", "esp32", "write_flash", "--flash_mode", "dio", "--flash_freq", "40m", "--flash_size", "4MB"] + pairs)
    print("Installed; esptool verified written data. Confirm display, touch, Wi-Fi and persistence on the board.")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, KeyError, ValueError, subprocess.CalledProcessError) as error:
        print("Stopped:", error, file=sys.stderr)
        sys.exit(1)
