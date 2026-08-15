#!/usr/bin/env python3
"""Convert simple GLB 2.0 meshes to the We Beast WBM1 runtime format.

WBM1 intentionally keeps the runtime loader tiny on Wii U:
- indexed triangle mesh;
- float4 positions;
- RGBA8 vertex colours;
- float2 UVs reserved for the textured renderer.

The source GLB remains the editable/master asset. WBM is a generated runtime asset.
"""

import argparse
import json
import struct
from pathlib import Path

COMPONENT = {
    5120: ("b", 1),
    5121: ("B", 1),
    5122: ("h", 2),
    5123: ("H", 2),
    5125: ("I", 4),
    5126: ("f", 4),
}
TYPE_COUNT = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def load_glb(path):
    data = Path(path).read_bytes()
    magic, version, length = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2 or length > len(data):
        raise ValueError("Only GLB v2 is supported")

    offset = 12
    gltf = None
    bin_chunk = None
    while offset + 8 <= length:
        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        chunk = data[offset : offset + chunk_length]
        offset += chunk_length
        if chunk_type == 0x4E4F534A:
            gltf = json.loads(chunk.rstrip(b"\0 ").decode("utf-8"))
        elif chunk_type == 0x004E4942:
            bin_chunk = chunk

    if gltf is None or bin_chunk is None:
        raise ValueError("GLB requires JSON and BIN chunks")
    return gltf, bin_chunk


def accessor_values(gltf, blob, index):
    accessor = gltf["accessors"][index]
    view = gltf["bufferViews"][accessor["bufferView"]]
    if accessor.get("sparse"):
        raise ValueError("Sparse accessors are not supported")

    fmt, component_size = COMPONENT[accessor["componentType"]]
    component_count = TYPE_COUNT[accessor["type"]]
    stride = view.get("byteStride", component_size * component_count)
    base = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    normalized = accessor.get("normalized", False)
    unpacker = struct.Struct("<" + fmt * component_count)

    result = []
    for i in range(accessor["count"]):
        values = unpacker.unpack_from(blob, base + i * stride)
        if normalized and accessor["componentType"] != 5126:
            component_type = accessor["componentType"]
            if component_type == 5121:
                values = tuple(v / 255.0 for v in values)
            elif component_type == 5123:
                values = tuple(v / 65535.0 for v in values)
            elif component_type == 5120:
                values = tuple(max(-1.0, v / 127.0) for v in values)
            elif component_type == 5122:
                values = tuple(max(-1.0, v / 32767.0) for v in values)
        result.append(values)
    return result


def identity_matrix():
    return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]


def node_matrix(node):
    if "matrix" in node:
        return [float(x) for x in node["matrix"]]

    translation = node.get("translation", [0, 0, 0])
    scale = node.get("scale", [1, 1, 1])
    x, y, z, w = node.get("rotation", [0, 0, 0, 1])

    r00 = 1 - 2 * (y * y + z * z)
    r01 = 2 * (x * y + z * w)
    r02 = 2 * (x * z - y * w)
    r10 = 2 * (x * y - z * w)
    r11 = 1 - 2 * (x * x + z * z)
    r12 = 2 * (y * z + x * w)
    r20 = 2 * (x * z + y * w)
    r21 = 2 * (y * z - x * w)
    r22 = 1 - 2 * (x * x + y * y)

    return [
        r00 * scale[0], r10 * scale[0], r20 * scale[0], 0,
        r01 * scale[1], r11 * scale[1], r21 * scale[1], 0,
        r02 * scale[2], r12 * scale[2], r22 * scale[2], 0,
        translation[0], translation[1], translation[2], 1,
    ]


def matrix_multiply(a, b):
    out = [0.0] * 16
    for column in range(4):
        for row in range(4):
            out[column * 4 + row] = sum(
                a[k * 4 + row] * b[column * 4 + k] for k in range(4)
            )
    return out


def transform_point(matrix, point):
    x, y, z = point[:3]
    return (
        matrix[0] * x + matrix[4] * y + matrix[8] * z + matrix[12],
        matrix[1] * x + matrix[5] * y + matrix[9] * z + matrix[13],
        matrix[2] * x + matrix[6] * y + matrix[10] * z + matrix[14],
    )


def scene_nodes(gltf):
    scene_index = gltf.get("scene", 0)
    fallback = {"nodes": list(range(len(gltf.get("nodes", []))))}
    roots = gltf.get("scenes", [fallback])[scene_index].get("nodes", [])
    nodes = gltf.get("nodes", [])
    result = []

    def visit(index, parent):
        world = matrix_multiply(parent, node_matrix(nodes[index]))
        result.append((index, world))
        for child in nodes[index].get("children", []):
            visit(child, world)

    for root in roots:
        visit(root, identity_matrix())
    return result


def material_factor(gltf, material_index):
    if material_index is None:
        return (1, 1, 1, 1)
    material = gltf.get("materials", [])[material_index]
    return tuple(
        material.get("pbrMetallicRoughness", {}).get(
            "baseColorFactor", [1, 1, 1, 1]
        )
    )


def convert(source, destination):
    gltf, blob = load_glb(source)
    vertices = []
    indices = []

    for node_index, world in scene_nodes(gltf):
        node = gltf["nodes"][node_index]
        if "mesh" not in node:
            continue

        mesh = gltf["meshes"][node["mesh"]]
        for primitive in mesh.get("primitives", []):
            if primitive.get("mode", 4) != 4:
                raise ValueError("Only triangle primitives are supported")

            attributes = primitive.get("attributes", {})
            positions = accessor_values(gltf, blob, attributes["POSITION"])
            colours = (
                accessor_values(gltf, blob, attributes["COLOR_0"])
                if "COLOR_0" in attributes
                else None
            )
            uvs = (
                accessor_values(gltf, blob, attributes["TEXCOORD_0"])
                if "TEXCOORD_0" in attributes
                else None
            )
            factor = material_factor(gltf, primitive.get("material"))
            base_vertex = len(vertices)

            for i, position in enumerate(positions):
                x, y, z = transform_point(world, position)
                colour = colours[i] if colours else (1, 1, 1, 1)
                if len(colour) == 3:
                    colour = (*colour, 1.0)
                rgba = tuple(
                    max(0, min(255, round(colour[k] * factor[k] * 255)))
                    for k in range(4)
                )
                uv = uvs[i] if uvs else (0.0, 0.0)
                vertices.append((x, y, z, 1.0, *rgba, float(uv[0]), float(uv[1])))

            if "indices" in primitive:
                primitive_indices = accessor_values(gltf, blob, primitive["indices"])
                indices.extend(base_vertex + int(v[0]) for v in primitive_indices)
            else:
                indices.extend(range(base_vertex, base_vertex + len(positions)))

    if not vertices:
        raise ValueError("No mesh vertices found")

    xs = [v[0] for v in vertices]
    ys = [v[1] for v in vertices]
    zs = [v[2] for v in vertices]
    bounds = (min(xs), min(ys), min(zs), max(xs), max(ys), max(zs))

    # Header: magic, version, vertexCount, indexCount, flags, AABB[6].
    # Vertex: float4 position + RGBA8 + float2 UV = 28 bytes.
    output = bytearray(
        struct.pack(
            "<4sIIII6f",
            b"WBM1",
            1,
            len(vertices),
            len(indices),
            0x3,
            *bounds,
        )
    )

    for vertex in vertices:
        output += struct.pack(
            "<4f4B2f",
            vertex[0], vertex[1], vertex[2], vertex[3],
            vertex[4], vertex[5], vertex[6], vertex[7],
            vertex[8], vertex[9],
        )
    output += struct.pack("<%dI" % len(indices), *indices)

    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)

    print(
        f"{source} -> {destination}: {len(vertices)} vertices, "
        f"{len(indices) // 3} triangles, {len(output) / 1024:.1f} KiB"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Convert a simple GLB 2.0 mesh to We Beast WBM1."
    )
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()
    convert(args.input, args.output)
