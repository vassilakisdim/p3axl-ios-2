#!/usr/bin/env python3
"""
Build the "XNUP" payload container consumed by Mu-Silicium's BootShim.S and XnuLoader.c:
  header : u32 magic "XNUP" (0x50554E58), u32 size (bytes after header)
  entries: char name[16], u32 size, u32 reserved, data padded to 16 bytes
  end    : 24 zero bytes
"""
import struct, sys

if len(sys.argv) < 3:
    raise SystemExit("usage: mkxnup.py OUT.bin name=path [name=path ...]")

out_path = sys.argv[1]
body = bytearray()

for arg in sys.argv[2:]:
    name, path = arg.split("=", 1)
    data = open(path, "rb").read()
    body += struct.pack("<16sII", name.encode(), len(data), 0)
    body += data
    body += b"\0" * (-len(data) % 16)

body += b"\0" * 24
blob = struct.pack("<II", 0x50554E58, len(body)) + bytes(body)
open(out_path, "wb").write(blob)
print(f"{out_path}: {len(blob)} bytes")
