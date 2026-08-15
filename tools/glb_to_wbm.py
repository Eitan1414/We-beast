#!/usr/bin/env python3
"""Convert GLB 2.0 meshes to the compact We Beast WBM1 runtime format.

The normal path uses strict glTF JSON. A compatibility path is included for
Nomad Sculpt exports that contain malformed private metadata: only standard
accessors/buffer views/primitive references are recovered, never Nomad state.
"""

import argparse
import json
import struct
from pathlib import Path

COMPONENT = {
    5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2),
    5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4),
}
TYPE_COUNT = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}
JSON_CHUNK = 0x4E4F534A
BIN_CHUNK = 0x004E4942


def scan_value(text, start):
    while start < len(text) and text[start].isspace():
        start += 1
    if start >= len(text):
        raise ValueError("Missing JSON value")

    if text[start] == '"':
        escaped = False
        for i in range(start + 1, len(text)):
            ch = text[i]
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                return i + 1
        raise ValueError("Unterminated JSON string")

    if text[start] in "[{":
        stack = [text[start]]
        pairs = {"]": "[", "}": "{"}
        in_string = False
        escaped = False
        for i in range(start + 1, len(text)):
            ch = text[i]
            if in_string:
                if escaped:
                    escaped = False
                elif ch == "\\":
                    escaped = True
                elif ch == '"':
                    in_string = False
                continue
            if ch == '"':
                in_string = True
            elif ch in "[{":
                stack.append(ch)
            elif ch in "]}" and stack and stack[-1] == pairs[ch]:
                stack.pop()
                if not stack:
                    return i + 1
        raise ValueError("Unterminated JSON container")

    i = start
    while i < len(text) and text[i] not in ",}":
        i += 1
    return i


def top_members(text):
    """Read root members without validating their internal/private metadata."""
    out = {}
    decoder = json.JSONDecoder()
    i = text.find("{") + 1
    if i <= 0:
        raise ValueError("glTF JSON is not an object")

    while i < len(text):
        while i < len(text) and (text[i].isspace() or text[i] == ","):
            i += 1
        if i >= len(text) or text[i] == "}":
            break
        if text[i] != '"':
            raise ValueError(f"Invalid root member near {i}")
        key, used = decoder.raw_decode(text[i:])
        i += used
        while i < len(text) and text[i].isspace():
            i += 1
        if i >= len(text) or text[i] != ":":
            raise ValueError(f"Missing colon after {key!r}")
        i += 1
        while i < len(text) and text[i].isspace():
            i += 1
        start = i
        end = scan_value(text, start)
        out[key] = text[start:end]
        i = end
    return out


def parse_member(raw, key, required=False):
    value = raw.get(key)
    if value is None:
        if required:
            raise ValueError(f"Missing required glTF field {key!r}")
        return None
    try:
        return json.loads(value)
    except json.JSONDecodeError:
        if required:
            raise ValueError(f"Required glTF field {key!r} is malformed")
        return None


def recover_mesh_primitives(raw_meshes):
    """Extract valid `primitives` arrays from otherwise broken mesh metadata."""
    meshes = []
    needle = '"primitives"'
    pos = 0
    while True:
        key = raw_meshes.find(needle, pos)
        if key < 0:
            break
        colon = key + len(needle)
        while colon < len(raw_meshes) and raw_meshes[colon].isspace():
            colon += 1
        if colon >= len(raw_meshes) or raw_meshes[colon] != ":":
            pos = key + len(needle)
            continue
        start = colon + 1
        while start < len(raw_meshes) and raw_meshes[start].isspace():
            start += 1
        try:
            end = scan_value(raw_meshes, start)
            primitives = json.loads(raw_meshes[start:end])
        except (ValueError, json.JSONDecodeError):
            pos = key + len(needle)
            continue
        if isinstance(primitives, list) and primitives:
            meshes.append({"primitives": primitives})
        pos = max(end, key + len(needle))
    if not meshes:
        raise ValueError("No valid mesh primitives found in Nomad GLB")
    return meshes


def decode_gltf_json(chunk):
    text = chunk.rstrip(b"\0 \t\r\n").decode("utf-8")
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        raw = top_members(text)
        accessors = parse_member(raw, "accessors", True)
        views = parse_member(raw, "bufferViews", True)
        meshes = parse_member(raw, "meshes")
        if not isinstance(meshes, list) or not meshes:
            meshes = recover_mesh_primitives(raw.get("meshes", ""))

        # In the malformed-Nomad recovery path props are normalised by their WBM
        # bounds at runtime, so identity nodes preserve all actual mesh data.
        result = {
            "accessors": accessors,
            "bufferViews": views,
            "meshes": meshes,
            "nodes": [{"mesh": i} for i in range(len(meshes))],
        }
        materials = parse_member(raw, "materials")
        result["materials"] = materials if isinstance(materials, list) else []
        return result


def recover_bin_chunk(data, nominal_limit):
    """Find a valid BIN chunk signature if Nomad's JSON length is off."""
    search_end = min(len(data), nominal_limit + 64)
    marker = data.find(b"BIN\0", 20, search_end)
    while marker >= 4:
        header = marker - 4
        size = struct.unpack_from("<I", data, header)[0]
        payload = marker + 4
        if size > 0 and payload + size <= len(data):
            return data[payload:payload + size]
        marker = data.find(b"BIN\0", marker + 4, search_end)
    return None


def load_glb(path):
    data = Path(path).read_bytes()
    if len(data) < 20:
        raise ValueError("GLB is too small")
    magic, version, declared_length = struct.unpack_from("<4sII", data, 0)
    if magic != b"glTF" or version != 2:
        raise ValueError("Only GLB v2 is supported")

    # Do not reject a Nomad file merely because its outer size has trailing
    # bytes. The individual chunks are validated below.
    limit = min(max(declared_length, 20), len(data))
    offset = 12
    gltf = None
    bin_chunk = None

    while offset + 8 <= limit:
        chunk_length, chunk_type = struct.unpack_from("<II", data, offset)
        offset += 8
        end = offset + chunk_length
        if end > len(data):
            break
        chunk = data[offset:end]
        offset = end
        if chunk_type == JSON_CHUNK and gltf is None:
            gltf = decode_gltf_json(chunk)
        elif chunk_type == BIN_CHUNK and bin_chunk is None:
            bin_chunk = chunk

    # The supplied Nomad 1.10 prop has a shifted BIN header despite usable BIN
    # bytes. Signature recovery is constrained by size validation above.
    if bin_chunk is None:
        bin_chunk = recover_bin_chunk(data, limit)

    if gltf is None or bin_chunk is None:
        raise ValueError("GLB requires recoverable JSON and BIN chunks")
    return gltf, bin_chunk


def accessor_values(gltf, blob, index):
    accessor = gltf["accessors"][index]
    view = gltf["bufferViews"][accessor["bufferView"]]
    if accessor.get("sparse"):
        raise ValueError("Sparse accessors are not supported")

    fmt, component_size = COMPONENT[accessor["componentType"]]
    count = TYPE_COUNT[accessor["type"]]
    stride = view.get("byteStride", component_size * count)
    base = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    unpacker = struct.Struct("<" + fmt * count)
    normalized = accessor.get("normalized", False)

    result = []
    for i in range(accessor["count"]):
        at = base + i * stride
        if at < 0 or at + unpacker.size > len(blob):
            raise ValueError(f"Accessor {index} exceeds BIN chunk")
        values = unpacker.unpack_from(blob, at)
        if normalized and accessor["componentType"] != 5126:
            t = accessor["componentType"]
            if t == 5121:
                values = tuple(v / 255.0 for v in values)
            elif t == 5123:
                values = tuple(v / 65535.0 for v in values)
            elif t == 5120:
                values = tuple(max(-1.0, v / 127.0) for v in values)
            elif t == 5122:
                values = tuple(max(-1.0, v / 32767.0) for v in values)
        result.append(values)
    return result


def identity_matrix():
    return [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]


def node_matrix(node):
    if "matrix" in node:
        return [float(v) for v in node["matrix"]]
    tx, ty, tz = node.get("translation", [0, 0, 0])
    sx, sy, sz = node.get("scale", [1, 1, 1])
    x, y, z, w = node.get("rotation", [0, 0, 0, 1])
    r00, r01, r02 = 1-2*(y*y+z*z), 2*(x*y+z*w), 2*(x*z-y*w)
    r10, r11, r12 = 2*(x*y-z*w), 1-2*(x*x+z*z), 2*(y*z+x*w)
    r20, r21, r22 = 2*(x*z+y*w), 2*(y*z-x*w), 1-2*(x*x+y*y)
    return [
        r00*sx, r10*sx, r20*sx, 0,
        r01*sy, r11*sy, r21*sy, 0,
        r02*sz, r12*sz, r22*sz, 0,
        tx, ty, tz, 1,
    ]


def matrix_multiply(a, b):
    out = [0.0] * 16
    for col in range(4):
        for row in range(4):
            out[col*4+row] = sum(a[k*4+row] * b[col*4+k] for k in range(4))
    return out


def transform_point(m, p):
    x, y, z = p[:3]
    return (
        m[0]*x + m[4]*y + m[8]*z + m[12],
        m[1]*x + m[5]*y + m[9]*z + m[13],
        m[2]*x + m[6]*y + m[10]*z + m[14],
    )


def scene_nodes(gltf):
    nodes = gltf.get("nodes", [])
    scenes = gltf.get("scenes")
    if not scenes:
        return [(i, node_matrix(n)) for i, n in enumerate(nodes) if "mesh" in n]

    scene_index = gltf.get("scene", 0)
    if not isinstance(scene_index, int) or not 0 <= scene_index < len(scenes):
        scene_index = 0
    result = []

    def visit(index, parent):
        if not 0 <= index < len(nodes):
            return
        world = matrix_multiply(parent, node_matrix(nodes[index]))
        result.append((index, world))
        for child in nodes[index].get("children", []):
            visit(child, world)

    for root in scenes[scene_index].get("nodes", []):
        visit(root, identity_matrix())
    return result


def material_factor(gltf, index):
    materials = gltf.get("materials", [])
    if not isinstance(index, int) or not 0 <= index < len(materials):
        return (1, 1, 1, 1)
    return tuple(materials[index].get("pbrMetallicRoughness", {}).get(
        "baseColorFactor", [1, 1, 1, 1]))


def convert(source, destination):
    gltf, blob = load_glb(source)
    vertices, indices = [], []

    for node_index, world in scene_nodes(gltf):
        node = gltf["nodes"][node_index]
        mesh_index = node.get("mesh")
        if not isinstance(mesh_index, int) or not 0 <= mesh_index < len(gltf["meshes"]):
            continue
        for primitive in gltf["meshes"][mesh_index].get("primitives", []):
            if primitive.get("mode", 4) != 4:
                raise ValueError("Only triangle primitives are supported")
            attrs = primitive.get("attributes", {})
            if "POSITION" not in attrs:
                continue
            positions = accessor_values(gltf, blob, attrs["POSITION"])
            colours = accessor_values(gltf, blob, attrs["COLOR_0"]) if "COLOR_0" in attrs else None
            uvs = accessor_values(gltf, blob, attrs["TEXCOORD_0"]) if "TEXCOORD_0" in attrs else None
            factor = material_factor(gltf, primitive.get("material"))
            base_vertex = len(vertices)

            for i, position in enumerate(positions):
                x, y, z = transform_point(world, position)
                colour = colours[i] if colours else (1, 1, 1, 1)
                if len(colour) == 3:
                    colour = (*colour, 1.0)
                rgba = tuple(max(0, min(255, round(colour[k] * factor[k] * 255))) for k in range(4))
                uv = uvs[i] if uvs else (0.0, 0.0)
                vertices.append((x, y, z, 1.0, *rgba, float(uv[0]), float(uv[1])))

            if "indices" in primitive:
                src_indices = accessor_values(gltf, blob, primitive["indices"])
                indices.extend(base_vertex + int(v[0]) for v in src_indices)
            else:
                indices.extend(range(base_vertex, base_vertex + len(positions)))

    if not vertices:
        raise ValueError("No mesh vertices found")
    if len(indices) < 3 or len(indices) % 3:
        raise ValueError("Converted mesh does not contain complete triangles")

    xs, ys, zs = [v[0] for v in vertices], [v[1] for v in vertices], [v[2] for v in vertices]
    bounds = (min(xs), min(ys), min(zs), max(xs), max(ys), max(zs))
    output = bytearray(struct.pack("<4sIIII6f", b"WBM1", 1, len(vertices), len(indices), 0x3, *bounds))
    for v in vertices:
        output += struct.pack("<4f4B2f", v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9])
    output += struct.pack("<%dI" % len(indices), *indices)

    destination = Path(destination)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(output)
    print(f"{source} -> {destination}: {len(vertices)} vertices, {len(indices)//3} triangles, {len(output)/1024:.1f} KiB")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert GLB 2.0 to We Beast WBM1")
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()
    convert(args.input, args.output)
