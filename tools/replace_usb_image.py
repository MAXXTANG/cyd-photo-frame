#!/usr/bin/env python3
"""Replace one known CYD image over USB, using a CRC compare and atomic commit."""
import argparse
import json
from pathlib import Path
import struct
import time
import zlib
import serial


def validate(data):
    if len(data)!=153616:
        raise ValueError("Expected one 153616-byte CYD image")
    magic,w,h,n,crc=struct.unpack("<4sHHII",data[:16])
    if magic!=b"CYD1" or (w,h) not in ((240,320),(320,240)) or n!=153600 or zlib.crc32(data[16:])!=crc:
        raise ValueError("Invalid image dimensions/header/CRC")
    return w,h,crc


def wait_line(conn,prefix,seconds=10):
    end=time.monotonic()+seconds
    while time.monotonic()<end:
        line=conn.readline().decode("utf-8",errors="replace").strip()
        if line.startswith("ERR"):
            raise RuntimeError(line)
        if line.startswith(prefix):
            return line
    raise RuntimeError("Timed out waiting for "+prefix)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--port",required=True)
    p.add_argument("--slot",type=int,required=True)
    p.add_argument("--old-crc",required=True,help="Expected previous image CRC32, hexadecimal")
    p.add_argument("--image",type=Path,required=True)
    p.add_argument("--report",type=Path,required=True)
    args=p.parse_args()
    data=args.image.read_bytes();w,h,crc=validate(data)
    old_crc=int(args.old_crc,16)
    if not 0<=args.slot<12 or not 0<=old_crc<=0xffffffff:
        raise ValueError("Invalid slot/CRC")
    conn=serial.Serial(port=None,baudrate=115200,timeout=0.5,write_timeout=5)
    conn.dtr=False;conn.rts=False;conn.port=args.port;conn.open()
    with conn:
        conn.rts=True;time.sleep(0.15);conn.rts=False
        time.sleep(4)
        conn.reset_input_buffer()
        conn.write(f"U{args.slot} {len(data)} {old_crc:x}\n".encode())
        wait_line(conn,"READY")
        for offset in range(0,len(data),1024):
            block=data[offset:offset+1024]
            conn.write(block);conn.flush()
            ack=wait_line(conn,"ACK ")
            if ack!=f"ACK {offset+len(block)}":
                raise RuntimeError("Unexpected acknowledgement: "+ack)
        result=wait_line(conn,"OK ")
        if result!=f"OK {args.slot} {w} {h}":
            raise RuntimeError("Unexpected commit response: "+result)
    report={"slot":args.slot,"old_crc":f"{old_crc:08x}","new_crc":f"{crc:08x}","width":w,"height":h,"bytes":len(data),"response":result}
    args.report.write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps(report),flush=True)


if __name__=="__main__":
    main()
