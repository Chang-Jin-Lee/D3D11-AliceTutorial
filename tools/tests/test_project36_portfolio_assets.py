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

# The authored local-frame delta for every rotation channel, in degrees,
# transcribed from the brief's euler tables. The expected quaternion for each row
# is derived below through this file's own matrix route
# (`expected_local_delta`), never by running the generator, and it is compared
# against the delta recovered from the file as conjugate(bind) * frame. That
# inverts the generator's composition instead of repeating it, and it pins the
# local-frame convention, the sign, degrees -> radians, and — because a
# multi-axis triple is order-sensitive — the intrinsic X -> Y -> Z euler order.
EXPECTED_LOCAL_DELTA_EULER_DEG = {
    "PortfolioDance": {
        "J_Bip_C_Hips":     [(0, 0, 0), (0, 10, 7), (0, 0, 0), (0, -10, -7), (0, 0, 0), (0, 10, 7), (0, 0, 0), (0, -10, -7), (0, 0, 0)],
        "J_Bip_C_Spine":    [(0, 0, 0), (4, -8, -5), (0, 0, 0), (4, 8, 5), (0, 0, 0), (4, -8, -5), (0, 0, 0), (4, 8, 5), (0, 0, 0)],
        "J_Bip_C_Chest":    [(0, 0, 0), (-3, -10, -8), (0, 0, 0), (-3, 10, 8), (0, 0, 0), (-3, -10, -8), (0, 0, 0), (-3, 10, 8), (0, 0, 0)],
        "J_Bip_L_UpperArm": [(0, 0, 0), (-20, 0, -35), (-110, 0, -70), (-20, 0, -35), (0, 0, 0), (-20, 0, -35), (-110, 0, -70), (-20, 0, -35), (0, 0, 0)],
        "J_Bip_R_UpperArm": [(0, 0, 0), (-110, 0, 70), (-20, 0, 35), (-110, 0, 70), (0, 0, 0), (-110, 0, 70), (-20, 0, 35), (-110, 0, 70), (0, 0, 0)],
        "J_Bip_L_LowerArm": [(0, 0, 0), (0, 0, -25), (0, 0, -55), (0, 0, -25), (0, 0, 0), (0, 0, -25), (0, 0, -55), (0, 0, -25), (0, 0, 0)],
        "J_Bip_R_LowerArm": [(0, 0, 0), (0, 0, 55), (0, 0, 25), (0, 0, 55), (0, 0, 0), (0, 0, 55), (0, 0, 25), (0, 0, 55), (0, 0, 0)],
        "J_Bip_L_UpperLeg": [(0, 0, 0), (0, 0, 8), (0, 0, -5), (0, 0, 2), (0, 0, 0), (0, 0, 8), (0, 0, -5), (0, 0, 2), (0, 0, 0)],
        "J_Bip_R_UpperLeg": [(0, 0, 0), (0, 0, -2), (0, 0, 5), (0, 0, -8), (0, 0, 0), (0, 0, -2), (0, 0, 5), (0, 0, -8), (0, 0, 0)],
    },
    "PortfolioUpperWave": {
        "J_Bip_C_Chest":    [(0, 0, 0), (-5, -12, -5), (0, 0, 0), (-5, 12, 5), (0, 0, 0)],
        "J_Bip_L_Shoulder": [(0, 0, 0), (0, 0, -12), (0, 0, -20), (0, 0, -12), (0, 0, 0)],
        "J_Bip_R_Shoulder": [(0, 0, 0), (0, 0, 12), (0, 0, 20), (0, 0, 12), (0, 0, 0)],
        "J_Bip_L_UpperArm": [(0, 0, 0), (-45, 0, -55), (-75, 0, -90), (-45, 0, -55), (0, 0, 0)],
        "J_Bip_R_UpperArm": [(0, 0, 0), (-75, 0, 90), (-45, 0, 55), (-75, 0, 90), (0, 0, 0)],
        "J_Bip_L_LowerArm": [(0, 0, 0), (0, 0, -35), (0, 0, -65), (0, 0, -35), (0, 0, 0)],
        "J_Bip_R_LowerArm": [(0, 0, 0), (0, 0, 65), (0, 0, 35), (0, 0, 65), (0, 0, 0)],
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
    print(f"[FAIL] {message}", file=sys.stderr)
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


def matrix_multiply(left, right):
    return tuple(
        tuple(sum(left[row][k] * right[k][column] for k in range(3)) for column in range(3))
        for row in range(3)
    )


def rotation_matrix_from_euler_degrees(x_deg: float, y_deg: float, z_deg: float):
    """Intrinsic X -> Y -> Z as a row-major 3x3 matrix: M = Rx . Ry . Rz.

    Deliberately built from the textbook single-axis rotation matrices rather
    than from a quaternion product, so this route shares no algebra with the
    generator and cannot inherit an error from it.
    """
    sin_x, cos_x = math.sin(math.radians(x_deg)), math.cos(math.radians(x_deg))
    sin_y, cos_y = math.sin(math.radians(y_deg)), math.cos(math.radians(y_deg))
    sin_z, cos_z = math.sin(math.radians(z_deg)), math.cos(math.radians(z_deg))
    around_x = ((1.0, 0.0, 0.0), (0.0, cos_x, -sin_x), (0.0, sin_x, cos_x))
    around_y = ((cos_y, 0.0, sin_y), (0.0, 1.0, 0.0), (-sin_y, 0.0, cos_y))
    around_z = ((cos_z, -sin_z, 0.0), (sin_z, cos_z, 0.0), (0.0, 0.0, 1.0))
    return matrix_multiply(matrix_multiply(around_x, around_y), around_z)


def quaternion_from_matrix(matrix):
    """(x, y, z, w) for a rotation matrix, taking the positive-w branch.

    Valid while trace > -1; every authored delta in this file turns by well under
    180 deg, and the guard below refuses rather than silently losing precision.
    """
    trace = matrix[0][0] + matrix[1][1] + matrix[2][2]
    check(trace > -0.5, f"euler delta trace {trace} is too near a 180 deg turn for the positive-w branch")
    scale = 2.0 * math.sqrt(trace + 1.0)
    return [
        (matrix[2][1] - matrix[1][2]) / scale,
        (matrix[0][2] - matrix[2][0]) / scale,
        (matrix[1][0] - matrix[0][1]) / scale,
        0.25 * scale,
    ]


def expected_local_delta(x_deg: float, y_deg: float, z_deg: float):
    """The delta quaternion an (x, y, z) degree triple must produce."""
    return quaternion_from_matrix(rotation_matrix_from_euler_degrees(x_deg, y_deg, z_deg))


def verify_delta_derivation() -> None:
    """Pins this file's euler -> quaternion route without consulting the generator.

    Two independent anchors:

    1. A hand-worked probe. For (90, 0, 90), intrinsic X -> Y -> Z is
       q = qx (x) qz with qx = (sin45, 0, 0, cos45) and qz = (0, 0, sin45, cos45).
       Their Hamilton product, expanded by hand, is
       x = cos45*0 + sin45*cos45 = 0.5, y = -sin45*sin45 = -0.5,
       z = cos45*sin45 = 0.5, w = cos45*cos45 = 0.5  ->  (0.5, -0.5, 0.5, 0.5).
       The reversed order Z -> Y -> X gives (0.5, +0.5, 0.5, 0.5), so this one row
       separates the two orders and the sign of y is what does it.
    2. Every pure-Z row of the expected table must equal the closed form
       (0, 0, sin(z/2), cos(z/2)), which pins the sign and degrees -> radians.
    """
    probe = expected_local_delta(90, 0, 90)
    hand_derived = [0.5, -0.5, 0.5, 0.5]
    check(
        all(abs(actual - expected) < 1e-12 for actual, expected in zip(probe, hand_derived)),
        f"euler -> quaternion probe (90, 0, 90) gave {probe}, hand derivation says {hand_derived}",
    )
    for rows in EXPECTED_LOCAL_DELTA_EULER_DEG.values():
        for euler_rows in rows.values():
            for x_deg, y_deg, z_deg in euler_rows:
                if x_deg or y_deg:
                    continue
                half = math.radians(z_deg) / 2.0
                closed_form = [0.0, 0.0, math.sin(half), math.cos(half)]
                derived = expected_local_delta(x_deg, y_deg, z_deg)
                check(
                    all(abs(actual - expected) < 1e-12 for actual, expected in zip(derived, closed_form)),
                    f"euler -> quaternion for (0, 0, {z_deg}) gave {derived}, closed form says {closed_form}",
                )
    print("[OK] expected deltas match the hand-derived probe and the closed-form pure-Z rows")


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


def verify_asset(
    path: Path,
    animation_name: str,
    duration: float,
    closure_names: set,
    bind_nodes: dict,
    source_children: dict,
) -> None:
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

    # Indices in range is not enough: a scrambled-but-in-range remap would still
    # pass, and regenerating with the same bug would still be byte-identical. Tie
    # every parent -> child edge back to the source skeleton by name.
    emitted_children = {
        node["name"]: sorted(doc["nodes"][child]["name"] for child in node.get("children", []))
        for node in doc["nodes"]
    }
    for node_name in sorted(emitted_children):
        check(
            emitted_children[node_name] == source_children[node_name],
            f"{name}: node {node_name!r} has children {emitted_children[node_name]} "
            f"but the source skeleton says {source_children[node_name]}",
        )
    parented = {child for children in emitted_children.values() for child in children}
    emitted_roots = sorted(doc["nodes"][root]["name"] for root in doc["scenes"][0]["nodes"])
    check(
        emitted_roots == sorted(node_names - parented),
        f"{name}: scene roots {emitted_roots} are not exactly the unparented nodes",
    )

    # Scan the file as written, not a re-serialisation: anything a JSON parse and
    # re-dump would normalise away must not be able to slip past this.
    haystack = path.read_bytes().lower()
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
            expected_euler = EXPECTED_LOCAL_DELTA_EULER_DEG[animation_name].get(target_node["name"])
            check(
                expected_euler is not None,
                f"{name}: rotation channel {target_node['name']!r} has no expected local delta table",
            )
            check(
                len(expected_euler) == len(values),
                f"{name}: {target_node['name']} has {len(values)} keys, expected {len(expected_euler)}",
            )
            for euler, frame in zip(expected_euler, values):
                expected_delta = expected_local_delta(*euler)
                actual_delta = local_rotation_delta(bind, frame)
                check(
                    all(abs(a - b) < DELTA_TOLERANCE for a, b in zip(actual_delta, expected_delta)),
                    f"{name}: {target_node['name']} local delta {actual_delta} != euler {euler} deg {expected_delta}",
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
    check(
        {node_name for node_name, path_kind in seen_channels if path_kind == "rotation"}
        == set(EXPECTED_LOCAL_DELTA_EULER_DEG[animation_name]),
        f"{name}: the rotation channels and the expected local delta table cover different nodes",
    )

    # Samplers are only ever reached through channels, so an orphan or a
    # double-referenced sampler would otherwise go unnoticed.
    samplers = animation["samplers"]
    check(
        len(samplers) == len(animation["channels"]),
        f"{name}: {len(samplers)} samplers for {len(animation['channels'])} channels",
    )
    check(
        sorted(channel["sampler"] for channel in animation["channels"]) == list(range(len(samplers))),
        f"{name}: samplers are not referenced exactly once each by the channel list",
    )
    check(len(doc.get("buffers", [])) == 1, f"{name}: expected exactly one buffer")
    check(
        doc["buffers"][0].get("byteLength") == len(binary),
        f"{name}: buffer byteLength {doc['buffers'][0].get('byteLength')} != BIN chunk length {len(binary)}",
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

    verify_delta_derivation()

    source_doc, _ = read_glb(SOURCE)
    closure = skeleton_closure(source_doc)
    bind_nodes = {source_doc["nodes"][index]["name"]: source_doc["nodes"][index] for index in closure}
    closure_names = set(bind_nodes)
    check(len(bind_nodes) == len(closure), f"source skeleton closure has duplicate node names ({SOURCE})")
    source_children = {
        source_doc["nodes"][index]["name"]: sorted(
            source_doc["nodes"][child]["name"]
            for child in source_doc["nodes"][index].get("children", [])
            if child in closure
        )
        for index in closure
    }

    for filename, (animation_name, duration) in EXPECTED.items():
        verify_asset(
            ANIMATION_DIR / filename, animation_name, duration, closure_names, bind_nodes, source_children
        )

    verify_reproducible()
    print("[OK] project36 portfolio animation assets verified")
    return 0


if __name__ == "__main__":
    sys.exit(main())
