#!/usr/bin/env python3
"""Read the installed frame's USB diagnostics without exposing Wi-Fi credentials."""
import argparse
import json
from pathlib import Path
import time
import serial


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port", required=True)
    p.add_argument("--seconds", type=float, default=60)
    p.add_argument("--reset", action="store_true", help="Reset over RTS before observing startup")
    p.add_argument("--expect-auto", action="store_true", help="Require portrait and landscape dimensions in automatic mode")
    p.add_argument("--expected-count", type=int, default=5)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    conn = serial.Serial(port=None, baudrate=115200, timeout=0.2)
    conn.dtr = False
    conn.rts = False
    conn.port = args.port
    conn.open()
    # Release the BOOT control line after opening, before any RTS reset.
    conn.dtr = False
    samples, boot = [], []
    try:
        if args.reset:
            conn.rts = True
            time.sleep(0.15)
            conn.rts = False
        start = time.monotonic()
        next_query = start + (3 if args.reset else 0.5)
        while time.monotonic() - start < args.seconds:
            now = time.monotonic()
            if now >= next_query:
                conn.write(b"J")
                next_query = now + 2
            line = conn.readline().decode("utf-8", errors="replace").strip()
            if not line:
                continue
            if line.startswith("{"):
                try:
                    status = json.loads(line)
                except ValueError:
                    continue
                if {"count", "current", "heap", "storage"} <= status.keys():
                    status["seconds"] = round(time.monotonic()-start, 3)
                    samples.append(status)
                    print(json.dumps(status), flush=True)
            elif "CYD Photo Frame" in line or "rst:" in line or "error" in line.lower() or "panic" in line.lower():
                boot.append(line)
                print(line, flush=True)
    finally:
        conn.close()
    summary = {"samples": samples, "boot": boot, "reset": args.reset,
               "all_storage_mounted": bool(samples) and all(s["storage"] == 1 for s in samples),
               "all_five_images": bool(samples) and all(s["count"] == 5 for s in samples),
               "expected_count": args.expected_count,
               "all_expected_images": bool(samples) and all(s["count"] == args.expected_count for s in samples),
               "observed_indices": sorted({s["current"] for s in samples}),
               "heap_min": min((s["heap"] for s in samples), default=None),
               "heap_max": max((s["heap"] for s in samples), default=None)}
    summary["observed_orientations"] = sorted({(s["width"],s["height"]) for s in samples if "width" in s and "height" in s})
    summary["auto_mode"] = bool(samples) and all(s.get("mode")==4 for s in samples)
    args.output.write_text(json.dumps(summary, indent=2)+"\n")
    print("SUMMARY", json.dumps({k: v for k, v in summary.items() if k != "samples"}), flush=True)
    if not summary["all_storage_mounted"] or not summary["all_expected_images"]:
        raise SystemExit("Storage/image boot checks failed; inspect the saved report")
    if args.expect_auto and (not summary["auto_mode"] or summary["observed_orientations"] != [(240,320),(320,240)]):
        raise SystemExit("Automatic portrait/landscape transition not observed")


if __name__ == "__main__":
    main()
