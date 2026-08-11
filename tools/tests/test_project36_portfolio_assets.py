"""Verifies the original Project 36 portfolio animation clips.

The GLB reader here is intentionally independent of the generator so that a
parsing bug in the generator cannot hide behind a shared implementation.
"""

from pathlib import Path
from typing import NoReturn
import json
import math
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "tools" / "generate_project36_portfolio_animations.py"
SOURCE = ROOT / "Dx11" / "Resource" / "fbx" / "Public" / "MyAlice" / "Player" / "SampleModel.glb"
ANIMATION_DIR = ROOT / "Dx11" / "Resource" / "fbx" / "Public" / "MyAlice" / "Animations"

EXPECTED = {
    "anim_PortfolioDance.glb": ("PortfolioDance", 4.0),
    "anim_PortfolioUpperWave.glb": ("PortfolioUpperWave", 2.0),
}
BANNED = (b"NIKKE", b"Alice_.fbx", b"CaramellaDansen", b"RabbitHole", b"Specialist", b"CaliforniaGirls")

EXPECTED_TIMES = {
    "PortfolioDance": [0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0],
    "PortfolioUpperWave": [0.0, 0.5, 1.0, 1.5, 2.0],
}
EXPECTED_CHANNELS = {
    "PortfolioDance": [
        ("J_Bip_C_Hips", "rotation"),
        ("J_Bip_C_Spine", "rotation"),
        ("J_Bip_C_Chest", "rotation"),
        ("J_Bip_L_UpperArm", "rotation"),
        ("J_Bip_R_UpperArm", "rotation"),
        ("J_Bip_L_LowerArm", "rotation"),
        ("J_Bip_R_LowerArm", "rotation"),
        ("J_Bip_L_UpperLeg", "rotation"),
        ("J_Bip_R_UpperLeg", "rotation"),
        ("J_Bip_C_Hips", "translation"),
    ],
    "PortfolioUpperWave": [
        ("J_Bip_C_Chest", "rotation"),
        ("J_Bip_L_Shoulder", "rotation"),
        ("J_Bip_R_Shoulder", "rotation"),
        ("J_Bip_L_UpperArm", "rotation"),
        ("J_Bip_R_UpperArm", "rotation"),
        ("J_Bip_L_LowerArm", "rotation"),
        ("J_Bip_R_LowerArm", "rotation"),
    ],
}
EXPECTED_HIPS_OFFSET_X = [0.0, 0.08, 0.0, -0.08, 0.0, 0.08, 0.0, -0.08, 0.0]

# Every authored row below is a pure Z rotation, so the delta recovered from a
# keyframe as conjugate(bind) * frame must be exactly sin/cos of the half angle
# about Z. That inverts the generator's composition instead of repeating it, and
# it pins the local-frame delta convention, the sign, and degrees -> radians.
EXPECTED_LOCAL_Z_DELTA_DEG = {
    "PortfolioDance": {
        "J_Bip_L_LowerArm": [0, -25, -55, -25, 0, -25, -55, -25, 0],
        "J_Bip_R_LowerArm": [0, 55, 25, 55, 0, 55, 25, 55, 0],
        "J_Bip_L_UpperLeg": [0, 8, -5, 2, 0, 8, -5, 2, 0],
        "J_Bip_R_UpperLeg": [0, -2, 5, -8, 0, -2, 5, -8, 0],
    },
    "PortfolioUpperWave": {
        "J_Bip_L_Shoulder": [0, -12, -20, -12, 0],
        "J_Bip_R_Shoulder": [0, 12, 20, 12, 0],
        "J_Bip_L_LowerArm": [0, -35, -65, -35, 0],
        "J_Bip_R_LowerArm": [0, 65, 35, 65, 0],
    },
}
DELTA_TOLERANCE = 2e-6

GLB_MAGIC = 0x46546C67
CHUNK_JSON = 0x4E4F534A
CHUNK_BIN = 0x004E4942
COMPONENT_FLOAT = 5126
COMPONENT_COUNTS = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}
DEFAULT_ROTATION = [0.0, 0.0, 0.0, 1.0]
DEFAULT_TRANSLATION = [0.0, 0.0, 0.0]


def fail(message: str) -> NoReturn:
    print(f"[FAIL] {message}")
    raise SystemExit(1)


def check(condition: object, message: str) -> None:
    if not condition:
        fail(message)


def as_float32(value: float) -> float:
    return struct.unpack("<f", struct.pack("<f", value))[0]


def quaternion_multiply(left, right):
    """Hamilton product of two (x, y, z, w) quaternions."""
    lx, ly, lz, lw = left
    rx, ry, rz, rw = right
    return [
        lw * rx + lx * rw + ly * rz - lz * ry,
        lw * ry - lx * rz + ly * rw + lz * rx,
        lw * rz + lx * ry - ly * rx + lz * rw,
        lw * rw - lx * rx - ly * ry - lz * rz,
    ]


def local_rotation_delta(bind, frame):
    """The delta that turns the bind pose into this keyframe, in the bone's own frame."""
    return quaternion_multiply([-bind[0], -bind[1], -bind[2], bind[3]], frame)


def read_glb(path: Path):
    if not path.is_file():
        fail(f"missing GLB: {path}")
    data = path.read_bytes()
    check(len(data) >= 12, f"{path.name}: truncated header")
    magic, version, total = struct.unpack_from("<III", data, 0)
    check(magic == GLB_MAGIC, f"{path.name}: bad magic {magic:#x}")
    check(version == 2, f"{path.name}: bad container version {version}")
    check(total == len(data), f"{path.name}: header length {total} != file size {len(data)}")

    doc = None
    binary = b""
    offset = 12
    while offset < total:
        length, kind = struct.unpack_from("<II", data, offset)
        check(length % 4 == 0, f"{path.name}: chunk at {offset} is not 4-byte aligned")
        payload = data[offset + 8:offset + 8 + length]
        check(len(payload) == length, f"{path.name}: chunk at {offset} is truncated")
        if kind == CHUNK_JSON:
            doc = json.loads(payload.decode("utf-8"))
        elif kind == CHUNK_BIN:
            binary = payload
        else:
            fail(f"{path.name}: unexpected chunk type {kind:#x}")
        offset += 8 + length
    check(doc is not None, f"{path.name}: no JSON chunk")
    return doc, binary


def read_accessor(doc: dict, binary: bytes, index: int, path_name: str):
    accessor = doc["accessors"][index]
    check(accessor["componentType"] == COMPONENT_FLOAT, f"{path_name}: accessor {index} is not float32")
    check(not accessor.get("normalized"), f"{path_name}: accessor {index} must not be normalized")
    kind = accessor["type"]
    check(kind in COMPONENT_COUNTS, f"{path_name}: accessor {index} has unsupported type {kind}")
    components = COMPONENT_COUNTS[kind]
    view = doc["bufferViews"][accessor["bufferView"]]
    check(view.get("byteStride") in (None, components * 4), f"{path_name}: accessor {index} has an interleaved view")
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    span = accessor["count"] * components * 4
    check(start % 4 == 0, f"{path_name}: accessor {index} starts unaligned at {start}")
    check(start + span <= len(binary), f"{path_name}: accessor {index} runs past the binary chunk")
    flat = struct.unpack_from("<" + "f" * (accessor["count"] * components), binary, start)
    return [list(flat[i * components:(i + 1) * components]) for i in range(accessor["count"])]


def skeleton_closure(doc: dict) -> set:
    """Joint nodes plus every ancestor needed to reach the scene root."""
    parents = {}
    for index, node in enumerate(doc["nodes"]):
        for child in node.get("children", []):
            parents[child] = index
    closure = set(doc["skins"][0]["joints"])
    for joint in doc["skins"][0]["joints"]:
        cursor = joint
        while cursor in parents:
            cursor = parents[cursor]
            closure.add(cursor)
    return closure


def verify_asset(path: Path, animation_name: str, duration: float, closure_names: set, bind_nodes: dict) -> None:
    doc, binary = read_glb(path)
    name = path.name

    check(doc.get("asset", {}).get("version") == "2.0", f"{name}: asset.version must be '2.0'")
    check(len(doc.get("animations", [])) == 1, f"{name}: expected exactly one animation")
    animation = doc["animations"][0]
    check(animation["name"] == animation_name, f"{name}: animation name {animation['name']!r} != {animation_name!r}")

    for forbidden in ("meshes", "skins", "materials", "textures", "images", "audio", "samplers", "cameras"):
        check(not doc.get(forbidden), f"{name}: must not carry a {forbidden} payload")
    for node in doc["nodes"]:
        for forbidden in ("mesh", "skin", "camera", "extensions", "matrix"):
            check(forbidden not in node, f"{name}: node {node.get('name')!r} must not carry {forbidden!r}")
    for buffer in doc.get("buffers", []):
        check("uri" not in buffer, f"{name}: buffer data must stay embedded in the GLB")

    node_names = {node["name"] for node in doc["nodes"]}
    check(node_names == closure_names, f"{name}: node names differ from the source skeleton closure")
    check(len(doc["nodes"]) == len(closure_names), f"{name}: node list has duplicate names")
    check(
        all(child < len(doc["nodes"]) for node in doc["nodes"] for child in node.get("children", [])),
        f"{name}: a child index points outside the node list",
    )
    check(len(doc.get("scenes", [])) == 1, f"{name}: expected exactly one scene")
    check(
        all(root < len(doc["nodes"]) for root in doc["scenes"][0]["nodes"]),
        f"{name}: a scene root index points outside the node list",
    )

    haystack = (json.dumps(doc).encode("utf-8") + binary).lower()
    for token in BANNED:
        check(token.lower() not in haystack, f"{name}: banned provenance token {token!r} found")

    node_by_name = {node["name"]: node for node in doc["nodes"]}
    for node_name, node in node_by_name.items():
        source = bind_nodes[node_name]
        for key in ("translation", "rotation", "scale"):
            check(
                [as_float32(value) for value in node.get(key, [])] == [as_float32(value) for value in source.get(key, [])],
                f"{name}: node {node_name!r} lost its bind-local {key}",
            )

    expected_times = EXPECTED_TIMES[animation_name]
    seen_channels = []
    for channel in animation["channels"]:
        target = channel["target"]
        target_node = doc["nodes"][target["node"]]
        seen_channels.append((target_node["name"], target["path"]))
        sampler = animation["samplers"][channel["sampler"]]
        check(sampler.get("interpolation") == "LINEAR", f"{name}: sampler interpolation must be LINEAR")

        times = [row[0] for row in read_accessor(doc, binary, sampler["input"], name)]
        check(times[0] == 0.0, f"{name}: first keyframe time {times[0]} != 0")
        check(times[-1] == duration, f"{name}: last keyframe time {times[-1]} != declared duration {duration}")
        check(
            all(later > earlier for earlier, later in zip(times, times[1:])),
            f"{name}: keyframe times are not strictly increasing",
        )
        check(times == expected_times, f"{name}: keyframe times {times} != {expected_times}")
        time_accessor = doc["accessors"][sampler["input"]]
        check(time_accessor.get("min") == [0.0], f"{name}: time accessor min must be [0.0]")
        check(time_accessor.get("max") == [duration], f"{name}: time accessor max must be [{duration}]")

        values = read_accessor(doc, binary, sampler["output"], name)
        check(len(values) == len(times), f"{name}: sampler output count {len(values)} != input count {len(times)}")

        if target["path"] == "rotation":
            bind = [as_float32(value) for value in target_node.get("rotation", DEFAULT_ROTATION)]
            check(values[0] == bind, f"{name}: {target_node['name']} rotation does not start at the bind pose")
            check(values[-1] == bind, f"{name}: {target_node['name']} rotation does not end at the bind pose")
            check(
                any(frame != bind for frame in values),
                f"{name}: {target_node['name']} rotation never leaves the bind pose",
            )
            for frame in values:
                norm = math.sqrt(sum(component * component for component in frame))
                check(abs(norm - 1.0) < 1e-5, f"{name}: {target_node['name']} rotation {frame} is not a unit quaternion")
            expected_z_deg = EXPECTED_LOCAL_Z_DELTA_DEG[animation_name].get(target_node["name"])
            if expected_z_deg is not None:
                check(
                    len(expected_z_deg) == len(values),
                    f"{name}: {target_node['name']} has {len(values)} keys, expected {len(expected_z_deg)}",
                )
                for degrees, frame in zip(expected_z_deg, values):
                    half = math.radians(degrees) / 2.0
                    expected_delta = [0.0, 0.0, math.sin(half), math.cos(half)]
                    actual_delta = local_rotation_delta(bind, frame)
                    check(
                        all(abs(a - b) < DELTA_TOLERANCE for a, b in zip(actual_delta, expected_delta)),
                        f"{name}: {target_node['name']} local delta {actual_delta} != {degrees} deg about Z {expected_delta}",
                    )
        elif target["path"] == "translation":
            bind = [as_float32(value) for value in target_node.get("translation", DEFAULT_TRANSLATION)]
            check(
                len(EXPECTED_HIPS_OFFSET_X) == len(values),
                f"{name}: hips translation has {len(values)} keys, expected {len(EXPECTED_HIPS_OFFSET_X)}",
            )
            for offset, frame in zip(EXPECTED_HIPS_OFFSET_X, values):
                check(
                    frame == [as_float32(bind[0] + offset), bind[1], bind[2]],
                    f"{name}: hips translation {frame} != bind {bind} plus x offset {offset}",
                )
        else:
            fail(f"{name}: unexpected animated path {target['path']!r}")

    check(
        seen_channels == EXPECTED_CHANNELS[animation_name],
        f"{name}: channels {seen_channels} != {EXPECTED_CHANNELS[animation_name]}",
    )
    print(f"[OK] {name}: {animation_name} {duration:.1f}s, {len(seen_channels)} channels, {len(doc['nodes'])} nodes, {path.stat().st_size} bytes")


def verify_reproducible() -> None:
    with tempfile.TemporaryDirectory() as temp:
        result = subprocess.run(
            [sys.executable, str(GENERATOR), "--source", str(SOURCE), "--output-dir", temp],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            print(result.stdout, end="")
            print(result.stderr, end="")
            fail(f"generator exited {result.returncode}")
        for filename in EXPECTED:
            regenerated = Path(temp) / filename
            if not regenerated.is_file():
                fail(f"generator did not write {filename}")
            check(
                regenerated.read_bytes() == (ANIMATION_DIR / filename).read_bytes(),
                f"{filename}: regenerated bytes differ from the committed asset",
            )
    print("[OK] regeneration is byte-identical for both clips")


def main() -> int:
    if not GENERATOR.is_file():
        fail(f"missing generator: {GENERATOR}")
    if not SOURCE.is_file():
        fail(f"missing source model: {SOURCE}")

    source_doc, _ = read_glb(SOURCE)
    closure = skeleton_closure(source_doc)
    bind_nodes = {source_doc["nodes"][index]["name"]: source_doc["nodes"][index] for index in closure}
    closure_names = set(bind_nodes)
    check(len(bind_nodes) == len(closure), f"source skeleton closure has duplicate node names ({SOURCE})")

    for filename, (animation_name, duration) in EXPECTED.items():
        verify_asset(ANIMATION_DIR / filename, animation_name, duration, closure_names, bind_nodes)

    verify_reproducible()
    print("[OK] project36 portfolio animation assets verified")
    return 0


if __name__ == "__main__":
    sys.exit(main())
