"""Generates the original Project 36 portfolio animation clips as GLB files.

The only thing taken from the source character is its skeleton: node names, the
parent/child hierarchy, and bind-local translation/rotation/scale. All motion
comes from the authored keyframe tables below. Each output holds exactly one
animation and no mesh, skin, material, image, texture, or audio payload.

Euler triples are (x, y, z) in degrees and are read as intrinsic X -> Y -> Z
deltas applied on top of the bind pose, so a (0, 0, 0) triple reproduces the
bind rotation bit-exactly.

Output is byte-identical for identical input: every float is emitted through
struct.pack, JSON key order is fixed, and nothing time-, path-, or
version-dependent is written into the file.

Usage:
    python tools/generate_project36_portfolio_animations.py \
        --source Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb \
        --output-dir Dx11/Resource/fbx/Public/MyAlice/Animations
"""

from pathlib import Path
from typing import Dict, List, NamedTuple, NoReturn, Optional, Sequence, Tuple
import argparse
import json
import math
import struct
import sys

GLB_MAGIC = 0x46546C67
GLB_VERSION = 2
CHUNK_JSON = 0x4E4F534A
CHUNK_BIN = 0x004E4942
COMPONENT_FLOAT = 5126
COMPONENT_COUNTS = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}
IDENTITY_ROTATION = (0.0, 0.0, 0.0, 1.0)
ORIGIN_TRANSLATION = (0.0, 0.0, 0.0)

DANCE_TIMES = [0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0]
DANCE_EULER_DEG = {
    "J_Bip_C_Hips":       [(0, 0, 0), (0, 10, 7), (0, 0, 0), (0, -10, -7), (0, 0, 0), (0, 10, 7), (0, 0, 0), (0, -10, -7), (0, 0, 0)],
    "J_Bip_C_Spine":      [(0, 0, 0), (4, -8, -5), (0, 0, 0), (4, 8, 5), (0, 0, 0), (4, -8, -5), (0, 0, 0), (4, 8, 5), (0, 0, 0)],
    "J_Bip_C_Chest":      [(0, 0, 0), (-3, -10, -8), (0, 0, 0), (-3, 10, 8), (0, 0, 0), (-3, -10, -8), (0, 0, 0), (-3, 10, 8), (0, 0, 0)],
    "J_Bip_L_UpperArm":   [(0, 0, 0), (-20, 0, -35), (-110, 0, -70), (-20, 0, -35), (0, 0, 0), (-20, 0, -35), (-110, 0, -70), (-20, 0, -35), (0, 0, 0)],
    "J_Bip_R_UpperArm":   [(0, 0, 0), (-110, 0, 70), (-20, 0, 35), (-110, 0, 70), (0, 0, 0), (-110, 0, 70), (-20, 0, 35), (-110, 0, 70), (0, 0, 0)],
    "J_Bip_L_LowerArm":   [(0, 0, 0), (0, 0, -25), (0, 0, -55), (0, 0, -25), (0, 0, 0), (0, 0, -25), (0, 0, -55), (0, 0, -25), (0, 0, 0)],
    "J_Bip_R_LowerArm":   [(0, 0, 0), (0, 0, 55), (0, 0, 25), (0, 0, 55), (0, 0, 0), (0, 0, 55), (0, 0, 25), (0, 0, 55), (0, 0, 0)],
    "J_Bip_L_UpperLeg":   [(0, 0, 0), (0, 0, 8), (0, 0, -5), (0, 0, 2), (0, 0, 0), (0, 0, 8), (0, 0, -5), (0, 0, 2), (0, 0, 0)],
    "J_Bip_R_UpperLeg":   [(0, 0, 0), (0, 0, -2), (0, 0, 5), (0, 0, -8), (0, 0, 0), (0, 0, -2), (0, 0, 5), (0, 0, -8), (0, 0, 0)],
}
DANCE_HIPS_X = [0.0, 0.08, 0.0, -0.08, 0.0, 0.08, 0.0, -0.08, 0.0]

UPPER_TIMES = [0.0, 0.5, 1.0, 1.5, 2.0]
UPPER_EULER_DEG = {
    "J_Bip_C_Chest":     [(0, 0, 0), (-5, -12, -5), (0, 0, 0), (-5, 12, 5), (0, 0, 0)],
    "J_Bip_L_Shoulder":  [(0, 0, 0), (0, 0, -12), (0, 0, -20), (0, 0, -12), (0, 0, 0)],
    "J_Bip_R_Shoulder":  [(0, 0, 0), (0, 0, 12), (0, 0, 20), (0, 0, 12), (0, 0, 0)],
    "J_Bip_L_UpperArm":  [(0, 0, 0), (-45, 0, -55), (-75, 0, -90), (-45, 0, -55), (0, 0, 0)],
    "J_Bip_R_UpperArm":  [(0, 0, 0), (-75, 0, 90), (-45, 0, 55), (-75, 0, 90), (0, 0, 0)],
    "J_Bip_L_LowerArm":  [(0, 0, 0), (0, 0, -35), (0, 0, -65), (0, 0, -35), (0, 0, 0)],
    "J_Bip_R_LowerArm":  [(0, 0, 0), (0, 0, 65), (0, 0, 35), (0, 0, 65), (0, 0, 0)],
}


class ClipSpec(NamedTuple):
    filename: str
    animation_name: str
    times: List[float]
    rotation_euler_deg: Dict[str, List[Tuple[float, float, float]]]
    translation_x_node: str = ""
    translation_x_offsets: Sequence[float] = ()


CLIPS = (
    ClipSpec(
        filename="anim_PortfolioDance.glb",
        animation_name="PortfolioDance",
        times=DANCE_TIMES,
        rotation_euler_deg=DANCE_EULER_DEG,
        translation_x_node="J_Bip_C_Hips",
        translation_x_offsets=DANCE_HIPS_X,
    ),
    ClipSpec(
        filename="anim_PortfolioUpperWave.glb",
        animation_name="PortfolioUpperWave",
        times=UPPER_TIMES,
        rotation_euler_deg=UPPER_EULER_DEG,
    ),
)


def fail(message: str) -> NoReturn:
    print(f"[FAIL] {message}", file=sys.stderr)
    raise SystemExit(1)


def as_float32(value: float) -> float:
    """Rounds to the value the GLB will actually store."""
    return struct.unpack("<f", struct.pack("<f", value))[0]


def quaternion_multiply(left: Sequence[float], right: Sequence[float]) -> Tuple[float, float, float, float]:
    """Hamilton product of two (x, y, z, w) quaternions."""
    lx, ly, lz, lw = left
    rx, ry, rz, rw = right
    return (
        lw * rx + lx * rw + ly * rz - lz * ry,
        lw * ry - lx * rz + ly * rw + lz * rx,
        lw * rz + lx * ry - ly * rx + lz * rw,
        lw * rw - lx * rx - ly * ry - lz * rz,
    )


def quaternion_from_euler_degrees(x_deg: float, y_deg: float, z_deg: float) -> Tuple[float, float, float, float]:
    """Intrinsic X -> Y -> Z rotation as an (x, y, z, w) quaternion.

    math.sin(0.0) is exactly 0.0 and math.cos(0.0) is exactly 1.0, so a
    (0, 0, 0) triple yields the exact identity quaternion.
    """
    half_x, half_y, half_z = (math.radians(x_deg) / 2.0, math.radians(y_deg) / 2.0, math.radians(z_deg) / 2.0)
    around_x = (math.sin(half_x), 0.0, 0.0, math.cos(half_x))
    around_y = (0.0, math.sin(half_y), 0.0, math.cos(half_y))
    around_z = (0.0, 0.0, math.sin(half_z), math.cos(half_z))
    return quaternion_multiply(quaternion_multiply(around_x, around_y), around_z)


def read_glb_document(path: Path) -> dict:
    """Returns the JSON chunk of a GLB. The binary chunk is not needed here."""
    data = path.read_bytes()
    if len(data) < 12:
        fail(f"{path} is too small to be a GLB")
    magic, version, total = struct.unpack_from("<III", data, 0)
    if magic != GLB_MAGIC:
        fail(f"{path} is not a GLB (magic {magic:#x})")
    if version != GLB_VERSION:
        fail(f"{path} uses GLB container version {version}, expected {GLB_VERSION}")
    if total != len(data):
        fail(f"{path} declares {total} bytes but is {len(data)} bytes")

    offset = 12
    while offset < total:
        length, kind = struct.unpack_from("<II", data, offset)
        payload = data[offset + 8:offset + 8 + length]
        if len(payload) != length:
            fail(f"{path} has a truncated chunk at byte {offset}")
        if kind == CHUNK_JSON:
            return json.loads(payload.decode("utf-8"))
        offset += 8 + length
    fail(f"{path} has no JSON chunk")


def skeleton_closure(source_doc: dict, source_path: Path) -> List[int]:
    """Skin joints plus every ancestor up to the scene root, in DFS pre-order."""
    nodes = source_doc.get("nodes") or []
    skins = source_doc.get("skins") or []
    if not skins:
        fail(f"{source_path} has no skin to take a skeleton from")
    joints = skins[0].get("joints") or []
    if not joints:
        fail(f"{source_path} skin 0 has no joints")

    parents: Dict[int, int] = {}
    for index, node in enumerate(nodes):
        for child in node.get("children", []):
            parents[child] = index

    members = set(joints)
    for joint in joints:
        cursor = joint
        while cursor in parents:
            cursor = parents[cursor]
            members.add(cursor)

    order: List[int] = []
    visited = set()
    stack = sorted((index for index in members if index not in parents), reverse=True)
    while stack:
        index = stack.pop()
        if index in visited:
            continue
        visited.add(index)
        order.append(index)
        for child in reversed(nodes[index].get("children", [])):
            if child in members:
                stack.append(child)
    if len(order) != len(members):
        fail(f"{source_path} skeleton closure is not a connected tree ({len(order)} of {len(members)} reachable)")
    return order


def build_skeleton_nodes(source_doc: dict, order: List[int], source_path: Path) -> List[dict]:
    """Copies the closure's names, hierarchy, and bind-local TRS. Nothing else."""
    remap = {source_index: new_index for new_index, source_index in enumerate(order)}
    nodes: List[dict] = []
    for source_index in order:
        source_node = source_doc["nodes"][source_index]
        name = source_node.get("name")
        if not name:
            fail(f"{source_path} skeleton node {source_index} has no name")
        node: dict = {"name": name}
        children = [remap[child] for child in source_node.get("children", []) if child in remap]
        if children:
            node["children"] = children
        for key in ("translation", "rotation", "scale"):
            values = source_node.get(key)
            if values is not None:
                node[key] = [as_float32(value) for value in values]
        nodes.append(node)

    names = {node["name"] for node in nodes}
    if len(names) != len(nodes):
        fail(f"{source_path} skeleton closure has duplicate node names")
    return nodes


class BinaryWriter:
    """Accumulates float32 accessors into a single GLB buffer."""

    def __init__(self) -> None:
        self.data = bytearray()
        self.buffer_views: List[dict] = []
        self.accessors: List[dict] = []

    def add(self, rows: Sequence[Sequence[float]], kind: str, bounds: bool = False) -> int:
        components = COMPONENT_COUNTS[kind]
        byte_offset = len(self.data)
        packer = struct.Struct("<" + "f" * components)
        for row in rows:
            self.data.extend(packer.pack(*row))
        self.buffer_views.append(
            {"buffer": 0, "byteOffset": byte_offset, "byteLength": len(self.data) - byte_offset}
        )
        accessor = {
            "bufferView": len(self.buffer_views) - 1,
            "componentType": COMPONENT_FLOAT,
            "count": len(rows),
            "type": kind,
        }
        if bounds:
            columns = list(zip(*rows))
            accessor["min"] = [as_float32(min(column)) for column in columns]
            accessor["max"] = [as_float32(max(column)) for column in columns]
        self.accessors.append(accessor)
        return len(self.accessors) - 1


def build_clip(source_doc: dict, clip: ClipSpec, source_path: Path) -> Tuple[dict, bytes]:
    order = skeleton_closure(source_doc, source_path)
    nodes = build_skeleton_nodes(source_doc, order, source_path)
    node_index_by_name = {node["name"]: index for index, node in enumerate(nodes)}
    parented = {child for node in nodes for child in node.get("children", [])}
    roots = [index for index in range(len(nodes)) if index not in parented]

    writer = BinaryWriter()
    input_accessor = writer.add([(time,) for time in clip.times], "SCALAR", bounds=True)
    channels: List[dict] = []
    samplers: List[dict] = []

    def add_channel(node_name: str, path: str, rows: Sequence[Sequence[float]], kind: str) -> None:
        samplers.append(
            {"input": input_accessor, "interpolation": "LINEAR", "output": writer.add(rows, kind)}
        )
        channels.append(
            {"sampler": len(samplers) - 1, "target": {"node": node_index_by_name[node_name], "path": path}}
        )

    for node_name, euler_rows in clip.rotation_euler_deg.items():
        if node_name not in node_index_by_name:
            fail(f"{clip.animation_name}: node {node_name!r} is not in the skeleton of {source_path}")
        if len(euler_rows) != len(clip.times):
            fail(f"{clip.animation_name}: node {node_name!r} has {len(euler_rows)} keys, expected {len(clip.times)}")
        bind = nodes[node_index_by_name[node_name]].get("rotation", IDENTITY_ROTATION)
        frames = [quaternion_multiply(bind, quaternion_from_euler_degrees(*euler)) for euler in euler_rows]
        add_channel(node_name, "rotation", frames, "VEC4")

    if clip.translation_x_node or clip.translation_x_offsets:
        node_name = clip.translation_x_node
        if not node_name:
            fail(
                f"{clip.animation_name}: {len(clip.translation_x_offsets)} translation offsets "
                f"were given without a translation node"
            )
        if node_name not in node_index_by_name:
            fail(f"{clip.animation_name}: node {node_name!r} is not in the skeleton of {source_path}")
        if len(clip.translation_x_offsets) != len(clip.times):
            fail(
                f"{clip.animation_name}: node {node_name!r} has {len(clip.translation_x_offsets)} "
                f"translation keys, expected {len(clip.times)}"
            )
        bind = nodes[node_index_by_name[node_name]].get("translation", ORIGIN_TRANSLATION)
        frames = [(bind[0] + offset, bind[1], bind[2]) for offset in clip.translation_x_offsets]
        add_channel(node_name, "translation", frames, "VEC3")

    binary = bytes(writer.data) + b"\x00" * (-len(writer.data) % 4)
    document = {
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": roots}],
        "nodes": nodes,
        "animations": [{"name": clip.animation_name, "channels": channels, "samplers": samplers}],
        "accessors": writer.accessors,
        "bufferViews": writer.buffer_views,
        "buffers": [{"byteLength": len(binary)}],
    }
    return document, binary


def write_glb(path: Path, document: dict, binary: bytes) -> None:
    json_chunk = json.dumps(document, separators=(",", ":"), sort_keys=False).encode("utf-8")
    json_chunk += b" " * (-len(json_chunk) % 4)
    total = 12 + 8 + len(json_chunk) + 8 + len(binary)
    blob = bytearray()
    blob += struct.pack("<III", GLB_MAGIC, GLB_VERSION, total)
    blob += struct.pack("<II", len(json_chunk), CHUNK_JSON) + json_chunk
    blob += struct.pack("<II", len(binary), CHUNK_BIN) + binary
    path.write_bytes(bytes(blob))


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="Generate the original Project 36 portfolio animation clips.")
    parser.add_argument("--source", required=True, type=Path, help="Source GLB whose skeleton is reused.")
    parser.add_argument("--output-dir", required=True, type=Path, help="Directory that receives the clip GLBs.")
    args = parser.parse_args(argv)

    if not args.source.is_file():
        fail(f"missing source model: {args.source}")
    source_doc = read_glb_document(args.source)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    for clip in CLIPS:
        document, binary = build_clip(source_doc, clip, args.source)
        output_path = args.output_dir / clip.filename
        write_glb(output_path, document, binary)
        print(
            f"[OK] {clip.animation_name}: {len(document['nodes'])} nodes, "
            f"{len(document['animations'][0]['channels'])} channels, "
            f"{clip.times[-1]:.1f}s -> {output_path} ({output_path.stat().st_size} bytes)"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
