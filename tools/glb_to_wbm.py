#!/usr/bin/env python3
"""Convert GLB 2.0 meshes to the small We Beast WBM1 runtime format."""

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


def _scan_json_value(text, start):
    """Return the end offset of one JSON-ish value without validating it."""
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
        in_string = False
        escaped = False
        pairs = {"]": "[", "}": "{"}
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
            elif ch in "]}":
                if stack and stack[-1] == pairs[ch]:
                    stack.pop()
                    if not stack:
                        return i + 1
        raise ValueError("Unterminated JSON container")

    i = start
    while i < len(text) and text[i] not in ",}":
        i += 1
    return i


def _top_level_members_loose(text):
    """Split a glTF root object without validating its member values."""
    members = {}
    i = 0
    while i < len(text) and text[i].isspace():
        i += 1
    if i >= len(text) or text[i] != "{":
        raise ValueError("glTF JSON chunk is not an object")
    i += 1

    decoder = json.JSONDecoder()
    while i < len(text):
        while i < len(text) and (text[i].isspace() or text[i] == ","):
            i += 1
        if i >= len(text) or text[i] == "}":
            break
        if text[i] != '"':
            raise ValueError(f"Expected top-level glTF key near offset {i}")

        key, consumed = decoder.raw_decode(text[i:])
        i += consumed
        while i < len(text) and text[i].isspace():
            i += 1
        if i >= len(text) or text[i] != ":":
            raise ValueError(f"Missing colon after glTF key {key!r}")
        i += 1
        while i < len(text) and text[i].isspace():
            i += 1

        value_start = i
        value_end = _scan_json_value(text, value_start)
        members[key] = text[value_start:value_end]
        i = value_end

    return members


def _load_member(raw, key, required=False):
    value = raw.get(key)
    if value is None:
        if required:
            raise ValueError(f"GLB is missing required glTF field {key!r}")
        return None
    try:
        return json.loads(value)
    except json.JSONDecodeError as exc:
        if required:
            context = value[max(0, exc.pos - 100):exc.pos + 100]
            raise ValueError(
                f"glTF field {key!r} is malformed near offset {exc.pos}: {context!r}"
            ) from exc
        return None


def _recover_meshes(raw_meshes):
    """Recover standard primitive arrays while ignoring private mesh metadata.

    Nomad Sculpt 1.10 files seen in this project can contain malformed JSON in
    mesh-level `extras.nomad`, before a perfectly valid `primitives` member.
    WBM only needs those primitives, so find and parse them independently.
    """
    meshes = []
    needle = '"primitives"'
    search_from = 0

    while True:
        key = raw_meshes.find(needle, search_from)
        if key < 0:
            break
        colon = key + len(needle)
        while colon < len(raw_meshes) and raw_meshes[colon].isspace():
            colon += 1
        if colon >= len(raw_meshes) or raw_meshes[colon] != ":":
            search_from = key + len(needle)
            continue

        start = colon + 1
        while start < len(raw_meshes) and raw_meshes[start].isspace():
            start += 1
        if start >= len(raw_meshes) or raw_meshes[start] != "[":
            search_from = key + len(needle)
            continue

        try:
            end = _scan_json_value(raw_meshes, start)
            primitives = json.loads(raw_meshes[start:end])
        except (ValueError, json.JSONDecodeError):
            search_from = key + len(needle)
            continue

        if isinstance(primitives, list) and primitives:
            meshes.append({"primitives": primitives})
        search_from = max(end, key + len(needle))

    if not meshes:
        raise ValueError("Could not recover any valid mesh primitives from Nomad GLB")
    return meshes


def _minimal_gltf_from_loose_json(text):
    raw = _top_level_members_loose(text)
    result = {
        "accessors": _load_member(raw, "accessors", required=True),
        "bufferViews": _load_member(raw, "bufferViews", required=True),
    }

    meshes = _load_member(raw, "meshes")
    if not isinstance(meshes, list) or not meshes:
        meshes = _recover_meshes(raw.get("meshes", ""))
    result["meshes"] = meshes

    nodes = _load_member(raw, "nodes")
    if not isinstance(nodes, list) or not nodes:
        # The renderer normalises prop scale from WBM bounds, so identity nodes
        # are a safe recovery when only Nomad's node metadata is malformed.
        nodes = [{"mesh": index} for index in range(len(meshes))]
    result["nodes"] = nodes

    for key in ("scene", "scenes", "buffers", "asset"):
        value = _load_member(raw, key)
        if value is not None:
            result[key] = value

    materials = _load_member(raw, "materials")
    result["materials"] = materials if isinstance(materials, list) else []
    return result


def _decode_gltf_json(chunk):
    text = chunk.rstrip(b"\0 \t\r\n").decode("utf-8")
    try:
        return json.loads(text)
    except json.JSONDecodeError:
        return _minimal_gltf_from_loose_json(text)


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
            gltf = _decode_gltf_json(chunk)
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
    scenes = gltf.get("scenes") or [fallback]
    if not isinstance(scene_index, int) or scene_index < 0 or scene_index >= len(scenes):
        scene_index = 0
    roots = scenes[scene_index].get("nodes", [])
    nodes = gltf.get("nodes", [])
    result = []

    def visit(index, parent):
        if index < 0 or index >= len(nodes):
            return
        world = matrix_multiply(parent, node_matrix(nodes[index]))
        result.append((index, world))
        for child in nodes[index].get("children", []):
            visit(child, world)

    for root in roots:
        visit(root, identity_matrix())

    if not result:
        for index, node in enumerate(nodes):
            if "mesh" in node:
                result.append((index, node_matrix(node)))
    return result


def material_factor(gltf, material_index):
    if material_index is None:
        return (1, 1, 1, 1)
    materials = gltf.get("materials", [])
    if not isinstance(material_index, int) or material_index < 0 or material_index >= len(materials):
        return (1, 1, 1, 1)
    material = materials[material_index]
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
        mesh_index = node["mesh"]
        if mesh_index < 0 or mesh_index >= len(gltf["meshes"]):
            continue

        mesh = gltf["meshes"][mesh_index]
        for primitive in mesh.get("primitives", []):
            if primitive.get("mode", 4) != 4:
                raise ValueError("Only triangle primitives are supported")

            attributes = primitive.get("attributes", {})
            if "POSITION" not in attributes:
                continue
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
    if len(indices) < 3 or len(indices) % 3 != 0:
        raise ValueError("Converted mesh does not contain complete triangles")

    xs = [v[0] for v in vertices]
    ys = [v[1] for v in vertices]
    zs = [v[2] for v in vertices]
    bounds = (min(xs), min(ys), min(zs), max(xs), max(ys), max(zs))

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
        description="Convert a GLB 2.0 mesh to We Beast WBM1."
    )
    parser.add_argument("input")
    parser.add_argument("output")
    args = parser.parse_args()
    convert(args.input, args.output)
