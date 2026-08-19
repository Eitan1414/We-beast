#!/usr/bin/env python3
"""Generate a tiny flat Map 2 GLB for real-Wii-U hardware tests.

This is only a fallback. The Wii U workflow prefers the real Map 2 GLB when
assets/source/maps/map_02.glb (or map 2.glb) exists.
"""

import json
import struct
from pathlib import Path


def build_glb(path: Path) -> None:
    positions = [
        (-6.0, 0.0, -6.0),
        (6.0, 0.0, -6.0),
        (6.0, 0.0, 6.0),
        (-6.0, 0.0, 6.0),
    ]
    # Winding chosen so the current top-down renderer keeps the triangles.
    indices = [0, 2, 1, 0, 3, 2]

    binary = bytearray()
    for position in positions:
        binary += struct.pack("<3f", *position)

    index_offset = len(binary)
    for index in indices:
        binary += struct.pack("<H", index)

    while len(binary) % 4:
        binary += b"\0"

    gltf = {
        "asset": {"version": "2.0", "generator": "We Beast fallback map generator"},
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": [
            {
                "buffer": 0,
                "byteOffset": 0,
                "byteLength": len(positions) * 12,
                "target": 34962,
            },
            {
                "buffer": 0,
                "byteOffset": index_offset,
                "byteLength": len(indices) * 2,
                "target": 34963,
            },
        ],
        "accessors": [
            {
                "bufferView": 0,
                "componentType": 5126,
                "count": len(positions),
                "type": "VEC3",
                "min": [-6.0, 0.0, -6.0],
                "max": [6.0, 0.0, 6.0],
            },
            {
                "bufferView": 1,
                "componentType": 5123,
                "count": len(indices),
                "type": "SCALAR",
            },
        ],
        "meshes": [
            {
                "primitives": [
                    {
                        "attributes": {"POSITION": 0},
                        "indices": 1,
                        "mode": 4,
                    }
                ]
            }
        ],
        "nodes": [{"mesh": 0}],
        "scenes": [{"nodes": [0]}],
        "scene": 0,
    }

    json_bytes = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    while len(json_bytes) % 4:
        json_bytes += b" "

    total_length = 12 + 8 + len(json_bytes) + 8 + len(binary)
    glb = bytearray(struct.pack("<III", 0x46546C67, 2, total_length))
    glb += struct.pack("<II", len(json_bytes), 0x4E4F534A)
    glb += json_bytes
    glb += struct.pack("<II", len(binary), 0x004E4942)
    glb += binary

    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(glb)
    print(f"Generated fallback Map 2 GLB: {path} ({len(glb)} bytes)")


if __name__ == "__main__":
    build_glb(Path("build-runtime/map_02_fallback.glb"))
