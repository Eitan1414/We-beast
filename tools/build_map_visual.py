#!/usr/bin/env python3
"""Bake a flat textured map into a coloured WBM1 grid for early Wii U GX2 tests.

This is an interim renderer-friendly asset: the source texture is sampled into
vertex colours, so the map is recognizable before the full GX2 texture path is
implemented. Requires Pillow (`python -m pip install pillow`).
"""

import argparse
import io
import struct
from pathlib import Path

from PIL import Image

from glb_to_wbm import accessor_values, load_glb, scene_nodes, transform_point


def extract_base_colour_image(gltf, blob):
    materials = gltf.get("materials", [])
    textures = gltf.get("textures", [])
    images = gltf.get("images", [])

    for material in materials:
        pbr = material.get("pbrMetallicRoughness", {})
        texture_info = pbr.get("baseColorTexture")
        if texture_info is None:
            continue
        texture = textures[texture_info["index"]]
        image = images[texture["source"]]
        view = gltf["bufferViews"][image["bufferView"]]
        offset = view.get("byteOffset", 0)
        raw = blob[offset : offset + view["byteLength"]]
        factor = pbr.get("baseColorFactor", [1, 1, 1, 1])
        return Image.open(io.BytesIO(raw)).convert("RGBA"), factor

    raise ValueError("No embedded baseColorTexture found")


def mesh_bounds(gltf, blob):
    points = []
    for node_index, world in scene_nodes(gltf):
        node = gltf["nodes"][node_index]
        if "mesh" not in node:
            continue
        for primitive in gltf["meshes"][node["mesh"]].get("primitives", []):
            positions = accessor_values(gltf, blob, primitive["attributes"]["POSITION"])
            points.extend(transform_point(world, p) for p in positions)

    if not points:
        raise ValueError("No mesh positions found")

    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    zs = [p[2] for p in points]
    return min(xs), min(ys), min(zs), max(xs), max(ys), max(zs)


def build(source, destination, grid):
    if grid < 8 or grid > 128:
        raise ValueError("grid must be between 8 and 128")

    gltf, blob = load_glb(source)
    image, factor = extract_base_colour_image(gltf, blob)
    min_x, min_y, min_z, max_x, max_y, max_z = mesh_bounds(gltf, blob)

    vertices = []
    indices = []

    for row in range(grid + 1):
        v = row / grid
        z = min_z + (max_z - min_z) * v
        py = min(image.height - 1, round(v * (image.height - 1)))
        for column in range(grid + 1):
            u = column / grid
            x = min_x + (max_x - min_x) * u
            px = min(image.width - 1, round(u * (image.width - 1)))
            colour = image.getpixel((px, py))
            rgba = tuple(
                max(0, min(255, round(colour[i] * factor[i])))
                for i in range(4)
            )
            vertices.append((x, max_y, z, 1.0, *rgba, u, v))

    stride = grid + 1
    for row in range(grid):
        for column in range(grid):
            a = row * stride + column
            b = a + 1
            d = (row + 1) * stride + column
            c = d + 1
            # Winding chosen so the top-down renderer sees +Y facing triangles.
            indices.extend((a, d, c, a, c, b))

    flags = 0x0F  # colour + UV + texture-baked visual-grid marker
    output = bytearray(
        struct.pack(
            "<4sIIII6f",
            b"WBM1", 1, len(vertices), len(indices), flags,
            min_x, max_y, min_z, max_x, max_y, max_z,
        )
    )
    for vertex in vertices:
        output += struct.pack("<4f4B2f", *vertex)
    output += struct.pack("<%dI" % len(indices), *indices)

    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)
    print(
        f"{destination}: {len(vertices)} vertices, {len(indices)//3} triangles, "
        f"{len(output)/1024:.1f} KiB"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="Map GLB")
    parser.add_argument("output", help="Output map_01.wbm")
    parser.add_argument("--grid", type=int, default=64)
    args = parser.parse_args()
    build(args.input, args.output, args.grid)
