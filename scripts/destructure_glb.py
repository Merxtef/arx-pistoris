#!/usr/bin/env python3
import struct
import sys
from pathlib import Path

def destructure_glb(glb_path, out_path):
    with open(glb_path, "rb") as f:
        data = f.read()

    if len(data) < 20:
        return False

    magic, version, length = struct.unpack_from("<III", data, 0)
    if magic != 0x46546C67 or version != 2 or length != len(data):  # 'glTF'
        return False

    pos = 12
    json_chunk = b""
    bin_chunk = b""

    while pos + 8 <= len(data):
        chunk_len, chunk_type = struct.unpack_from("<II", data, pos)
        pos += 8
        if pos + chunk_len > len(data):
            return False
        if chunk_type == 0x4E4F534A:  # 'JSON'
            json_chunk = data[pos : pos + chunk_len]
        elif chunk_type == 0x004E4942:  # 'BIN\0'
            bin_chunk = data[pos : pos + chunk_len]
        pos += chunk_len

    if not json_chunk:
        return False

    with open(out_path, "wb") as f:
        f.write(struct.pack("<I", len(bin_chunk)))
        f.write(json_chunk)
        f.write(bin_chunk)
    return True

def main():
    if len(sys.argv) < 3:
        print("Usage: destructure_glb.py <input_dir> <output_dir>")
        return

    in_dir = Path(sys.argv[1])
    out_dir = Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)

    for glb in in_dir.glob("*.glb"):
        if destructure_glb(glb, out_dir / (glb.stem + ".seed")):
            print(f"Destructured {glb.name} -> {glb.stem}.seed")

if __name__ == "__main__":
    main()
