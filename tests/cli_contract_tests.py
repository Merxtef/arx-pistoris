#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

from __future__ import annotations

import base64
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FIXTURE_ROOT = ROOT / "data" / "fixtures"
with (FIXTURE_ROOT / "catalog.json").open(encoding="utf-8") as fixture_catalog_stream:
    FIXTURE_CATALOG = json.load(fixture_catalog_stream)


def fixture_path(entry: dict[str, object], field: str) -> Path:
    value = entry[field]
    if isinstance(value, dict):
        value = value["path"]
    if not isinstance(value, str):
        raise TypeError(f"Fixture field {field!r} must resolve to a path string")
    return FIXTURE_ROOT / value


def glb_document(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    if len(data) < 20 or data[:4] != b"glTF":
        raise ValueError(f"Not a GLB file: {path}")
    chunk_length, chunk_type = struct.unpack_from("<II", data, 12)
    if chunk_type != 0x4E4F534A or 20 + chunk_length > len(data):
        raise ValueError(f"GLB has no valid JSON chunk: {path}")
    document = json.loads(data[20 : 20 + chunk_length].decode("utf-8"))
    if not isinstance(document, dict):
        raise ValueError(f"GLB JSON chunk is not an object: {path}")
    return document


def glb_image_paths(path: Path) -> tuple[str, ...]:
    result = []
    for image in glb_document(path).get("images", ()):
        if not isinstance(image, dict):
            continue
        value = image.get("uri", image.get("name"))
        if isinstance(value, str):
            result.append(value)
    return tuple(result)


def catalog_path(category: str, field: str) -> Path | None:
    fixtures = FIXTURE_CATALOG[category]
    return fixture_path(fixtures[0], field) if fixtures else None


def catalog_model_animation_paths() -> tuple[Path | None, Path | None, Path | None]:
    animations = FIXTURE_CATALOG["animations"]
    if not animations:
        return None, None, None

    animation = animations[0]
    model = next(
        (candidate for candidate in FIXTURE_CATALOG["models"] if candidate["name"] == animation["model"]),
        None,
    )
    if model is None:
        raise ValueError(f"Animation fixture {animation['name']!r} references an unknown Model fixture")
    return (
        fixture_path(model, "glb"),
        fixture_path(model, "ftl"),
        fixture_path(animation, "tea"),
    )


MODEL_GLB, MODEL_FTL, ANIMATION_TEA = catalog_model_animation_paths()
LEVEL_GLB = catalog_path("levels", "glb")
assert LEVEL_GLB is not None


def discover_arx_files(extension: str, root: Path = ROOT) -> tuple[Path, ...]:
    mount = root / "data" / "arx"
    normalized_extension = extension.lower()
    files = (
        path
        for path in mount.rglob("*")
        if path.is_file() and path.suffix.lower() == normalized_extension
    )
    return tuple(sorted(files, key=lambda path: path.as_posix().lower()))


def discover_level_triplets(root: Path = ROOT) -> tuple[tuple[Path, Path, Path], ...]:
    mount = root / "data" / "arx"
    triplets = []
    for dlf in discover_arx_files(".dlf", root):
        relative = dlf.relative_to(mount)
        llf = dlf.with_suffix(".llf")
        fts = mount / "game" / relative.parent / "fast.fts"
        if llf.is_file() and fts.is_file():
            triplets.append((fts, llf, dlf))
    return tuple(triplets)


CATALOG_LEVEL_TRIPLETS = tuple(
    (
        FIXTURE_ROOT / fixture["fts"],
        FIXTURE_ROOT / fixture["llf"],
        FIXTURE_ROOT / fixture["dlf"],
    )
    for fixture in FIXTURE_CATALOG["levels"]
)
LEVEL_FTS_FILES = tuple(triplet[0] for triplet in CATALOG_LEVEL_TRIPLETS) + discover_arx_files(".fts")
LEVEL_TRIPLETS = CATALOG_LEVEL_TRIPLETS + discover_level_triplets()
LEVEL_FTS_AVAILABLE = bool(LEVEL_FTS_FILES)
LEVEL_BUNDLE_AVAILABLE = bool(LEVEL_TRIPLETS)
LEVEL_FTS = LEVEL_FTS_FILES[0] if LEVEL_FTS_AVAILABLE else LEVEL_GLB
LEVEL_NAVIGATION_FTS = next(
    (
        FIXTURE_ROOT / fixture["fts"]
        for fixture in FIXTURE_CATALOG["levels"]
        if "navigation" in fixture.get("regeneration", {})
    ),
    None,
)
if LEVEL_NAVIGATION_FTS is None:
    raise ValueError("Fixture catalog must include a Level with navigation regeneration")
LEVEL_BUNDLE_FTS, LEVEL_LLF, LEVEL_DLF = LEVEL_TRIPLETS[0] if LEVEL_BUNDLE_AVAILABLE else (LEVEL_GLB,) * 3
if CATALOG_LEVEL_TRIPLETS:
    LEVEL_BUNDLE_NAME = Path(FIXTURE_CATALOG["levels"][0]["dlf"]).stem
    LEVEL_BUNDLE_SELECTOR = FIXTURE_CATALOG["levels"][0]["selector"]
else:
    LEVEL_BUNDLE_NAME = LEVEL_DLF.stem
    LEVEL_BUNDLE_SELECTOR = f"level:{LEVEL_BUNDLE_NAME.removeprefix('level')}"


def run(
    cli: Path,
    *args: str | Path,
    cwd: Path = ROOT,
    env: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    process_env = None
    if env is not None:
        process_env = os.environ.copy()
        process_env.update(env)
    return subprocess.run(
        [str(cli), *(str(arg) for arg in args)],
        cwd=cwd,
        env=process_env,
        text=True,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def expect_code(cli: Path, expected_returncode: int, expected_text: str, *args: str | Path) -> None:
    proc = run(cli, *args)
    text = proc.stdout + proc.stderr
    if proc.returncode != expected_returncode:
        raise AssertionError(f"{args}: expected rc {expected_returncode}, got {proc.returncode}\n{text}")
    if expected_text not in text:
        raise AssertionError(f"{args}: expected {expected_text!r} in output\n{text}")


def expect_success(cli: Path, *args: str | Path) -> subprocess.CompletedProcess[str]:
    proc = run(cli, *args)
    text = proc.stdout + proc.stderr
    if proc.returncode != 0:
        raise AssertionError(f"{args}: expected success, got {proc.returncode}\n{text}")
    return proc


def expect_success_contains(cli: Path, expected_text: str, *args: str | Path) -> None:
    proc = run(cli, *args)
    text = proc.stdout + proc.stderr
    if proc.returncode != 0:
        raise AssertionError(f"{args}: expected success, got {proc.returncode}\n{text}")
    if expected_text not in text:
        raise AssertionError(f"{args}: expected {expected_text!r} in output\n{text}")


def expect_success_not_contains(cli: Path, absent_text: str, *args: str | Path) -> None:
    proc = run(cli, *args)
    text = proc.stdout + proc.stderr
    if proc.returncode != 0:
        raise AssertionError(f"{args}: expected success, got {proc.returncode}\n{text}")
    if absent_text in text:
        raise AssertionError(f"{args}: did not expect {absent_text!r} in output\n{text}")


def help_output(cli: Path, *topics: str) -> str:
    proc = run(cli, "--help", *topics)
    if proc.returncode != 0 or not proc.stdout or proc.stderr:
        raise AssertionError(
            f"help {topics}: expected successful stdout-only output\nstdout={proc.stdout}\nstderr={proc.stderr}"
        )
    if "\x1b" in proc.stdout:
        raise AssertionError(f"help {topics}: redirected output must not contain ANSI escapes\n{proc.stdout}")
    return proc.stdout


def make_level_fts_json(level: int) -> dict[str, object]:
    zero = {"x": 0, "y": 0, "z": 0}
    vertices = [
        {"x": 0, "y": 0, "z": 0, "u": 0, "v": 0, "llfColorIdx": 0},
        {"x": 1, "y": 0, "z": 0, "u": 1, "v": 0, "llfColorIdx": 1},
        {"x": 0, "y": 0, "z": 1, "u": 0, "v": 1, "llfColorIdx": 2},
        {"x": 0, "y": 0, "z": 0, "u": 0, "v": 0},
    ]
    return {
        "$schema": "https://arx-tools.github.io/schemas/fts.schema.json",
        "header": {"levelIdx": level, "mScenePosition": zero},
        "uniqueHeaders": [],
        "textureContainers": [],
        "cells": [{} for _ in range(160 * 160)],
        "polygons": [
            {
                "vertices": vertices,
                "textureContainerId": 0,
                "norm": {"x": 0, "y": -1, "z": 0},
                "norm2": {"x": 0, "y": -1, "z": 0},
                "normals": [{"x": 0, "y": -1, "z": 0} for _ in range(4)],
                "transval": 0,
                "area": 0.5,
                "flags": 0,
                "room": 1,
            }
        ],
        "anchors": [],
        "portals": [],
        "rooms": [
            {"portals": [], "polygons": []},
            {"portals": [], "polygons": [{"cellX": 0, "cellY": 0, "polygonIdx": 0}]},
        ],
        "roomDistances": [
            {"distance": -1, "startPosition": zero, "endPosition": zero} for _ in range(4)
        ],
    }


def make_amb_track(sample_path: str | bytes, *, master: bool, loop_minus_one: int = 0) -> bytes:
    setting = lambda value: struct.pack("<ffII", value, value, 0, 0)
    key = struct.pack("<IIIII", 0, 0, loop_minus_one, 0, 0)
    key += setting(1.0) + setting(1.0)
    key += setting(0.0) * 4
    flags = 1 | (4 if master else 0)
    encoded_path = sample_path.encode("utf-8") if isinstance(sample_path, str) else sample_path
    return encoded_path + b"\0" + struct.pack("<II", flags, 1) + key


def make_amb_bytes(sample_path: str | bytes = "sfx/ambiance/test.wav") -> bytes:
    return struct.pack("<III", 0x424D4147, 0x01000001, 1) + make_amb_track(sample_path, master=True)


def make_timed_amb_bytes() -> bytes:
    return (
        struct.pack("<III", 0x424D4147, 0x01000001, 2)
        + make_amb_track("master.wav", master=True)
        + make_amb_track("child.wav", master=False, loop_minus_one=2)
    )


def make_bmp_bytes() -> bytes:
    return bytes(
        (
            66, 77, 58, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0,
            1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 24, 0, 0, 0, 0, 0, 0, 0,
            4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0, 0, 255, 0,
        )
    )


def make_wav_bytes(sample_count: int = 1) -> bytes:
    sample = b"\x80" * sample_count
    return (
        b"RIFF"
        + struct.pack("<I", 36 + len(sample))
        + b"WAVEfmt "
        + struct.pack("<IHHIIHH", 16, 1, 1, 8000, 8000, 1, 8)
        + b"data"
        + struct.pack("<I", len(sample))
        + sample
    )


def make_cinematic_glb() -> bytes:
    def png_chunk(kind: bytes, payload: bytes) -> bytes:
        body = kind + payload
        return struct.pack(">I", len(payload)) + body + struct.pack(">I", zlib.crc32(body))

    png = (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(b"\x00\xff\x00\x00\xff"))
        + png_chunk(b"IEND", b"")
    )
    positions = struct.pack(
        "<12f", -0.5, 0, -0.5, 0.5, 0, -0.5, 0.5, 0, 0.5, -0.5, 0, 0.5
    )
    texcoords = struct.pack("<8f", 0, 0, 1, 0, 1, 1, 0, 1)
    indices = struct.pack("<6H", 0, 2, 1, 0, 3, 2)
    binary = positions + texcoords + indices
    camera_rotation = [-0.7071067811865476, 0, 0, 0.7071067811865476]
    nodes = [
        {"name": "arx_cinematic__cinematic", "children": [1]},
        {"name": "arx_illustration__0__illustration", "mesh": 0, "children": [2, 5, 8]},
        {"name": "KEY_0__key", "rotation": camera_rotation, "children": [3]},
        {"name": "SOUND__EFFECT__sound", "children": [4]},
        {"name": "PATH_effects/hit.mp3__path"},
        {"name": "KEY_5__key", "rotation": camera_rotation, "children": [6]},
        {"name": "SOUND__SPEECH__sound", "children": [7]},
        {"name": "PATH_hero/line[English].mp3__path"},
        {"name": "KEY_10__key", "rotation": camera_rotation},
    ]
    document = {
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": nodes,
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(positions)},
            {"buffer": 0, "byteOffset": len(positions), "byteLength": len(texcoords)},
            {"buffer": 0, "byteOffset": len(positions) + len(texcoords), "byteLength": len(indices)},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 4, "type": "VEC3"},
            {"bufferView": 1, "componentType": 5126, "count": 4, "type": "VEC2"},
            {"bufferView": 2, "componentType": 5123, "count": 6, "type": "SCALAR"},
        ],
        "images": [
            {"name": "story/scene.png", "uri": "data:image/png;base64," + base64.b64encode(png).decode("ascii")}
        ],
        "textures": [{"source": 0}],
        "materials": [{"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
        "meshes": [
            {"primitives": [{"attributes": {"POSITION": 0, "TEXCOORD_0": 1}, "indices": 2, "material": 0}]}
        ],
    }
    json_chunk = json.dumps(document, separators=(",", ":")).encode("utf-8")
    json_chunk += b" " * (-len(json_chunk) % 4)
    return (
        struct.pack("<4sII", b"glTF", 2, 28 + len(json_chunk) + len(binary))
        + struct.pack("<II", len(json_chunk), 0x4E4F534A)
        + json_chunk
        + struct.pack("<II", len(binary), 0x004E4942)
        + binary
    )


def check_cinematic_route(cli: Path, tmp: Path) -> None:
    project = tmp / "cinematic-project"
    project.mkdir()
    source_glb = project / "intro.glb"
    source_glb.write_bytes(make_cinematic_glb())
    effect = project / "effects" / "hit.wav"
    effect.parent.mkdir()
    effect.write_bytes(make_wav_bytes(2))
    speech = project / "hero"
    speech.mkdir()
    (speech / "line[english].wav").write_bytes(make_wav_bytes(3))
    (speech / "line[french].wav").write_bytes(make_wav_bytes(4))

    game = tmp / "cinematic-game"
    game.mkdir()
    expect_success(cli, "--write-mount", game, source_glb, "cinematic:contract")
    native = game / "graph" / "interface" / "illustrations" / "contract.cin"
    expected_game_files = (
        native,
        game / "graph" / "interface" / "illustrations" / "scene.bmp",
        game / "sfx" / "hit.wav",
        game / "speech" / "english" / "line.wav",
        game / "speech" / "french" / "line.wav",
    )
    for path in expected_game_files:
        if not path.is_file():
            produced = [str(file.relative_to(game)) for file in game.rglob("*") if file.is_file()]
            raise AssertionError(f"Cinematic game output is missing {path}; produced {produced}")

    loose = tmp / "cinematic-loose"
    loose.mkdir()
    direct_native = loose / "direct.cin"
    expect_success(cli, "--mount", game, "cinematic:contract", direct_native)
    if direct_native.read_bytes() != native.read_bytes():
        raise AssertionError("Direct CIN conversion must preserve the native carrier")
    for path in (
        loose / "graph" / "interface" / "illustrations" / "scene.bmp",
        loose / "sfx" / "hit.wav",
        loose / "speech" / "english" / "line.wav",
        loose / "speech" / "french" / "line.wav",
    ):
        if not path.is_file():
            raise AssertionError(f"Direct CIN conversion is missing sidecar {path}")

    roundtrip_glb = loose / "intro.glb"
    expect_success(cli, "--mount", game, "cinematic:contract", roundtrip_glb)
    for path in (
        loose / "sounds" / "hit.wav",
        loose / "speech" / "line[english].wav",
        loose / "speech" / "line[french].wav",
    ):
        if not path.is_file():
            raise AssertionError(f"Cinematic loose output is missing {path}")
    roundtrip = glb_document(roundtrip_glb)
    names = {node.get("name") for node in roundtrip["nodes"]}
    for path in ("PATH_sounds/hit__path", "PATH_speech/line__path"):
        if path not in names:
            raise AssertionError(f"Cinematic selector rebase is missing {path}")
    if not roundtrip.get("images") or "bufferView" not in roundtrip["images"][0]:
        raise AssertionError("Cinematic GLB must embed its illustration image")

    absolute_glb = loose / "absolute-cin.glb"
    expect_success(
        cli,
        "--input-texture-folder", game,
        "--input-sound-folder", game,
        native,
        absolute_glb,
    )
    for path in (
        loose / "hit.wav",
        loose / "line[english].wav",
        loose / "line[french].wav",
    ):
        if not path.is_file():
            raise AssertionError(f"Absolute loose CIN lookup did not produce {path}")

    skipped = tmp / "cinematic-skipped"
    skipped.mkdir()
    expect_success(cli, "--write-mount", skipped, "--skip-sound-export", source_glb, "cinematic:skipped")
    if not (skipped / "graph" / "interface" / "illustrations" / "skipped.cin").is_file():
        raise AssertionError("Skipping Cinematic audio must still write CIN")
    if tuple(skipped.rglob("*.wav")):
        raise AssertionError("Skipping Cinematic audio must not write sound sidecars")

    targeted = tmp / "cinematic-targeted"
    targeted.mkdir()
    expect_success(
        cli,
        "--write-mount", targeted,
        "--rebase-sfx", "custom/effects",
        "--rebase-speech", "custom/speech",
        source_glb,
        "cinematic:targeted",
    )
    for path in (
        targeted / "sfx" / "custom" / "effects" / "hit.wav",
        targeted / "speech" / "english" / "custom" / "speech" / "line.wav",
        targeted / "speech" / "french" / "custom" / "speech" / "line.wav",
    ):
        if not path.is_file():
            raise AssertionError(f"Cinematic targeted rebase is missing {path}")
    expect_code(
        cli,
        1,
        "[CLI_INCOMPATIBLE_MODULES]",
        "--rebase-sounds", "shared",
        "--rebase-sfx", "effects",
        source_glb,
        "cinematic:conflicting",
    )


def make_animation_json() -> dict[str, object]:
    return {
        "$schema": "https://arx-tools.github.io/schemas/tea.schema.json",
        "header": {"name": "contract", "totalNumberOfFrames": 1},
        "keyframes": [
            {
                "flags": -1,
                "frame": 0,
                "groups": [
                    {
                        "isKey": True,
                        "quaternion": {"w": 1, "x": 0, "y": 0, "z": 0},
                        "translate": {"x": 0, "y": 0, "z": 0},
                    }
                ],
            }
        ],
    }


def make_empty_animation_json() -> dict[str, object]:
    return make_zero_group_animation_json("empty_contract")


def make_zero_group_animation_json(
    name: str, first_keyframe: dict[str, object] | None = None
) -> dict[str, object]:
    first = {"flags": -1, "frame": 0, "groups": []}
    if first_keyframe:
        first.update(first_keyframe)
    return {
        "$schema": "https://arx-tools.github.io/schemas/tea.schema.json",
        "header": {"name": name, "totalNumberOfFrames": 24},
        "keyframes": [
            first,
            {"flags": -1, "frame": 24, "groups": []},
        ],
    }


def check_native_text_route(cli: Path, tmp: Path) -> None:
    project = tmp / "native-text-project"
    project.mkdir()
    source = project / "latin1.amb"
    source.write_bytes(make_amb_bytes(b"caf\xe9.wav"))
    audio = make_wav_bytes(3)
    (project / "caf\u00e9.wav").write_bytes(audio)

    output = tmp / "native-text-output"
    output.mkdir()
    output_json = output / "latin1.json"
    expect_success(cli, "--native-text", "latin1", source, output_json)
    payload = json.loads(output_json.read_text(encoding="utf-8"))
    if payload["tracks"][0]["filename"] != "caf\u00e9.wav":
        raise AssertionError("Latin-1 native text must decode to UTF-8 JSON")
    exported_audio = output / "caf\u00e9.wav"
    if not exported_audio.is_file() or exported_audio.read_bytes() != audio:
        raise AssertionError("Latin-1 native references must resolve UTF-8 sidecar filenames")

    payload["tracks"][0]["filename"] = "price\u20ac.wav"
    unencodable_json = project / "unencodable.amb.json"
    unencodable_json.write_text(json.dumps(payload, ensure_ascii=False), encoding="utf-8")
    expect_success(
        cli,
        "--native-text", "utf8",
        "--skip-sound-export",
        unencodable_json,
        output / "utf8.amb",
    )
    latin1_output = output / "latin1.amb"
    expect_code(
        cli,
        1,
        "[CLI_AMBIANCE_INPUT_FAILED]",
        "--native-text", "latin1",
        "--skip-sound-export",
        unencodable_json,
        latin1_output,
    )
    if latin1_output.exists():
        raise AssertionError("Failed Latin-1 native output must not be published")


def main() -> int:
    global ANIMATION_TEA, MODEL_FTL, MODEL_GLB
    if len(sys.argv) != 2:
        print("usage: cli_contract_tests.py <arx-pistor executable>", file=sys.stderr)
        return 2

    cli = Path(sys.argv[1])
    if not LEVEL_FTS_AVAILABLE:
        print("Skipping optional Arx Level FTS checks; no *.fts files were discovered", file=sys.stderr)
    if not LEVEL_BUNDLE_AVAILABLE:
        print("Skipping optional Arx Level bundle checks; no complete FTS/LLF/DLF triplets were discovered", file=sys.stderr)
    with tempfile.TemporaryDirectory(prefix="arx-pistoris-cli-contract-") as tmp_raw:
        tmp = Path(tmp_raw)
        unknown_json = tmp / "unknown.json"
        unknown_json.write_text("{}", encoding="utf-8")
        empty_glb = tmp / "empty.glb"
        empty_glb_json = b'{"asset":{"version":"2.0"},"scene":0,"scenes":[{}]}'
        empty_glb_json += b" " * (-len(empty_glb_json) % 4)
        empty_glb.write_bytes(
            struct.pack("<4sII", b"glTF", 2, 20 + len(empty_glb_json))
            + struct.pack("<II", len(empty_glb_json), 0x4E4F534A)
            + empty_glb_json
        )
        bad_fts = tmp / "bad.fts"
        bad_llf = tmp / "bad.llf"
        bad_dlf = tmp / "bad.dlf"
        bad_recognized_fts = tmp / "bad-recognized.fts"
        bad_recognized_llf = tmp / "bad-recognized.llf"
        bad_recognized_dlf = tmp / "bad-recognized.dlf"
        bad_fts.write_bytes(b"not an FTS file")
        bad_llf.write_bytes(b"not an LLF file")
        bad_dlf.write_bytes(b"not a DLF file")
        if MODEL_FTL is None:
            model_obj = tmp / "contract-model.obj"
            model_obj.write_text(
                "mtllib contract-model.mtl\n"
                "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                "usemtl contract\nf 1 2 3\n",
                encoding="utf-8",
            )
            (tmp / "contract-model.mtl").write_text(
                "newmtl contract\nmap_Kd contract-model.bmp\n", encoding="utf-8"
            )
            (tmp / "contract-model.bmp").write_bytes(make_bmp_bytes())
            MODEL_FTL = tmp / "contract-model.ftl"
            expect_success(cli, model_obj, MODEL_FTL)
        if ANIMATION_TEA is None:
            animation_json = tmp / "contract-animation.tea.json"
            animation_json.write_text(json.dumps(make_animation_json()), encoding="utf-8")
            ANIMATION_TEA = tmp / "contract-animation.tea"
            expect_success(cli, animation_json, ANIMATION_TEA)

        expect_code(
            cli,
            1,
            "[CLI_RESOURCE_NOT_FOUND]",
            tmp / "missing-input.ftl",
            tmp / "missing-input.json",
        )
        missing_mtl_obj = tmp / "missing-mtl.obj"
        missing_mtl_obj.write_text("mtllib absent.mtl\n", encoding="utf-8")
        expect_code(
            cli,
            1,
            "[CLI_RESOURCE_NOT_FOUND]",
            missing_mtl_obj,
            tmp / "missing-mtl.ftl",
        )
        expect_code(
            cli,
            1,
            "[CLI_RESOURCE_NOT_FOUND]",
            "--input-icon",
            tmp / "missing-icon.png",
            MODEL_FTL,
            tmp / "missing-icon.glb",
        )
        expect_code(
            cli,
            1,
            "[CLI_RESOURCE_NOT_FOUND]",
            "--ftl-reference",
            tmp / "missing-reference.ftl",
            "--snap-bone-origins",
            MODEL_FTL,
            tmp / "missing-reference.glb",
        )
        dash_input = tmp / "-input.tea"
        dash_output = tmp / "-output.json"
        shutil.copyfile(ANIMATION_TEA, dash_input)
        dash_result = run(cli, "--", dash_input.name, dash_output.name, cwd=tmp)
        if dash_result.returncode != 0 or not dash_output.is_file():
            raise AssertionError(
                "-- must make option-like paths positional\n"
                + dash_result.stdout
                + dash_result.stderr
            )
        empty_animation_json = tmp / "empty-animation.tea.json"
        empty_animation_json.write_text(json.dumps(make_empty_animation_json()), encoding="utf-8")
        empty_animation_tea = tmp / "empty-animation.tea"
        expect_success(cli, empty_animation_json, empty_animation_tea)
        if not empty_animation_tea.is_file():
            raise AssertionError("standalone Animation output must write an empty Animation")
        active_collision_json = tmp / "active-collision.tea.json"
        active_collision_json.write_text(
            json.dumps(
                make_zero_group_animation_json(
                    "empty_contract", {"translate": {"x": 1, "y": 0, "z": 0}}
                )
            ),
            encoding="utf-8",
        )
        active_collision_tea = tmp / "active-collision.tea"
        expect_success(cli, active_collision_json, active_collision_tea)
        zero_group_nonempty_animations: list[tuple[str, Path]] = []
        for name, keyframe in (
            ("root_translation", {"translate": {"x": 1, "y": 0, "z": 0}}),
            ("root_rotation", {"quaternion": {"w": 0, "x": 0, "y": 1, "z": 0}}),
            ("footstep", {"flags": 9}),
            ("referenced_sound", {"sample": {"name": "contract.wav"}}),
        ):
            source_json = tmp / f"{name}.tea.json"
            source_json.write_text(json.dumps(make_zero_group_animation_json(name, keyframe)), encoding="utf-8")
            source_tea = tmp / f"{name}.tea"
            expect_success(cli, source_json, source_tea)
            zero_group_nonempty_animations.append((name, source_tea))
        referenced_sound_animation = dict(zero_group_nonempty_animations)["referenced_sound"]
        model_glb = tmp / "model.glb"
        expect_success(cli, MODEL_FTL, model_glb)
        if MODEL_GLB is None:
            MODEL_GLB = model_glb

        icon_input_directory = tmp / "icon-input"
        icon_input_directory.mkdir()
        icon_input = icon_input_directory / "source.ftl"
        shutil.copyfile(MODEL_FTL, icon_input)
        icon = make_bmp_bytes()
        (icon_input_directory / "source[icon].bmp").write_bytes(icon)

        native_icon_directory = tmp / "icon-native"
        native_icon_directory.mkdir()
        native_icon_output = native_icon_directory / "model.ftl"
        expect_success(cli, "--skip-texture-export", icon_input, native_icon_output)
        if (native_icon_directory / "model[icon].bmp").read_bytes() != icon:
            raise AssertionError("direct native Model output must preserve its inventory icon")

        intermediate_icon_directory = tmp / "icon-intermediate"
        intermediate_icon_directory.mkdir()
        intermediate_icon_output = intermediate_icon_directory / "model.glb"
        expect_success(cli, icon_input, intermediate_icon_output)
        intermediate_icon = (intermediate_icon_directory / "model[icon].png").read_bytes()
        if not intermediate_icon.startswith(b"\x89PNG\r\n\x1a\n"):
            raise AssertionError("intermediate Model output must render its inventory icon as PNG")

        derived_native_directory = tmp / "icon-derived-native"
        derived_native_directory.mkdir()
        derived_native_output = derived_native_directory / "model.ftl"
        expect_success(cli, "--skip-texture-export", "--icon-slots", "-", "-", icon_input, derived_native_output)
        derived_native_icon = (derived_native_directory / "model[icon].bmp").read_bytes()
        if not derived_native_icon.startswith(b"BM"):
            raise AssertionError("explicit icon operations must render native Model icons as BMP")

        centered_icon_output = intermediate_icon_directory / "centered.glb"
        expect_success(cli, "--icon-slots", "2", "1", "--icon-layout", "c", icon_input, centered_icon_output)
        centered_icon = (intermediate_icon_directory / "centered[icon].png").read_bytes()
        if struct.unpack_from(">II", centered_icon, 16) != (64, 32):
            raise AssertionError("--icon-slots must set the rendered inventory footprint")

        stretched_icon_output = intermediate_icon_directory / "stretched.glb"
        expect_success(cli, "--icon-slots", "2", "1", "--icon-layout", "stretch", icon_input, stretched_icon_output)
        stretched_icon = (intermediate_icon_directory / "stretched[icon].png").read_bytes()
        if struct.unpack_from(">II", stretched_icon, 16) != (64, 32) or stretched_icon == centered_icon:
            raise AssertionError("--icon-layout stretch must fill the rendered inventory footprint")

        bottom_right_prefix_output = intermediate_icon_directory / "bottom-right-prefix.glb"
        expect_success(
            cli,
            "--icon-slots",
            "2",
            "1",
            "--icon-layout",
            "bottom-r",
            icon_input,
            bottom_right_prefix_output,
        )
        bottom_right_full_output = intermediate_icon_directory / "bottom-right-full.glb"
        expect_success(
            cli,
            "--icon-slots",
            "2",
            "1",
            "--icon-layout",
            "bottom-right",
            icon_input,
            bottom_right_full_output,
        )
        if (intermediate_icon_directory / "bottom-right-prefix[icon].png").read_bytes() != (
            intermediate_icon_directory / "bottom-right-full[icon].png"
        ).read_bytes():
            raise AssertionError("unambiguous icon-layout prefixes must resolve to the full layout")

        invalid_icon_directory = tmp / "icon-invalid"
        invalid_icon_directory.mkdir()
        invalid_icon_input = invalid_icon_directory / "source.ftl"
        shutil.copyfile(MODEL_FTL, invalid_icon_input)
        invalid_icon = invalid_icon_directory / "source[icon].png"
        invalid_icon.write_bytes(b"not an image")
        invalid_native_output = invalid_icon_directory / "native.ftl"
        expect_success_contains(
            cli,
            "Invalid Model inventory icon was skipped",
            "--skip-texture-export",
            invalid_icon_input,
            invalid_native_output,
        )
        if (invalid_icon_directory / "native[icon].png").exists():
            raise AssertionError("invalid automatically discovered icon must be omitted")
        expect_success_contains(
            cli,
            "Invalid Model inventory icon was skipped",
            invalid_icon_input,
            invalid_icon_directory / "automatic.glb",
        )
        expect_code(
            cli,
            1,
            "[CLI_MODEL_INPUT_FAILED]",
            "--input-icon",
            invalid_icon,
            invalid_icon_input,
            invalid_icon_directory / "explicit.glb",
        )
        expect_code(
            cli,
            1,
            "[CLI_MODEL_INPUT_FAILED]",
            "--skip-texture-export",
            "--input-icon",
            invalid_icon,
            invalid_icon_input,
            invalid_icon_directory / "explicit.ftl",
        )

        glb_as_fts = tmp / "glb-as-fts.fts"
        glb_as_fts.write_bytes(model_glb.read_bytes())
        fts_header = bytearray(280)
        struct.pack_into("<i", fts_header, 256, -1)
        struct.pack_into("<f", fts_header, 260, 0.141)
        bad_recognized_fts.write_bytes(fts_header)
        llf_header = bytearray(7464)
        struct.pack_into("<f", llf_header, 0, 1.44)
        llf_header[4 : 4 + len(b"DANAE_LLH_FILE\0")] = b"DANAE_LLH_FILE\0"
        struct.pack_into("<i", llf_header, 280, -1)
        bad_recognized_llf.write_bytes(llf_header)
        dlf_header = bytearray(8520)
        struct.pack_into("<f", dlf_header, 0, 1.44)
        dlf_header[4 : 4 + len(b"DANAE_FILE\0")] = b"DANAE_FILE\0"
        struct.pack_into("<i", dlf_header, 304, 2)
        bad_recognized_dlf.write_bytes(dlf_header)
        out_json = tmp / "out.json"
        out_glb = tmp / "out.glb"
        level_json = tmp / "level1.fts.json"
        level_json.write_text(json.dumps(make_level_fts_json(1)), encoding="utf-8")
        ambiance_amb = tmp / "ambiance.amb"
        ambiance_amb.write_bytes(make_amb_bytes())
        ambiance_collision_input = tmp / "ambiance-collision-source.amb"
        ambiance_collision_input.write_bytes(make_amb_bytes("ambiance-collision.amb"))
        ambiance_collision_output = tmp / "ambiance-collision.amb"
        ambiance_collision_audio = make_wav_bytes()
        ambiance_collision_output.write_bytes(ambiance_collision_audio)

        cli_help = help_output(cli)
        if help_output(cli, "cli") != cli_help:
            raise AssertionError("--help and --help cli must select the same page")
        if not cli_help.startswith("CLI / GENERAL\n"):
            raise AssertionError(f"default help must identify its page\n{cli_help}")
        for expected in (
            "arx-pistor --auto-mount level:1 level1.glb",
            "--mount <FOLDER>",
            "--auto-mount",
            "--keep-first-resource",
            "cli selectors",
            "cli formats",
            "level debug",
        ):
            if expected not in cli_help:
                raise AssertionError(f"default help is missing {expected!r}\n{cli_help}")
        if "Perform conversion and validation without writing output files." not in " ".join(cli_help.split()):
            raise AssertionError(f"default help must describe dry-run behavior directly\n{cli_help}")
        for route_option in ("--gen-minimap", "--skip-texture-export", "--skip-sound-export"):
            if route_option in cli_help:
                raise AssertionError(f"default help must not dump route option {route_option!r}\n{cli_help}")
        if str(cli) in cli_help:
            raise AssertionError(f"help examples must use the stable product name\n{cli_help}")

        selectors_help = help_output(cli, "cli", "selectors")
        if not selectors_help.startswith("CLI / SELECTORS\n"):
            raise AssertionError(f"selector help must identify its page\n{selectors_help}")
        for selector in (
            "level:<N>",
            "model:<type>:<name>[:<tweak>]",
            "anim:<npc|fix_inter>:<name>",
            "ambiance:<name>",
            "cinematic:<name>",
            "Without an explicit --mount, reads begin in the current directory",
        ):
            if selector not in selectors_help:
                raise AssertionError(f"selector help is missing {selector!r}\n{selectors_help}")
        if "--mount folders are searched from left to right" not in selectors_help:
            raise AssertionError(f"selector help must explain mount priority directly\n{selectors_help}")

        formats_help = help_output(cli, "cli", "formats")
        if not formats_help.startswith("CLI / FORMATS\n"):
            raise AssertionError(f"format help must identify its page\n{formats_help}")
        for formats in (
            "Primary inputs    TEA, JSON",
            "Primary inputs    AMB, JSON, GLB",
            "Primary inputs    CIN, GLB",
            "Primary inputs    FTL, OBJ, JSON, GLB",
            "Primary inputs    FTS, DLF, JSON, GLB",
        ):
            if formats not in formats_help:
                raise AssertionError(f"format help is missing {formats!r}\n{formats_help}")
        if "Companion inputs" not in formats_help or "Additional inputs" in formats_help:
            raise AssertionError(f"format help must identify companion inputs consistently\n{formats_help}")

        level_help = help_output(cli, "level")
        model_help = help_output(cli, "model")
        animation_help = help_output(cli, "animation")
        ambiance_help = help_output(cli, "ambiance")
        cinematic_help = help_output(cli, "cinematic")
        for page, heading in (
            (level_help, "LEVEL / GENERAL"),
            (model_help, "MODEL / GENERAL"),
            (animation_help, "ANIMATION / GENERAL"),
            (ambiance_help, "AMBIANCE / GENERAL"),
            (cinematic_help, "CINEMATIC / GENERAL"),
        ):
            if not page.startswith(heading + "\n"):
                raise AssertionError(f"route help must start with {heading!r}\n{page}")

        for expected in (
            "Convert native Level bundles, compatible JSON, and editable Level GLB.",
            "--gen-room-distances",
            "--gen-navigation",
            "--gen-nav-surface",
            "--gen-anchors",
            "--weld-vertices",
            "    --weld-radius",
            "--weld-radius <UNITS=0.0001>",
            "--weld-metric <MODE=euclidean>",
            "--weld-degenerate-faces <MODE=preserve>",
            "--flatten-portals",
            "--snap-to-portals",
            "    --portal-snap-radius",
            "--portal-snap-radius <UNITS=1>",
            "    --nav-from-floor",
            "    --nav-radius",
            "--nav-radius <UNITS=50>",
            "--prune-nav-surface-islands",
            "    --nav-prune-min-area",
            "    --rdist-spacing",
            "    --rdist-offset",
            "    --rdist-height",
            "    --rdist-link-distance",
            "--connect-anchors",
            "--prune-anchor-islands",
            "    --anchor-prune-min-count",
            "--anchor-spacing <UNITS=2*radius>",
            "--gen-static-lighting",
            "    --light-ambient",
            "--gen-minimap",
            "--minimap-border-color",
            "    --minimap-fg-color",
            "    --minimap-fg-image",
            "    --minimap-bg-color",
            "    --minimap-bg-image",
            "    --minimap-halo-radius",
            "    --nav-max-slope",
            "--glb-arx-units-per-unit <UNITS=100>",
            "--glb-offset <X=0> <Y=0> <Z=0>",
            "--dlf-only",
            "--no-quad-reconstruction",
            "--dlf-scene-directory",
            "--pretty",
        ):
            if expected not in level_help:
                raise AssertionError(f"Level help is missing {expected!r}\n{level_help}")
        normalized_level_help = " ".join(level_help.split())
        implication_summary = (
            "Includes: --gen-nav-surface, --prune-nav-surface-islands, --gen-anchors, "
            "--connect-anchors, --prune-anchor-islands. Included flags and their options may also be "
            "specified explicitly."
        )
        if implication_summary not in normalized_level_help:
            raise AssertionError(f"Level help must describe the navigation preset\n{level_help}")
        for absent in ("--minimap-projection-offset", "--skip-sound-export"):
            if absent in level_help:
                raise AssertionError(f"Level help unexpectedly contains {absent!r}\n{level_help}")

        for expected in (
            "--glb-arx-units-per-unit <UNITS=10>",
            "--rebase-textures",
            "--skip-texture-export",
            "--skip-sound-export",
            "--input-sound-folder <PATH>",
            "--rebase-sounds <RESOURCE-DIRECTORY>",
            "--allow-empty-animation",
            "--icon-slots",
            "--icon-layout <LAYOUT=CENTER>",
        ):
            if expected not in model_help:
                raise AssertionError(f"Model help is missing {expected!r}\n{model_help}")
        if "--glb-offset" in model_help:
            raise AssertionError(f"Model help must not advertise its ignored GLB offset\n{model_help}")
        for retired_model_option in (
            "--overwrite-texture",
            "--rename-selections",
            "--autosize-to-reference",
            "--snap-action-points-to-reference",
            "--copy-synthetic-selection-affiliations",
            "--center-icon",
            "--stretch-icon",
        ):
            if retired_model_option in model_help:
                raise AssertionError(f"Model help contains retired option {retired_model_option!r}\n{model_help}")

        for expected in ("--skip-sound-export", "--input-sound-folder <PATH>", "--rebase-sounds <RESOURCE-DIRECTORY>"):
            if expected not in animation_help:
                raise AssertionError(f"Animation help is missing {expected!r}\n{animation_help}")
        for absent in ("--glb-arx-units-per-unit", "--skip-texture-export"):
            if absent in animation_help:
                raise AssertionError(f"Animation help unexpectedly contains {absent!r}\n{animation_help}")

        for expected in (
            "--glb-arx-units-per-unit <UNITS=10>",
            "--trim-to-master",
            "--reference-model <MODEL-PATH>",
            "--skip-sound-export",
            "--input-sound-folder <PATH>",
            "--rebase-sounds <RESOURCE-DIRECTORY>",
        ):
            if expected not in ambiance_help:
                raise AssertionError(f"Ambiance help is missing {expected!r}\n{ambiance_help}")
        for absent in ("--glb-offset", "--skip-texture-export"):
            if absent in ambiance_help:
                raise AssertionError(f"Ambiance help unexpectedly contains {absent!r}\n{ambiance_help}")

        for expected in (
            "Convert native CIN and editable GLB Cinematics.",
            "--skip-texture-export",
            "--skip-sound-export",
            "--input-texture-folder <PATH>",
            "--input-sound-folder <PATH>",
            "--rebase-textures <RESOURCE-DIRECTORY>",
            "--rebase-sounds <RESOURCE-DIRECTORY>",
            "--rebase-sfx <RESOURCE-DIRECTORY>",
            "--rebase-speech <RESOURCE-DIRECTORY>",
            "cinematic:intro intro.glb",
        ):
            if expected not in cinematic_help:
                raise AssertionError(f"Cinematic help is missing {expected!r}\n{cinematic_help}")
        for absent in ("--glb-arx-units-per-unit", "--glb-offset"):
            if absent in cinematic_help:
                raise AssertionError(f"Cinematic help unexpectedly contains {absent!r}\n{cinematic_help}")

        for page, expected in (
            (model_help, "model:npc:human_base anim:npc:human_normal_walk human_base.glb"),
            (animation_help, "anim:npc:human_normal_walk human_normal_walk.json"),
            (ambiance_help, "ambiance:ambient_cave_a ambient_cave_a.glb"),
        ):
            if expected not in page:
                raise AssertionError(f"route help is missing usable stock-resource example {expected!r}\n{page}")
        if "textures referenced by a loose --reference-model input" not in " ".join(ambiance_help.split()):
            raise AssertionError(f"Ambiance help must explain reference Model texture lookup\n{ambiance_help}")

        level_debug_help = help_output(cli, "level", "debug")
        if not level_debug_help.startswith("LEVEL / DEBUG\n"):
            raise AssertionError(f"Level debug help must identify its page\n{level_debug_help}")
        for option in ("--debug-cells", "--debug-navigation", "--debug-room-distances"):
            if option not in level_debug_help:
                raise AssertionError(f"Level debug help is missing {option!r}\n{level_debug_help}")
        if "--gen-minimap" in level_debug_help:
            raise AssertionError(f"Level debug help must not contain normal Level options\n{level_debug_help}")

        if help_output(cli, "lev") != level_help or help_output(cli, "level", "d") != level_debug_help:
            raise AssertionError("unambiguous help topic prefixes must resolve to the same pages")
        if help_output(cli, "levle") != cli_help or help_output(cli, "--gen-minimap") != cli_help:
            raise AssertionError("unknown or option-like primary help words must keep CLI / GENERAL")
        if help_output(cli, "level", "ambiance") != level_help or help_output(cli, "level", "--gen-minimap") != level_help:
            raise AssertionError("unknown or option-like refinements must keep the selected general page")
        expect_code(cli, 1, "[CLI_HELP_TOPIC_AMBIGUOUS]", "--help", "a")

        expect_success_contains(cli, "arx-pistor", "--version", "--ignored-after-version")
        expect_code(cli, 1, "[CLI_DUPLICATE_MODULE]", "--pretty", "--pretty")
        expect_code(cli, 1, "[CLI_AMBIGUOUS_OPTION]", "--glb")
        expect_code(cli, 1, "[CLI_UNKNOWN_OPTION]", "--definitely-missing")
        parse_error = run(cli, "--definitely-missing")
        if (
            parse_error.stdout
            or "Usage:" in parse_error.stderr
            or "Run arx-pistor --help for usage." not in parse_error.stderr
            or "\x1b" in parse_error.stderr
        ):
            raise AssertionError(
                f"parse failure emitted the wrong streams or full usage\n{parse_error.stdout}{parse_error.stderr}"
            )
        expect_code(cli, 1, "[CLI_UNKNOWN_OPTION]", "--foo", "--help")
        expect_code(cli, 1, "[CLI_UNKNOWN_OPTION]", "--debug-rooms")
        expect_code(cli, 1, "[CLI_UNKNOWN_OPTION]", "--debug-fts-rooms")
        expect_code(cli, 1, "[CLI_UNKNOWN_OPTION]", "--weld-debug")
        expect_code(cli, 1, "[CLI_UNKNOWN_OPTION]", "--normal-weld-degrees")
        for retired_level_option in (
            "--generate-anchors",
            "--generate-minimap",
            "--generate-nav-surface",
            "--generate-room-distances",
            "--generate-static-lighting",
            "--minimap-foreground-color",
            "--minimap-foreground-image",
            "--minimap-background-color",
            "--minimap-background-image",
            "--nav-max-slope-degrees",
            "--fts-scene-directory",
        ):
            expect_code(cli, 1, "[CLI_UNKNOWN_OPTION]", retired_level_option)
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--input-texture-folder")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--input-icon")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--icon-slots")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--icon-slots", "1")
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--icon-slots", "0", "1")
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--icon-slots", "-", "4")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--icon-layout")
        expect_code(cli, 1, "[CLI_INVALID_MODE]", "--icon-layout", "b")
        expect_code(cli, 1, "[CLI_INVALID_MODE]", "--icon-layout", "middle")
        expect_code(
            cli,
            1,
            "[CLI_DUPLICATE_MODULE]",
            "--icon-layout",
            "center",
            "--icon-layout",
            "stretch",
        )
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--rebase-textures")
        expect_code(cli, 1, "[CLI_RESOURCE_PATH_INVALID]", "--rebase-textures", "", MODEL_FTL, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--input-sound-folder")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--rebase-sounds")
        expect_code(cli, 1, "[CLI_RESOURCE_PATH_INVALID]", "--rebase-sounds", "", ambiance_amb, out_glb)
        expect_code(
            cli,
            1,
            "[CLI_DUPLICATE_MODULE]",
            "--trim-to-master",
            "--trim-to-master",
            ambiance_amb,
            tmp / "duplicate-trim.json",
        )
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--dlf-scene-directory")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--mount")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--write-mount")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--write-mount", "", MODEL_FTL, out_glb)
        expect_code(cli, 1, "[CLI_DUPLICATE_MODULE]", "--write-mount", ".", "--write-mount", ".")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--native-text")
        expect_code(cli, 1, "[CLI_INVALID_MODE]", "--native-text", "other", MODEL_FTL, out_glb)
        expect_code(cli, 1, "[CLI_DUPLICATE_MODULE]", "--native-text", "auto", "--native-text", "utf8")
        expect_success(cli, "--native-text", "latin", MODEL_FTL, tmp / "native-text.glb")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--log-level")
        expect_code(cli, 1, "[CLI_LOG_LEVEL_INVALID]", "--log-level", "verbose")
        expect_code(cli, 1, "[CLI_DUPLICATE_MODULE]", "--log-level", "info", "--log-level", "warn")
        expect_code(cli, 1, "[CLI_KIND_INVALID]", "--kind", "asset", MODEL_GLB, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_INPUT_OUTPUT]", str(MODEL_GLB))
        expect_code(cli, 1, "[CLI_UNSUPPORTED_OUTPUT_FORMAT]", MODEL_GLB, tmp / "out.bin")
        expect_code(cli, 1, "[CLI_RESOURCE_SELECTOR_INVALID]", MODEL_FTL, "anim:items:bad")
        expect_code(cli, 1, "[CLI_RESOURCE_SELECTOR_INVALID]", "level:not-a-number", out_glb)
        expect_code(
            cli,
            1,
            "[CLI_ROUTE_CONSTRAINT_CONFLICT]",
            "--kind",
            "level",
            "--skip-sound-export",
            level_json,
            out_glb,
        )
        expect_code(
            cli,
            1,
            "[CLI_ROUTE_CONSTRAINT_CONFLICT]",
            "--kind",
            "ambiance",
            "--skip-texture-export",
            ambiance_amb,
            tmp / "ambiance-skip-texture.json",
        )
        expect_code(
            cli,
            1,
            "[CLI_ROUTE_INPUT_MISMATCH]",
            ANIMATION_TEA,
            ANIMATION_TEA,
            tmp / "multiple-animation-inputs.tea",
        )
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--overwrite", "--no-overwrite", MODEL_GLB, out_glb)
        expect_code(
            cli,
            1,
            "[CLI_INCOMPATIBLE_MODULES]",
            "--dry-run",
            "--overwrite",
            "--no-overwrite",
            MODEL_GLB,
            out_glb,
        )
        expect_code(cli, 1, "[CLI_DUPLICATE_MODULE]", "--dry-run", "--dry-run")
        expect_code(
            cli,
            1,
            "[CLI_OUTPUT_CONVERTER_CONFLICT]",
            "--gen-anchors",
            "--debug-navigation",
            "--debug-room-distances",
            LEVEL_FTS,
            out_glb,
        )
        expect_code(
            cli,
            1,
            "[CLI_MISSING_DEPENDENCY]",
            "--snap-bone-origins",
            MODEL_GLB,
            out_glb,
        )
        expect_code(
            cli,
            1,
            "[CLI_INVALID_MODULE_VALUE]",
            "--ftl-reference",
            MODEL_FTL,
            MODEL_GLB,
            out_glb,
        )
        expect_success(
            cli,
            "--ftl-reference",
            MODEL_FTL,
            "--snap-bone-origins",
            "--copy-bone-selections",
            "--copy-action-selections",
            "--skip-texture-export",
            MODEL_FTL,
            tmp / "snapped-reference.ftl",
        )
        expect_success(
            cli,
            "--infer-bone-selections",
            "--skip-texture-export",
            MODEL_FTL,
            tmp / "inferred-bone-selections.ftl",
        )
        expect_code(
            cli,
            1,
            "[CLI_INCOMPATIBLE_MODULES]",
            "--ftl-reference",
            MODEL_FTL,
            "--copy-bone-selections",
            "--infer-bone-selections",
            MODEL_FTL,
            tmp / "conflicting-bone-selections.ftl",
        )
        ambiance_glb = tmp / "ambiance.glb"
        expect_success(cli, ambiance_amb, ambiance_glb)
        expect_success(cli, "--kind", "ambiance", ambiance_glb, tmp / "ambiance-roundtrip.amb")
        ambiance_json = tmp / "ambiance.json"
        expect_success(cli, ambiance_amb, ambiance_json)
        ambiance_payload = json.loads(ambiance_json.read_text(encoding="utf-8"))
        if ambiance_payload.get("$schema") != "https://arx-tools.github.io/schemas/amb.schema.json":
            raise AssertionError(f"AMB JSON schema mismatch: {ambiance_payload.get('$schema')}")
        expect_success(cli, ambiance_json, tmp / "ambiance-json-roundtrip.amb")
        check_native_text_route(cli, tmp)
        check_cinematic_route(cli, tmp)

        one_track_trimmed = tmp / "ambiance-one-track-trimmed.json"
        expect_success(cli, "--trim-to-master", "--skip-sound-export", ambiance_amb, one_track_trimmed)

        trim_input = tmp / "ambiance-trim-input"
        trim_input.mkdir()
        timed_ambiance = trim_input / "timed.amb"
        timed_ambiance.write_bytes(make_timed_amb_bytes())
        (trim_input / "master.wav").write_bytes(make_wav_bytes(8000))
        (trim_input / "child.wav").write_bytes(make_wav_bytes(8000))
        trim_output = tmp / "ambiance-trim-output"
        trim_output.mkdir()
        trimmed_json = trim_output / "timed.json"
        expect_success_contains(
            cli,
            "trimmed 1 non-master Ambiance track(s) to the master duration",
            "--trim-to-master",
            "--skip-sound-export",
            timed_ambiance,
            trimmed_json,
        )
        trimmed_payload = json.loads(trimmed_json.read_text(encoding="utf-8"))
        if trimmed_payload["tracks"][1]["keys"][0]["loop"] != 1:
            raise AssertionError("--trim-to-master must reduce trailing child-track repetitions")
        if tuple(trim_output.rglob("*.wav")):
            raise AssertionError("--skip-sound-export must suppress audio output needed only for trimming")

        missing_trim_input = tmp / "ambiance-trim-missing"
        missing_trim_input.mkdir()
        missing_timed_ambiance = missing_trim_input / "timed.amb"
        missing_timed_ambiance.write_bytes(make_timed_amb_bytes())
        missing_trim_output = tmp / "ambiance-trim-missing.json"
        expect_code(
            cli,
            1,
            "[CLI_AMBIANCE_MODULE_FAILED]",
            "--trim-to-master",
            "--skip-sound-export",
            missing_timed_ambiance,
            missing_trim_output,
        )
        if missing_trim_output.exists():
            raise AssertionError("failed Ambiance track trimming must not publish output")

        expect_code(
            cli,
            1,
            "[CLI_RESOURCE_OUTPUT_COLLISION]",
            "--overwrite",
            ambiance_collision_input,
            ambiance_collision_output,
        )
        if ambiance_collision_output.read_bytes() != ambiance_collision_audio:
            raise AssertionError("Ambiance resource collisions must be resolved before writing the primary output")
        expect_success(
            cli,
            ambiance_amb,
            tmp / "ambiance-reference.glb",
            "--reference-model",
            MODEL_FTL,
            "--input-texture-folder",
            tmp / "unused-reference-textures",
        )
        expect_code(
            cli,
            1,
            "[CLI_MODULE_OUTPUT_MISMATCH]",
            ambiance_amb,
            tmp / "ambiance-reference.amb",
            "--reference-model",
            MODEL_FTL,
        )
        expect_code(cli, 1, "[CLI_AMBIGUOUS_ROUTE]", MODEL_GLB, out_glb)
        expect_code(cli, 1, "[CLI_AMBIGUOUS_ROUTE]", unknown_json, out_json)
        expect_success(cli, "--overwrite", "--sign-level", "unused", level_json, tmp / "level2.fts.json")
        if (tmp / "level2.dlf.json").exists() or (tmp / "level2.llf.json").exists():
            raise AssertionError("direct partial native JSON output must not synthesize missing siblings")
        level2 = json.loads((tmp / "level2.fts.json").read_text(encoding="utf-8"))
        if level2["header"]["levelIdx"] != 2:
            raise AssertionError("Level JSON output filename must override the FTS level number")

        expect_success(
            cli,
            "--overwrite",
            "--no-quad-reconstruction",
            "--sign-level",
            "editor",
            level_json,
            tmp / "level3.fts.json",
        )
        for extension in ("fts", "dlf", "llf"):
            if not (tmp / f"level3.{extension}.json").is_file():
                raise AssertionError("Level-mediated JSON output must emit the full native triplet")
        for extension in ("dlf", "llf"):
            signed = json.loads((tmp / f"level3.{extension}.json").read_text(encoding="utf-8"))
            if signed["header"]["lastModifiedBy"] != "arx-pistoris/editor":
                raise AssertionError("--sign-level must stamp emitted DLF and LLF metadata")
            if signed["header"]["lastModifiedAt"] <= 0:
                raise AssertionError("emitted DLF and LLF metadata must contain a current timestamp")

        expect_success(cli, "--overwrite", tmp / "level3.fts.json", tmp / "level6.fts.json")
        if (tmp / "level6.dlf.json").exists() or (tmp / "level6.llf.json").exists():
            raise AssertionError("Level JSON input must not discover sibling carriers")

        arbitrary_llf = tmp / "lighting.json"
        arbitrary_dlf = tmp / "scene.json"
        shutil.copyfile(tmp / "level3.llf.json", arbitrary_llf)
        shutil.copyfile(tmp / "level3.dlf.json", arbitrary_dlf)
        expect_success(
            cli,
            "--overwrite",
            tmp / "level3.fts.json",
            arbitrary_dlf,
            arbitrary_llf,
            tmp / "level4.fts.json",
        )
        for extension in ("fts", "dlf", "llf"):
            output = tmp / f"level4.{extension}.json"
            if not output.is_file():
                raise AssertionError("direct complete native JSON output must preserve every explicit carrier")
        level4_dlf = json.loads((tmp / "level4.dlf.json").read_text(encoding="utf-8"))
        if level4_dlf["header"]["levelIdx"] != 4:
            raise AssertionError("Level JSON output filename must override the DLF level number")

        expect_success(cli, "--overwrite", "--dlf-only", level_json, tmp / "level5.fts.json")
        if (
            (tmp / "level5.fts.json").exists()
            or (tmp / "level5.llf.json").exists()
            or not (tmp / "level5.dlf.json").is_file()
        ):
            raise AssertionError("--dlf-only Level JSON output must write only the DLF carrier")

        expect_success(
            cli,
            "--overwrite",
            "--input-texture-folder",
            tmp / "unused-textures",
            level_json,
            tmp / "level7.fts.json",
        )
        expect_success(
            cli,
            "--overwrite",
            "--rebase-textures",
            "graph/obj3d/textures",
            level_json,
            tmp / "level8.fts.json",
        )
        expect_code(
            cli,
            1,
            "[CLI_MODULE_OUTPUT_MISMATCH]",
            "--dlf-scene-directory",
            "graph/levels/custom",
            level_json,
            tmp / "level8.fts.json",
        )
        expect_code(cli, 1, "[CLI_MODEL_INPUT_FAILED]", unknown_json, ANIMATION_TEA, out_json)
        expect_code(cli, 1, "[CLI_MODEL_INPUT_FAILED]", unknown_json, tmp / "claimed-model.ftl")
        expect_code(cli, 1, "[CLI_ANIMATION_INPUT_FAILED]", unknown_json, tmp / "claimed-animation.tea")
        expect_code(cli, 1, "[CLI_LEVEL_INPUT_FAILED]", "--debug-cells", empty_glb, out_glb)
        expect_code(
            cli,
            1,
            "[CLI_LEVEL_INPUT_FAILED]",
            "--mount",
            tmp,
            empty_glb,
            "empty.dlf",
        )
        expect_code(
            cli,
            1,
            "[CLI_LEVEL_INPUT_FAILED]",
            "--dlf-only",
            "--no-quad-reconstruction",
            "--skip-texture-export",
            "--rebase-textures",
            "graph/obj3d/textures",
            empty_glb,
            tmp / "empty.fts",
        )
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--gen-nav-surface", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--gen-nav-surface", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--gen-navigation", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--gen-navigation", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--prune-nav-surface-islands", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--prune-nav-surface-islands", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--gen-room-distances", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--gen-room-distances", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--gen-anchors", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--gen-anchors", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--connect-anchors", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--connect-anchors", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--prune-anchor-islands", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--prune-anchor-islands", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--gen-static-lighting", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--gen-static-lighting", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--nav-radius", "50", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--nav-from-floor", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--nav-prune-ratio", "0.1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--nav-prune-min-area", "100", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--anchor-prune-ratio", "0.1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--anchor-prune-min-count", "2", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--weld-radius", "0.1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--weld-metric", "euclidean", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--portal-snap-radius", "1", LEVEL_FTS, out_glb)
        expect_code(
            cli,
            1,
            "[CLI_MISSING_DEPENDENCY]",
            "--weld-degenerate-faces",
            "preserve",
            LEVEL_FTS,
            out_glb,
        )
        expect_code(
            cli,
            1,
            "[CLI_INCOMPATIBLE_MODULES]",
            "--gen-nav-surface",
            "--nav-from-floor",
            "--nav-radius",
            "50",
            LEVEL_FTS,
            out_glb,
        )
        expect_code(
            cli,
            1,
            "[CLI_INCOMPATIBLE_MODULES]",
            "--gen-nav-surface",
            "--nav-height",
            "165",
            "--nav-from-floor",
            LEVEL_FTS,
            out_glb,
        )
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--rdist-spacing", "100", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--rdist-offset", "10", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--rdist-height", "82.5", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--rdist-link-distance", "150", LEVEL_FTS, out_glb)
        if LEVEL_FTS_AVAILABLE:
            expect_success(cli, "--debug-navigation", LEVEL_FTS, out_glb)
            expect_success(cli, "--debug-room-distances", LEVEL_FTS, out_glb)
            expect_success(
                cli,
                "--weld-vertices",
                "--weld-radius",
                "0.0001",
                "--weld-metric",
                "axis-aligned",
                "--weld-degenerate-faces",
                "preserve",
                LEVEL_FTS,
                tmp / "welded.glb",
            )
            expect_success(
                cli,
                "--weld-vertices",
                "--debug-cells",
                LEVEL_FTS,
                tmp / "welded-debug-cells.glb",
            )
            expect_success(
                cli,
                "--snap-to-portals",
                "--portal-snap-radius",
                "1",
                LEVEL_FTS,
                tmp / "portal-snapped.glb",
            )
            expect_success(cli, "--flatten-portals", LEVEL_FTS, tmp / "portals-flattened.glb")
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--weld-vertices", "--weld-radius", "0", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--weld-vertices", "--weld-radius", "nan", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--snap-to-portals", "--portal-snap-radius", "0", LEVEL_FTS, out_glb)
        expect_code(
            cli,
            1,
            "[CLI_INVALID_MODULE_VALUE]",
            "--snap-to-portals",
            "--portal-snap-radius",
            "nan",
            LEVEL_FTS,
            out_glb,
        )
        expect_code(cli, 1, "[CLI_INVALID_MODE]", "--weld-vertices", "--weld-metric", "box", LEVEL_FTS, out_glb)
        expect_code(
            cli,
            1,
            "[CLI_INVALID_MODE]",
            "--weld-vertices",
            "--weld-degenerate-faces",
            "explode",
            LEVEL_FTS,
            out_glb,
        )
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-nav-surface", "--nav-radius", "4", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-navigation", "--nav-radius", "4", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-nav-surface", "--nav-height", "-1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-nav-surface", "--nav-clearance", "-1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--prune-nav-surface-islands", "--nav-prune-ratio", "1.1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--prune-nav-surface-islands", "--nav-prune-min-area", "-1", LEVEL_FTS, out_glb)
        expect_code(
            cli,
            1,
            "[CLI_INVALID_MODULE_VALUE]",
            "--gen-nav-surface",
            "--nav-max-slope",
            "91",
            LEVEL_FTS,
            out_glb,
        )
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-room-distances", "--rdist-spacing", "0", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-room-distances", "--rdist-spacing", "19", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-room-distances", "--rdist-offset", "0", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-room-distances", "--rdist-offset", "51", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-room-distances", "--rdist-height", "0", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-room-distances", "--rdist-height", "49", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-room-distances", "--rdist-link-distance", "0", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-room-distances", "--rdist-spacing", "100", "--rdist-link-distance", "109", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--anchor-radius", "50", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--anchor-link-distance", "150", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--anchor-link-radius-scale", "0.9", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--light-ambient", "0.25", "0.25", "0.25", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--light-global-factor", "0.85", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--light-no-normals", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--light-no-shadows", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--minimap-halo-radius", "5", LEVEL_FTS, out_glb)
        expect_code(
            cli,
            1,
            "[CLI_MISSING_DEPENDENCY]",
            "--minimap-fg-color",
            "0.18",
            "0.34",
            "0.80",
            LEVEL_FTS,
            out_glb,
        )
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-anchors", "--anchor-spacing", "9", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-anchors", "--anchor-radius", "4", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-anchors", "--anchor-height", "-1", LEVEL_FTS, out_glb)
        expect_code(
            cli,
            1,
            "[CLI_INVALID_MODULE_VALUE]",
            "--connect-anchors",
            "--anchor-link-distance",
            "0",
            LEVEL_FTS,
            out_glb,
        )
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--connect-anchors", "--anchor-link-radius-scale", "0.49", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--connect-anchors", "--anchor-link-radius-scale", "1.01", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--prune-anchor-islands", "--anchor-prune-ratio", "1.1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_NUMBER]", "--prune-anchor-islands", "--anchor-prune-min-count", "-1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-static-lighting", "--light-ambient", "-0.1", "0.25", "0.25", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--gen-static-lighting", "--light-global-factor", "-1", LEVEL_FTS, out_glb)
        expect_code(
            cli,
            1,
            "[CLI_INVALID_MODULE_VALUE]",
            "--minimap-border-color",
            "1",
            "-0.1",
            "1",
            LEVEL_FTS,
            out_glb,
        )
        expect_code(
            cli,
            1,
            "[CLI_INVALID_MODULE_VALUE]",
            "--gen-minimap",
            "--minimap-lava-color",
            "0.25",
            "1.1",
            "0.90",
            LEVEL_FTS,
            out_glb,
        )
        expect_code(
            cli,
            1,
            "[CLI_RESOURCE_NOT_FOUND]",
            "--gen-minimap",
            "--minimap-water-image",
            tmp / "missing-minimap-water.bmp",
            LEVEL_FTS,
            out_glb,
        )
        minimap_sampler = tmp / "minimap-sampler.bmp"
        minimap_sampler.write_bytes(make_bmp_bytes())
        generated_minimap = tmp / "generated-minimap.glb"
        expect_success_contains(
            cli,
            "Level minimap generated:",
            "--gen-minimap",
            "--minimap-fg-image",
            minimap_sampler,
            "--minimap-fg-color",
            "1",
            "1",
            "1",
            LEVEL_GLB,
            generated_minimap,
        )
        if b"arx_minimap__map" not in generated_minimap.read_bytes():
            raise AssertionError("generated Level GLB must contain the minimap root")
        expect_success(cli, "--scale", "2", ANIMATION_TEA, tmp / "scaled-animation.json")
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--scale", "0", MODEL_FTL, out_json)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--scale", "nan", MODEL_FTL, out_json)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--rotate", "inf", "0", "0", MODEL_FTL, out_json)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--offset", "0", "nan", "0", MODEL_FTL, out_json)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--gen-room-distances", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--weld-vertices", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--flatten-portals", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--snap-to-portals", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--gen-anchors", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--connect-anchors", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--prune-nav-surface-islands", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--prune-anchor-islands", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--gen-static-lighting", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--gen-minimap", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_MODULE_OUTPUT_MISMATCH]", "--pretty", ANIMATION_TEA, tmp / "out.tea")
        expect_code(cli, 1, "[CLI_MODULE_OUTPUT_MISMATCH]", "--no-compression", ANIMATION_TEA, tmp / "out.tea")
        expect_code(
            cli,
            1,
            "[CLI_MODULE_OUTPUT_MISMATCH]",
            "--allow-empty-animation",
            MODEL_FTL,
            empty_animation_tea,
            tmp / "empty-animation-model.glb",
        )

        empty_default_dir = tmp / "empty-animation-default"
        empty_default_model = empty_default_dir / "model.ftl"
        empty_default = run(cli, MODEL_FTL, empty_animation_tea, empty_default_model)
        empty_default_text = empty_default.stdout + empty_default.stderr
        if empty_default.returncode != 0 or not empty_default_model.is_file():
            raise AssertionError(f"empty Model Animation omission failed\n{empty_default_text}")
        if (
            "Empty Animation 'empty_contract' omitted" not in empty_default_text
            or "empty_contract.tea" not in empty_default_text
        ):
            raise AssertionError(f"empty Animation omission did not identify its planned output\n{empty_default_text}")
        if (empty_default_dir / "empty_contract.tea").exists():
            raise AssertionError("empty Model Animation sidecar must be omitted by default")

        empty_collision_dir = tmp / "empty-animation-collision"
        empty_collision_model = empty_collision_dir / "model.ftl"
        empty_collision = run(
            cli, MODEL_FTL, empty_animation_tea, active_collision_tea, empty_collision_model
        )
        empty_collision_text = empty_collision.stdout + empty_collision.stderr
        if empty_collision.returncode != 0:
            raise AssertionError(f"empty Animation collision planning failed\n{empty_collision_text}")
        if not (empty_collision_dir / "empty_contract.tea").is_file():
            raise AssertionError("emitted Animation must retain the natural output name")
        if (empty_collision_dir / "empty_contract2.tea").exists():
            raise AssertionError("omitted Animation must not reserve an output name")
        if "empty_contract2.tea" not in empty_collision_text:
            raise AssertionError("omitted Animation warning must report its post-output candidate path")

        empty_intermediate_default_dir = tmp / "empty-animation-intermediate-default"
        empty_intermediate_default_model = empty_intermediate_default_dir / "model.ftl"
        expect_success(cli, "--scale", "1", MODEL_FTL, empty_animation_tea, empty_intermediate_default_model)
        if (empty_intermediate_default_dir / "empty_contract.tea").exists():
            raise AssertionError("empty intermediate Model Animation sidecar must be omitted by default")

        empty_intermediate_dir = tmp / "empty-animation-intermediate"
        empty_intermediate_model = empty_intermediate_dir / "model.ftl"
        expect_success(
            cli,
            "--scale",
            "1",
            "--allow-empty-animation",
            MODEL_FTL,
            empty_animation_tea,
            empty_intermediate_model,
        )
        if not (empty_intermediate_dir / "empty_contract.tea").is_file():
            raise AssertionError("--allow-empty-animation must write an empty intermediate Model sidecar")

        empty_json_default_dir = tmp / "empty-animation-json-default"
        empty_json_default_model = empty_json_default_dir / "model.json"
        expect_success(cli, MODEL_FTL, empty_animation_tea, empty_json_default_model)
        if (empty_json_default_dir / "empty_contract.json").exists():
            raise AssertionError("empty Model Animation JSON sidecar must be omitted by default")

        empty_json_allowed_dir = tmp / "empty-animation-json-allowed"
        empty_json_allowed_model = empty_json_allowed_dir / "model.json"
        expect_success(
            cli,
            "--scale",
            "1",
            "--allow-empty-animation",
            MODEL_FTL,
            empty_animation_tea,
            empty_json_allowed_model,
        )
        if not (empty_json_allowed_dir / "empty_contract.json").is_file():
            raise AssertionError("--allow-empty-animation must write an empty intermediate JSON sidecar")

        for name, animation in zero_group_nonempty_animations:
            native_dir = tmp / f"zero-group-{name}-native"
            native_model = native_dir / "model.ftl"
            expect_success(cli, "--skip-sound-export", MODEL_FTL, animation, native_model)
            if not (native_dir / f"{name}.tea").is_file():
                raise AssertionError(f"zero-group Animation with {name} must survive direct native output")

            intermediate_dir = tmp / f"zero-group-{name}-intermediate"
            intermediate_model = intermediate_dir / "model.json"
            expect_success(cli, "--scale", "1", "--skip-sound-export", MODEL_FTL, animation, intermediate_model)
            if not (intermediate_dir / f"{name}.json").is_file():
                raise AssertionError(f"zero-group Animation with {name} must survive intermediate JSON output")
        expect_code(cli, 1, "[CLI_MODULE_OUTPUT_MISMATCH]", "--no-compression", MODEL_FTL, out_glb)
        expect_code(
            cli,
            1,
            "[CLI_MODULE_FORMAT_MISMATCH]",
            "--glb-arx-units-per-unit",
            "25",
            MODEL_FTL,
            out_json,
        )
        expect_success(
            cli,
            "--dry-run",
            "--glb-arx-units-per-unit",
            "25",
            "--glb-offset",
            "10",
            "20",
            "30",
            MODEL_FTL,
            out_glb,
        )
        expect_code(cli, 1, "[CLI_MODULE_OUTPUT_MISMATCH]", "--debug-cells", LEVEL_FTS, tmp / "debug.fts")
        expect_code(cli, 1, "[CLI_MODULE_OUTPUT_MISMATCH]", "--debug-room-distances", LEVEL_FTS, tmp / "debug.fts")
        expect_code(
            cli,
            1,
            "[CLI_MODULE_OUTPUT_MISMATCH]",
            "--gen-anchors",
            "--connect-anchors",
            "--debug-navigation",
            LEVEL_FTS,
            tmp / "debug.fts",
        )
        expect_success(
            cli,
            "--gen-navigation",
            "--debug-navigation",
            LEVEL_NAVIGATION_FTS,
            tmp / "preset-navigation.glb",
        )
        if LEVEL_FTS_AVAILABLE:
            expect_success(
                cli,
                "--gen-nav-surface",
                "--nav-from-floor",
                "--prune-nav-surface-islands",
                "--nav-prune-ratio",
                "0",
                "--nav-prune-min-area",
                "0",
                "--debug-navigation",
                LEVEL_FTS,
                out_glb,
            )
        if LEVEL_FTS_AVAILABLE:
            expect_success(
                cli,
                "--overwrite",
                "--rebase-textures",
                "graph",
                LEVEL_FTS,
                tmp / "rebased.glb",
            )
        if LEVEL_FTS_AVAILABLE:
            expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--rotate", "0", "0", "0", LEVEL_FTS, out_glb)
            expect_code(
                cli,
                1,
                "[CLI_MODULE_FORMAT_MISMATCH]",
                "--glb-offset",
                "0",
                "0",
                "0",
                LEVEL_FTS,
                tmp / "out.fts",
            )
        if LEVEL_BUNDLE_AVAILABLE:
            expect_code(
                cli,
                1,
                "[CLI_ROUTE_INVOCATION_INVALID]",
                "--kind",
                "level",
                LEVEL_BUNDLE_FTS,
                LEVEL_LLF,
                LEVEL_LLF,
                out_glb,
            )
            expect_success(cli, "--debug-cells", LEVEL_BUNDLE_FTS, LEVEL_LLF, tmp / "debug-with-llf.glb")
        if LEVEL_FTS_AVAILABLE:
            level_glb = tmp / "level-source.glb"
            expect_success(cli, LEVEL_FTS, level_glb)
            expect_success(cli, "--debug-cells", level_glb, tmp / "debug-from-level.glb")

            passthrough_mount = tmp / "passthrough-level-mount"
            expect_success(
                cli,
                "--write-mount",
                passthrough_mount,
                "--no-compression",
                "--skip-texture-export",
                LEVEL_FTS,
                "passthrough-level.dlf",
            )
            passthrough_dlf = passthrough_mount / "passthrough-level.dlf"
            passthrough_llf = passthrough_mount / "passthrough-level.llf"
            passthrough_fts = passthrough_mount / "game" / "graph" / "levels" / "passthrough-level" / "fast.fts"
            if not passthrough_fts.exists() or passthrough_dlf.exists() or passthrough_llf.exists():
                raise AssertionError("direct native output must preserve the supplied FTS-only carrier set")

            direct_mount = tmp / "direct-level-mount"
            expect_success(
                cli,
                "--write-mount",
                direct_mount,
                "--no-compression",
                "--skip-texture-export",
                level_glb,
                "direct-level.dlf",
            )
            direct_dlf = direct_mount / "direct-level.dlf"
            direct_llf = direct_mount / "direct-level.llf"
            direct_fts = direct_mount / "game" / "graph" / "levels" / "direct-level" / "fast.fts"
            if not direct_dlf.exists() or not direct_llf.exists() or not direct_fts.exists():
                raise AssertionError("direct DLF output must write a complete game-resource bundle")

            expect_success(cli, "--mount", direct_mount, "direct-level.dlf", tmp / "direct-level.glb")
            expect_code(
                cli,
                1,
                "[CLI_ROUTE_INVOCATION_INVALID]",
                "--mount",
                direct_mount,
                "direct-level.dlf",
                "direct-level.llf",
                tmp / "invalid-direct-level.glb",
            )
            expect_code(cli, 1, "[CLI_LEVEL_INPUT_FAILED]", direct_dlf, tmp / "absolute-dlf-input.glb")
            expect_code(cli, 1, "[CLI_LEVEL_OUTPUT_FAILED]", LEVEL_FTS, tmp / "absolute-dlf-output.dlf")

            dlf_only_fts = tmp / "dlf-only-level.fts"
            dlf_only_dlf = dlf_only_fts.with_suffix(".dlf")
            expect_success(
                cli,
                "--dlf-only",
                "--no-quad-reconstruction",
                "--skip-texture-export",
                "--rebase-textures",
                "graph/obj3d/textures",
                "--no-compression",
                LEVEL_FTS,
                dlf_only_fts,
            )
            if dlf_only_fts.exists() or not dlf_only_dlf.exists():
                raise AssertionError("--dlf-only must write only the sibling DLF output")

            custom_scene_fts = tmp / "custom-scene.fts"
            custom_scene = run(
                cli,
                "--overwrite",
                "--no-compression",
                "--skip-texture-export",
                "--dlf-scene-directory",
                "custom-scenes/new-scene",
                LEVEL_FTS,
                custom_scene_fts,
            )
            if custom_scene.returncode != 0:
                raise AssertionError(f"custom FTS scene directory failed\n{custom_scene.stdout}{custom_scene.stderr}")
            custom_scene_dlf = custom_scene_fts.with_suffix(".dlf")
            if b"custom-scenes/new-scene\0" not in custom_scene_dlf.read_bytes():
                raise AssertionError("custom FTS scene directory was not written into DLF")
            if "loose Level output uses physical FTS" not in custom_scene.stderr:
                raise AssertionError("loose native output must report its detached runtime FTS reference")

            custom_scene_mount = tmp / "custom-scene-mount"
            expect_success(
                cli,
                "--write-mount",
                custom_scene_mount,
                "--no-compression",
                "--skip-texture-export",
                "--dlf-scene-directory",
                "graph/my-level-folder/placed-scene/",
                LEVEL_FTS,
                "placed.dlf",
            )
            if not (custom_scene_mount / "game" / "graph" / "my-level-folder" / "placed-scene" / "fast.fts").exists():
                raise AssertionError("DLF-pattern output must place FTS at the configured runtime resource path")

            expect_code(
                cli,
                1,
                "[CLI_RESOURCE_PATH_INVALID]",
                "--dlf-scene-directory",
                "../invalid-scene",
                LEVEL_FTS,
                tmp / "invalid-scene.fts",
            )

            dlf_selector_mount = tmp / "dlf-selector-mount"
            expect_success(cli, "--write-mount", dlf_selector_mount, "--dlf-only", LEVEL_FTS, "level:24")
            selector_dlf = dlf_selector_mount / "graph" / "levels" / "level24" / "level24.dlf"
            selector_llf = dlf_selector_mount / "graph" / "levels" / "level24" / "level24.llf"
            selector_fts = dlf_selector_mount / "game" / "graph" / "levels" / "level24" / "fast.fts"
            if not selector_dlf.exists() or selector_llf.exists() or selector_fts.exists():
                raise AssertionError("--dlf-only Level selector output must write only the canonical DLF resource")
        expect_code(cli, 1, "[CLI_LEVEL_INPUT_FAILED]", bad_fts, tmp / "bad.glb")
        expect_success(cli, "--kind", "model", glb_as_fts, tmp / "recognized-model.json")
        if LEVEL_FTS_AVAILABLE:
            expect_code(cli, 1, "[CLI_LEVEL_INPUT_FAILED]", LEVEL_FTS, bad_llf, tmp / "bad-extra.glb")
            expect_code(cli, 1, "[CLI_CLASSIFICATION_FAILED]", LEVEL_FTS, bad_dlf, tmp / "bad-extra-2.glb")
        expect_code(cli, 1, "[CLI_LEVEL_INPUT_FAILED]", bad_recognized_fts, tmp / "bad-recognized.glb")
        if LEVEL_FTS_AVAILABLE:
            expect_code(cli, 1, "[CLI_LEVEL_INPUT_FAILED]", LEVEL_FTS, bad_recognized_llf, tmp / "bad-recognized-extra.glb")
            expect_code(
                cli,
                1,
                "[CLI_LEVEL_INPUT_FAILED]",
                LEVEL_FTS,
                bad_recognized_dlf,
                tmp / "bad-recognized-extra-2.glb",
            )

        if LEVEL_BUNDLE_AVAILABLE:
            normalized_fts = tmp / "normalized.fts"
            expect_success(
                cli,
                "--no-compression",
                "--rebase-textures",
                r"Graph\Obj3D\Textures",
                LEVEL_BUNDLE_FTS,
                LEVEL_LLF,
                LEVEL_DLF,
                normalized_fts,
            )
            if b"graph/obj3d/textures/" not in normalized_fts.read_bytes():
                raise AssertionError("expected normalized texture folder in FTS output")

            negative_folder_fts = tmp / "negative-folder.fts"
            expect_success(
                cli,
                "--no-compression",
                "--rebase-textures",
                "-foo",
                LEVEL_BUNDLE_FTS,
                LEVEL_LLF,
                LEVEL_DLF,
                negative_folder_fts,
            )
            if b"-foo/" not in negative_folder_fts.read_bytes():
                raise AssertionError("expected option-like texture folder argument in FTS output")

        compressed_ftl = tmp / "compressed.ftl"
        expect_success(cli, MODEL_FTL, compressed_ftl)
        if compressed_ftl.read_bytes()[:2] != b"\x00\x06":
            raise AssertionError("expected FTL output to use PKWARE DCL compression by default")
        expect_success(cli, compressed_ftl, tmp / "compressed-ftl.json")

        raw_ftl = tmp / "raw.ftl"
        expect_success(cli, "--no-compression", MODEL_FTL, raw_ftl)
        if raw_ftl.read_bytes()[:4] != b"FTL\x00":
            raise AssertionError("expected --no-compression FTL output to retain the raw magic")

        numeric_input = tmp / "123.ftl"
        shutil.copyfile(MODEL_FTL, numeric_input)
        numeric_scale_output = tmp / "numeric-scale.json"
        numeric_scale = run(cli, "--scale", "2", numeric_input.name, numeric_scale_output.name, cwd=tmp)
        if numeric_scale.returncode != 0 or not numeric_scale_output.is_file():
            raise AssertionError(
                "--scale must consume exactly one value and leave a numeric input positional intact\n"
                f"{numeric_scale.stdout}{numeric_scale.stderr}"
            )

        unicode_output_directory = tmp / "unicode-\u043f\u0443\u0442\u044c"
        unicode_output_directory.mkdir()
        unicode_output = unicode_output_directory / "\u043c\u043e\u0434\u0435\u043b\u044c.json"
        expect_success(cli, MODEL_FTL, unicode_output)
        if not unicode_output.is_file():
            raise AssertionError("UTF-8 raw output path was not written")

        listing_high = tmp / "listing-high"
        listing_low = tmp / "listing-low"
        listing_files = (
            listing_high / "graph/levels/level2/level2.dlf",
            listing_low / "graph/levels/level2/level2.dlf",
            listing_low / "graph/levels/level3/level3.dlf",
            listing_high / "graph/levels/level10/level10.dlf",
            listing_high / "game/graph/obj3d/interactive/npc/hero/hero.ftl",
            listing_low / "game/graph/obj3d/interactive/npc/guard2/guard2.ftl",
            listing_low / "game/graph/obj3d/interactive/npc/guard10/guard10.ftl",
            listing_low / "game/graph/obj3d/interactive/npc/human/tweaks/red.ftl",
            listing_low / "game/graph/obj3d/interactive/items/armor/chest/chest.ftl",
            listing_high / "game/graph/interface/book/runes/aam.ftl",
            listing_low / "game/graph/interface/menus/main.ftl",
            listing_low / "game/editor/obj3d/light.ftl",
            listing_high / "graph/obj3d/anims/npc/walk.tea",
            listing_low / "graph/obj3d/anims/fix_inter/open.tea",
            listing_high / "graph/interface/illustrations/intro.cin",
            listing_low / "sfx/ambiance/cave/water.amb",
        )
        for resource in listing_files:
            resource.parent.mkdir(parents=True, exist_ok=True)
            resource.write_bytes(b"")

        listing = run(
            cli,
            "--mount",
            listing_high,
            "--list-resources",
            "all",
            "--mount",
            listing_low,
        )
        expected_listing = [
            "ambiance:cave/water",
            "anim:fix_inter:open",
            "anim:npc:walk",
            "cinematic:intro",
            "level:2",
            "level:3",
            "level:10",
            "model:armor:chest",
            "model:editor:light",
            "model:npc:guard2",
            "model:npc:guard10",
            "model:npc:hero",
            "model:npc:human:red",
            "model:ui-menus:main",
            "model:ui-runes:aam",
        ]
        if listing.returncode != 0 or listing.stdout.splitlines() != expected_listing:
            raise AssertionError(
                "resource listing must merge mounts by priority and emit naturally sorted exact selectors\n"
                f"{listing.stdout}{listing.stderr}"
            )

        expect_success_contains(
            cli,
            "[INFO/CLI] duplicate mount ignored",
            "--mount",
            listing_high,
            "--mount",
            f"{listing_high}/",
            "--list-resources",
            "all",
        )

        appended_listing = run(
            cli,
            "missing-level.fts",
            "ignored-output.glb",
            "--gen-anchors",
            "--list-resources",
            "model",
            "--mount",
            listing_high,
            "--mount",
            listing_low,
        )
        expected_models = [resource for resource in expected_listing if resource.startswith("model:")]
        if appended_listing.returncode != 0 or appended_listing.stdout.splitlines() != expected_models:
            raise AssertionError(
                "resource listing appended to a conversion command must bypass route resolution\n"
                f"{appended_listing.stdout}{appended_listing.stderr}"
            )

        model_listing = run(
            cli,
            "--mount",
            listing_high,
            "--list-resources",
            "mod",
            "--mount",
            listing_low,
        )
        if model_listing.returncode != 0 or model_listing.stdout.splitlines() != expected_models:
            raise AssertionError(
                "resource listing kinds must accept unambiguous prefixes\n"
                f"{model_listing.stdout}{model_listing.stderr}"
            )

        ambiguous_listing = run(cli, "--list-resources", "a")
        if (
            ambiguous_listing.returncode != 1
            or "[CLI_INVALID_MODE]" not in ambiguous_listing.stderr
            or "ambiguous resource kind 'a'" not in ambiguous_listing.stderr
        ):
            raise AssertionError(
                "ambiguous resource listing prefixes must be rejected\n"
                f"{ambiguous_listing.stdout}{ambiguous_listing.stderr}"
            )

        empty_listing_mount = tmp / "empty-listing-mount"
        empty_listing_mount.mkdir()
        empty_listing = run(cli, "--mount", empty_listing_mount, "--list-resources", "all")
        if empty_listing.returncode != 0 or empty_listing.stdout:
            raise AssertionError(
                "missing resource roots must produce a successful empty listing\n"
                f"{empty_listing.stdout}{empty_listing.stderr}"
            )

        first_mount = tmp / "first-mount"
        second_mount = tmp / "second-mount"
        first_mount.mkdir()
        second_mount.mkdir()
        expect_success(
            cli,
            "--mount",
            first_mount,
            "--mou",
            second_mount,
            MODEL_FTL,
            tmp / "mounted-model.json",
        )
        relative_output_mount = tmp / "relative-output-mount"
        relative_output_mount.mkdir()
        relative_output = run(
            cli,
            "--write-mount",
            relative_output_mount,
            MODEL_FTL,
            "nested/mounted-model.json",
            cwd=tmp,
        )
        if relative_output.returncode != 0:
            raise AssertionError(
                f"mount-relative raw output failed\n{relative_output.stdout}{relative_output.stderr}"
            )
        if not (relative_output_mount / "nested" / "mounted-model.json").is_file():
            raise AssertionError("relative raw output must be written through the write mount")
        if (tmp / "nested" / "mounted-model.json").exists():
            raise AssertionError("relative raw output must not bypass the write mount")

        read_only_output = run(
            cli,
            "--mount",
            relative_output_mount,
            MODEL_FTL,
            "nested/default-write-model.json",
            cwd=tmp,
        )
        if read_only_output.returncode != 0:
            raise AssertionError(f"read-only mount conversion failed\n{read_only_output.stdout}{read_only_output.stderr}")
        if not (tmp / "nested" / "default-write-model.json").is_file():
            raise AssertionError("read mounts must not change the default write root")
        if (relative_output_mount / "nested" / "default-write-model.json").exists():
            raise AssertionError("read mounts must never receive outputs implicitly")
        missing_mount = tmp / "missing-mount"
        expect_success_contains(
            cli,
            "[INFO/CLI] write mount not found; will create on actual write",
            "--write-mount",
            missing_mount,
            MODEL_FTL,
            tmp / "invalid-mount-model.json",
        )
        if missing_mount.exists():
            raise AssertionError("a prospective mount must not be created for a raw-path output")

        missing_read_mount = tmp / "missing-read-mount"
        expect_success_contains(
            cli,
            "[WARN/CLI] mount not found; excluding from reads",
            "--mount",
            first_mount,
            "--mount",
            missing_read_mount,
            MODEL_FTL,
            tmp / "missing-read-mount.json",
        )

        auto_workspace = tmp / "auto-mount-workspace"
        auto_workspace.mkdir()
        current_only_input = Path(f"current-only-{tmp.name}") / "auto-input.ftl"
        (auto_workspace / current_only_input).parent.mkdir()
        shutil.copyfile(MODEL_FTL, auto_workspace / current_only_input)
        auto_environment = None
        if sys.platform != "win32":
            auto_environment = {
                "XDG_DATA_HOME": str(tmp / "auto-xdg"),
                "HOME": str(tmp / "auto-home"),
            }
        auto_with_implicit_current = run(
            cli,
            "--auto-mount",
            current_only_input.as_posix(),
            "auto-output.json",
            cwd=auto_workspace,
            env=auto_environment,
        )
        if auto_with_implicit_current.returncode != 0 or not (auto_workspace / "auto-output.json").is_file():
            raise AssertionError(
                "--auto-mount must retain the implicit current-directory read mount\n"
                f"{auto_with_implicit_current.stdout}{auto_with_implicit_current.stderr}"
            )
        auto_without_current = run(
            cli,
            "--auto-mount",
            "--mount",
            first_mount,
            current_only_input.as_posix(),
            "explicit-auto-output.json",
            cwd=auto_workspace,
            env=auto_environment,
        )
        if auto_without_current.returncode != 1 or "[CLI_RESOURCE_NOT_FOUND]" not in auto_without_current.stderr:
            raise AssertionError(
                "an explicit mount must suppress the implicit current-directory read mount\n"
                f"{auto_without_current.stdout}{auto_without_current.stderr}"
            )

        automatic_write = run(
            cli,
            "--auto-mount",
            "--dry-run",
            MODEL_FTL,
            "model:npc:auto_mount_default",
            env=auto_environment,
        )
        expected_suffix = Path(
            "game/graph/obj3d/interactive/npc/auto_mount_default/auto_mount_default.ftl"
        )
        if automatic_write.returncode == 0:
            expected_parts = tuple(part.lower() for part in expected_suffix.parts)
            dry_run_destinations = tuple(
                Path(line.split(" to: ", 1)[1])
                for line in automatic_write.stderr.splitlines()
                if "[INFO/CLI] dry-run: would write" in line and " to: " in line
            )
            if not any(
                destination.is_absolute()
                and tuple(part.lower() for part in destination.parts[-len(expected_parts) :]) == expected_parts
                for destination in dry_run_destinations
            ):
                raise AssertionError(
                    "--auto-mount must resolve game-layout output under an absolute default game root\n"
                    f"{automatic_write.stdout}{automatic_write.stderr}"
                )
        elif (
            automatic_write.returncode != 1
            or "[CLI_DEFAULT_GAME_ROOT_UNAVAILABLE]" not in automatic_write.stderr
        ):
            raise AssertionError(
                "automatic game write mount must resolve or report unavailable root\n"
                f"{automatic_write.stdout}{automatic_write.stderr}"
            )

        if sys.platform != "win32":
            unavailable_write = run(
                cli,
                "--auto-mount",
                "--dry-run",
                MODEL_FTL,
                "model:npc:auto_mount_unavailable",
                env={"XDG_DATA_HOME": "relative-data", "HOME": "relative-home"},
            )
            if (
                unavailable_write.returncode != 1
                or "[CLI_DEFAULT_GAME_ROOT_UNAVAILABLE]" not in unavailable_write.stderr
            ):
                raise AssertionError(
                    "unavailable automatic game root must report its dedicated diagnostic\n"
                    f"{unavailable_write.stdout}{unavailable_write.stderr}"
                )
        for index, arguments in enumerate(
            (
                ("--auto-mount", "--write-mount"),
                ("--write-mount", "--auto-mount"),
            )
        ):
            explicit_auto_write = tmp / f"auto-write-{index}"
            if arguments[0] == "--auto-mount":
                command = (arguments[0], arguments[1], explicit_auto_write)
            else:
                command = (arguments[0], explicit_auto_write, arguments[1])
            expect_success(cli, *command, MODEL_FTL, f"model:npc:auto_write_{index}")
            expected = (
                explicit_auto_write
                / "game"
                / "graph"
                / "obj3d"
                / "interactive"
                / "npc"
                / f"auto_write_{index}"
                / f"auto_write_{index}.ftl"
            )
            if not expected.is_file():
                raise AssertionError(f"explicit --write-mount did not override --auto-mount: {expected}")

        prospective_mount = tmp / "prospective-mount"
        expect_success(cli, "--write-mount", prospective_mount, MODEL_FTL, "model:npc:prospective")
        prospective_model = (
            prospective_mount
            / "game"
            / "graph"
            / "obj3d"
            / "interactive"
            / "npc"
            / "prospective"
            / "prospective.ftl"
        )
        if not prospective_model.is_file():
            raise AssertionError(f"expected prospective mount output at {prospective_model}")

        dry_mount = tmp / "dry-prospective-mount"
        expect_success_contains(
            cli,
            "[INFO/CLI] dry-run: would write",
            "--dry-run",
            "--write-mount",
            dry_mount,
            MODEL_FTL,
            "model:npc:dry_mount",
        )
        if dry_mount.exists():
            raise AssertionError("dry-run must not create a prospective mount")

        mount_file = tmp / "mount-file"
        mount_file.write_text("not a directory", encoding="utf-8")
        expect_code(
            cli,
            1,
            "[CLI_IO_STAT_FAILED]",
            "--dry-run",
            "--mount",
            mount_file / "child",
            MODEL_FTL,
            tmp / "invalid-mount-output.json",
        )
        expect_code(
            cli,
            1,
            "[CLI_IO_STAT_FAILED]",
            "--dry-run",
            "--write-mount",
            mount_file / "child",
            MODEL_FTL,
            tmp / "invalid-write-mount-output.json",
        )
        if sys.platform == "win32":
            expect_code(
                cli,
                1,
                "[CLI_IO_STAT_FAILED]",
                "--dry-run",
                "--mount",
                tmp / "invalid*mount",
                MODEL_FTL,
                tmp / "invalid-native-mount.json",
            )
        expect_code(cli, 1, "[CLI_RESOURCE_SELECTOR_INVALID]", "--mount", first_mount, MODEL_FTL, "model:npc:NUL")
        if sys.platform == "win32":
            expect_code(cli, 1, "[CLI_IO_PATH_INVALID]", MODEL_FTL, tmp / "invalid*output.ftl")

        model_input_mount = tmp / "model-input-mount"
        mounted_model = model_input_mount / "game" / "graph" / "obj3d" / "interactive" / "npc" / "Adventurer" / "Adventurer.ftl"
        mounted_model.parent.mkdir(parents=True)
        shutil.copyfile(MODEL_FTL, mounted_model)
        expect_success(cli, "--mount", model_input_mount, "model:npc:Adventurer", tmp / "mounted-model-input.json")

        selector_model_mount = tmp / "selector-model-mount"
        selector_model = (
            selector_model_mount
            / "game"
            / "graph"
            / "obj3d"
            / "interactive"
            / "npc"
            / "contract"
            / "contract.ftl"
        )
        selector_animation = selector_model_mount / "graph" / "obj3d" / "anims" / "fix_inter" / "sound.tea"
        selector_model.parent.mkdir(parents=True)
        selector_animation.parent.mkdir(parents=True)
        shutil.copyfile(MODEL_FTL, selector_model)
        shutil.copyfile(referenced_sound_animation, selector_animation)

        selected_model_glb = tmp / "selected-model.glb"
        expect_success(
            cli,
            "--mount",
            selector_model_mount,
            "model:npc:contract",
            selected_model_glb,
        )
        if not any(path.startswith("textures/") for path in glb_image_paths(selected_model_glb)):
            raise AssertionError("Model selector input must use the local authoring texture directory")

        raw_model_glb = tmp / "raw-model.glb"
        expect_success(cli, selector_model, raw_model_glb)
        if any(path.startswith("textures/") for path in glb_image_paths(raw_model_glb)):
            raise AssertionError("raw Model input must preserve texture paths")

        selected_model_override_glb = tmp / "selected-model-override.glb"
        expect_success(
            cli,
            "--mount",
            selector_model_mount,
            "--rebase-textures",
            "custom/textures",
            "model:npc:contract",
            selected_model_override_glb,
        )
        if not any(path.startswith("custom/textures/") for path in glb_image_paths(selected_model_override_glb)):
            raise AssertionError("explicit texture rebase must override the Model input selector default")

        selected_sound_glb = tmp / "selected-sound.glb"
        expect_success(
            cli,
            "--mount",
            selector_model_mount,
            "--skip-sound-export",
            "model:npc:contract",
            "anim:fix_inter:sound",
            selected_sound_glb,
        )
        if b"sounds/contract.wav" not in selected_sound_glb.read_bytes():
            raise AssertionError("selected sound-bearing Animations must use the local authoring sound directory")
        (tmp / "contract.wav").write_bytes(make_wav_bytes())
        selected_sound_directory = tmp / "sounds"
        selected_sound_directory.mkdir()
        (selected_sound_directory / "contract.wav").write_bytes(make_wav_bytes())

        selected_loose_native = tmp / "selected-loose-native"
        selected_loose_native.mkdir()
        selected_loose_ftl = selected_loose_native / "model.ftl"
        expect_success(
            cli,
            "--mount",
            selector_model_mount,
            "--skip-texture-export",
            "--skip-sound-export",
            "--no-compression",
            "--scale",
            "1",
            "model:npc:contract",
            "anim:fix_inter:sound",
            selected_loose_ftl,
        )
        if b"textures/" not in selected_loose_ftl.read_bytes():
            raise AssertionError("Model selector input must rebase textures for any loose intermediate output")
        selected_loose_teas = list(selected_loose_native.glob("*.tea"))
        if len(selected_loose_teas) != 1 or b"sounds/contract" not in selected_loose_teas[0].read_bytes():
            raise AssertionError("Animation selector input must rebase sounds for loose Model output")

        same_game_mount = tmp / "same-game-rebase"
        expect_success(
            cli,
            "--mount",
            selector_model_mount,
            "--write-mount",
            same_game_mount,
            "--skip-texture-export",
            "--skip-sound-export",
            "--scale",
            "1",
            "model:npc:contract",
            "anim:fix_inter:sound",
            "model:npc:contract_copy",
        )
        same_game_ftl = (
            same_game_mount
            / "game"
            / "graph"
            / "obj3d"
            / "interactive"
            / "npc"
            / "contract_copy"
            / "contract_copy.ftl"
        )
        same_game_tea = same_game_mount / "graph" / "obj3d" / "anims" / "fix_inter" / "sound.tea"
        if b"graph/obj3d/textures/" in same_game_ftl.read_bytes():
            raise AssertionError("Game selector to Game selector must preserve Model texture paths")
        if b"sfx/contract.wav" in same_game_tea.read_bytes() or b"sounds/contract.wav" in same_game_tea.read_bytes():
            raise AssertionError("Game selector to Game selector must preserve Animation Sound paths")

        standalone_selected_json = tmp / "standalone-selected.json"
        expect_success(
            cli,
            "--mount",
            selector_model_mount,
            "--skip-sound-export",
            "--rotate",
            "0",
            "0",
            "0",
            "anim:fix_inter:sound",
            standalone_selected_json,
        )
        if "sounds/contract" not in standalone_selected_json.read_text(encoding="utf-8"):
            raise AssertionError("standalone Animation selector input must rebase sounds for loose output")

        raw_sound_glb = tmp / "raw-sound.glb"
        expect_success(
            cli,
            "--mount",
            selector_model_mount,
            "--skip-sound-export",
            "model:npc:contract",
            referenced_sound_animation,
            raw_sound_glb,
        )
        if b"sounds/contract.wav" in raw_sound_glb.read_bytes():
            raise AssertionError("raw sound-bearing Animations must preserve Sound paths")

        mixed_sound_glb = tmp / "mixed-sound.glb"
        expect_success_contains(
            cli,
            "mixed Animation sound sources do not share an automatic rebase; preserving Sound paths",
            "--mount",
            selector_model_mount,
            "--skip-sound-export",
            "model:npc:contract",
            "anim:fix_inter:sound",
            referenced_sound_animation,
            mixed_sound_glb,
        )
        if b"sounds/contract.wav" in mixed_sound_glb.read_bytes():
            raise AssertionError("mixed sound-bearing Animation sources must preserve all Sound paths")

        selected_with_soundless_raw_glb = tmp / "selected-with-soundless-raw.glb"
        expect_success(
            cli,
            "--mount",
            selector_model_mount,
            "--skip-sound-export",
            "model:npc:contract",
            "anim:fix_inter:sound",
            ANIMATION_TEA,
            selected_with_soundless_raw_glb,
        )
        if b"sounds/contract.wav" not in selected_with_soundless_raw_glb.read_bytes():
            raise AssertionError("soundless raw Animations must not suppress selected Sound rebasing")

        mixed_sound_override_glb = tmp / "mixed-sound-override.glb"
        expect_success(
            cli,
            "--mount",
            selector_model_mount,
            "--skip-sound-export",
            "--rebase-sounds",
            "custom/sounds",
            "model:npc:contract",
            "anim:fix_inter:sound",
            referenced_sound_animation,
            mixed_sound_override_glb,
        )
        if b"custom/sounds/contract.wav" not in mixed_sound_override_glb.read_bytes():
            raise AssertionError("explicit Sound rebase must override mixed Animation source preservation")

        ambiance_selector_mount = tmp / "ambiance-selector-mount"
        selected_ambiance = ambiance_selector_mount / "sfx" / "ambiance" / "contract.amb"
        selected_ambiance.parent.mkdir(parents=True)
        shutil.copyfile(ambiance_amb, selected_ambiance)
        selected_ambiance_glb = tmp / "selected-ambiance.glb"
        expect_success(
            cli,
            "--mount",
            ambiance_selector_mount,
            "--skip-sound-export",
            "ambiance:contract",
            selected_ambiance_glb,
        )
        if b"sounds/test.wav" not in selected_ambiance_glb.read_bytes():
            raise AssertionError("Ambiance selector input must use the local authoring sound directory")

        ambiance_game_mount = tmp / "ambiance-game-rebase"
        expect_success(
            cli,
            "--write-mount",
            ambiance_game_mount,
            "--skip-sound-export",
            selected_ambiance_glb,
            "ambiance:contract_copy",
        )
        ambiance_game = ambiance_game_mount / "sfx" / "ambiance" / "contract_copy.amb"
        if b"sfx/ambiance/test.wav" not in ambiance_game.read_bytes():
            raise AssertionError("loose Ambiance input must use the canonical selector Sound directory")

        raw_ambiance_glb = tmp / "raw-ambiance.glb"
        expect_success(cli, "--skip-sound-export", ambiance_amb, raw_ambiance_glb)
        if b"sounds/test.wav" in raw_ambiance_glb.read_bytes():
            raise AssertionError("raw Ambiance input must preserve Sound paths")

        obj_project = tmp / "obj-project"
        (obj_project / "materials").mkdir(parents=True)
        (obj_project / "images").mkdir()
        obj_input = obj_project / "source.obj"
        obj_input.write_text(
            "mtllib materials/library.mtl\n"
            "v 0 0 0\n"
            "v 1 0 0\n"
            "v 0 1 0\n"
            "usemtl wall\n"
            "f 1 2 3\n",
            encoding="utf-8",
        )
        (obj_project / "materials" / "library.mtl").write_text(
            "newmtl wall\nmap_Kd images/wall.bmp\n",
            encoding="utf-8",
        )
        image = make_bmp_bytes()
        (obj_project / "images" / "wall.bmp").write_bytes(image)
        obj_output = tmp / "obj-output" / "model.obj"
        expect_success(cli, obj_input, obj_output)
        if "mtllib model.mtl" not in obj_output.read_text(encoding="utf-8"):
            raise AssertionError("OBJ output must reference its generated MTL")
        if "map_Kd images/wall.bmp" not in obj_output.with_suffix(".mtl").read_text(encoding="utf-8"):
            raise AssertionError("OBJ output must preserve the Model texture path in map_Kd")
        if (obj_output.parent / "images" / "wall.bmp").read_bytes() != image:
            raise AssertionError("OBJ output must write texture sidecars relative to the OBJ")

        skipped_obj_output = tmp / "obj-output-skipped" / "model.obj"
        expect_success(cli, "--skip-texture-export", obj_input, skipped_obj_output)
        if (skipped_obj_output.parent / "images" / "wall.bmp").exists():
            raise AssertionError("--skip-texture-export must not write OBJ texture sidecars")

        tweak_model = (
            model_input_mount
            / "game"
            / "graph"
            / "obj3d"
            / "interactive"
            / "npc"
            / "human_base"
            / "tweaks"
            / "human_kultar.ftl"
        )
        tweak_model.parent.mkdir(parents=True)
        shutil.copyfile(MODEL_FTL, tweak_model)
        tweak_preview = tmp / "tweak-preview.glb"
        expect_success(
            cli,
            "--mount",
            model_input_mount,
            "model:npc:human_base:human_kultar",
            tweak_preview,
            "--as-level-preview",
        )
        if b"CLASS_model:npc:human_base__" not in tweak_preview.read_bytes():
            raise AssertionError("Model tweak preview must use the base Model class path")

        implicit_mount = tmp / "implicit-mount"
        implicit_model = implicit_mount / "game" / "graph" / "obj3d" / "interactive" / "npc" / "Implicit" / "Implicit.ftl"
        implicit_model.parent.mkdir(parents=True)
        shutil.copyfile(MODEL_FTL, implicit_model)
        proc = run(cli, "model:npc:Implicit", "model:npc:Written", cwd=implicit_mount)
        if proc.returncode != 0:
            raise AssertionError(f"implicit current-directory mount failed\n{proc.stdout}{proc.stderr}")
        implicit_output = implicit_mount / "game" / "graph" / "obj3d" / "interactive" / "npc" / "Written" / "Written.ftl"
        if not implicit_output.is_file():
            raise AssertionError(f"expected implicit mounted output at {implicit_output}")

        rebase_mount = tmp / "selector-rebase-mount"
        native_rebase_source = tmp / "native-rebase-source.ftl"
        expect_success(
            cli,
            "--no-compression",
            "--skip-texture-export",
            obj_input,
            native_rebase_source,
        )
        native_selector = "model:npc:native_texture_paths"
        expect_success(
            cli,
            "--write-mount",
            rebase_mount,
            "--overwrite",
            "--no-compression",
            "--skip-texture-export",
            native_rebase_source,
            native_selector,
        )
        native_selector_ftl = (
            rebase_mount
            / "game"
            / "graph"
            / "obj3d"
            / "interactive"
            / "npc"
            / "native_texture_paths"
            / "native_texture_paths.ftl"
        )
        if b"graph/obj3d/textures/" in native_selector_ftl.read_bytes():
            raise AssertionError("direct native Model selector output must preserve texture identities")

        intermediate_selector = "model:npc:intermediate_texture_paths"
        expect_success(
            cli,
            "--write-mount",
            rebase_mount,
            "--overwrite",
            "--no-compression",
            "--skip-texture-export",
            "--scale",
            "1",
            native_rebase_source,
            intermediate_selector,
        )
        intermediate_selector_ftl = (
            rebase_mount
            / "game"
            / "graph"
            / "obj3d"
            / "interactive"
            / "npc"
            / "intermediate_texture_paths"
            / "intermediate_texture_paths.ftl"
        )
        if b"graph/obj3d/textures/" not in intermediate_selector_ftl.read_bytes():
            raise AssertionError("intermediate Model selector output must use the canonical texture directory")

        model_sound_game_mount = tmp / "model-sound-game-rebase"
        expect_success(
            cli,
            "--write-mount",
            model_sound_game_mount,
            "--skip-texture-export",
            "--scale",
            "1",
            selected_sound_glb,
            "model:npc:sound_game_rebase",
        )
        model_sound_game_teas = list(model_sound_game_mount.rglob("*.tea"))
        if len(model_sound_game_teas) != 1 or not (model_sound_game_mount / "sfx" / "contract.wav").is_file():
            raise AssertionError("loose Model Animation input must use the canonical selector Sound directory")

        standalone_game_mount = tmp / "standalone-game-rebase"
        standalone_game = expect_success(
            cli,
            "--write-mount",
            standalone_game_mount,
            "--rotate",
            "0",
            "0",
            "0",
            referenced_sound_animation,
            "anim:fix_inter:standalone_game_rebase",
        )
        standalone_game_teas = list(standalone_game_mount.rglob("*.tea"))
        if len(standalone_game_teas) != 1 or not (standalone_game_mount / "sfx" / "contract.wav").is_file():
            written = sorted(
                str(path.relative_to(standalone_game_mount))
                for path in standalone_game_mount.rglob("*")
                if path.is_file()
            )
            raise AssertionError(
                "loose standalone Animation input must use the canonical selector Sound directory; "
                f"written files: {written}; source sound exists: {(tmp / 'contract.wav').is_file()}\n"
                f"{standalone_game.stdout}{standalone_game.stderr}"
            )

        standalone_same_game_mount = tmp / "standalone-same-game"
        expect_success(
            cli,
            "--mount",
            selector_model_mount,
            "--write-mount",
            standalone_same_game_mount,
            "--skip-sound-export",
            "--rotate",
            "0",
            "0",
            "0",
            "anim:fix_inter:sound",
            "anim:fix_inter:sound_copy",
        )
        standalone_same_game_teas = list(standalone_same_game_mount.rglob("*.tea"))
        if len(standalone_same_game_teas) != 1:
            raise AssertionError("standalone Game selector output did not write exactly one Animation")
        standalone_same_game_bytes = standalone_same_game_teas[0].read_bytes()
        if b"sfx/contract.wav" in standalone_same_game_bytes or b"sounds/contract.wav" in standalone_same_game_bytes:
            raise AssertionError("standalone Game selector to Game selector must preserve Sound paths")

        override_selector = "model:npc:override_texture_paths"
        expect_success(
            cli,
            "--write-mount",
            rebase_mount,
            "--overwrite",
            "--no-compression",
            "--skip-texture-export",
            "--rebase-textures",
            "custom/textures",
            MODEL_FTL,
            override_selector,
        )
        override_selector_ftl = (
            rebase_mount
            / "game"
            / "graph"
            / "obj3d"
            / "interactive"
            / "npc"
            / "override_texture_paths"
            / "override_texture_paths.ftl"
        )
        if b"custom/textures/" not in override_selector_ftl.read_bytes():
            raise AssertionError("explicit texture rebase must override the Model selector default")

        ordinary_output = tmp / "ordinary-texture-paths.ftl"
        expect_success(
            cli,
            "--overwrite",
            "--no-compression",
            "--skip-texture-export",
            "--scale",
            "1",
            native_rebase_source,
            ordinary_output,
        )
        if b"graph/obj3d/textures/" in ordinary_output.read_bytes():
            raise AssertionError("ordinary intermediate Model output must not rebase texture identities")

        level_rebase_mount = tmp / "level-selector-rebase-mount"
        expect_success(
            cli,
            "--write-mount",
            level_rebase_mount,
            "--overwrite",
            "--no-compression",
            "--skip-texture-export",
            LEVEL_GLB,
            "level:43",
        )
        level_selector_fts = level_rebase_mount / "game" / "graph" / "levels" / "level43" / "fast.fts"
        if b"graph/obj3d/textures/" not in level_selector_fts.read_bytes():
            raise AssertionError("intermediate Level selector output must use the canonical texture directory")

        explicit_empty_mount = tmp / "explicit-empty-mount"
        explicit_empty_mount.mkdir()
        proc = run(
            cli,
            "--mount",
            explicit_empty_mount,
            "model:npc:Implicit",
            tmp / "must-not-use-implicit.json",
            cwd=implicit_mount,
        )
        if proc.returncode != 1 or "[CLI_RESOURCE_NOT_FOUND]" not in proc.stdout + proc.stderr:
            raise AssertionError(f"explicit mount did not suppress implicit current directory\n{proc.stdout}{proc.stderr}")

        expect_success(
            cli,
            "--mount",
            first_mount,
            "--mount",
            second_mount,
            "--write-mount",
            first_mount,
            "--overwrite",
            MODEL_FTL,
            ANIMATION_TEA,
            "model:npc:written",
        )
        written_model = first_mount / "game" / "graph" / "obj3d" / "interactive" / "npc" / "written" / "written.ftl"
        if not written_model.is_file():
            raise AssertionError(f"expected mounted Model output at {written_model}")
        if (second_mount / "game" / "graph" / "obj3d" / "interactive" / "npc" / "written" / "written.ftl").exists():
            raise AssertionError("mounted Model output must not fall through to the second mount")
        written_animation_directory = first_mount / "graph" / "obj3d" / "anims" / "npc"
        written_animations = tuple(written_animation_directory.glob("*.tea"))
        if len(written_animations) != 1:
            raise AssertionError(f"expected one npc Model animation output in {written_animation_directory}")
        animation_output_stem = written_animations[0].stem

        collision_primary = tmp / f"{animation_output_stem}.json"
        expect_success(cli, "--overwrite", MODEL_FTL, ANIMATION_TEA, collision_primary)
        collision_sidecar = tmp / f"{animation_output_stem}2.json"
        if not collision_primary.is_file() or not collision_sidecar.is_file():
            raise AssertionError("model JSON and animation JSON targets must be disambiguated before writing")

        expect_success(
            cli,
            "--write-mount",
            first_mount,
            "--overwrite",
            MODEL_FTL,
            ANIMATION_TEA,
            "model:armor:written_item",
        )
        written_item = (
            first_mount
            / "game"
            / "graph"
            / "obj3d"
            / "interactive"
            / "items"
            / "armor"
            / "written_item"
            / "written_item.ftl"
        )
        if not written_item.is_file():
            raise AssertionError(f"expected mounted item Model output at {written_item}")
        fix_inter_animation = first_mount / "graph" / "obj3d" / "anims" / "fix_inter" / f"{animation_output_stem}.tea"
        if not fix_inter_animation.is_file():
            raise AssertionError(f"expected non-npc Model animation output at {fix_inter_animation}")

        animation_input_mount = tmp / "animation-input-mount"
        animation_family = animation_input_mount / "graph" / "obj3d" / "anims" / "npc"
        animation_family.mkdir(parents=True)
        shutil.copyfile(ANIMATION_TEA, animation_family / "idle.tea")
        shutil.copyfile(ANIMATION_TEA, animation_family / "idle2.tea")
        family_json = tmp / "family.json"
        expect_success(cli, "--mount", animation_input_mount, "anim:npc:idle", family_json)
        if not family_json.is_file():
            raise AssertionError("anim: selector must emit the selected standalone JSON output")
        if (tmp / "family2.json").exists():
            raise AssertionError("anim: selector must not discover a numbered animation family")

        failed_family_json = tmp / "failed-family.json"
        failed_family_json.mkdir()
        failed_family = run(
            cli,
            "--overwrite",
            "--mount",
            animation_input_mount,
            "anim:npc:idle",
            failed_family_json,
        )
        if failed_family.returncode != 1 or "[CLI_IO_STAT_FAILED]" not in failed_family.stderr:
            raise AssertionError(
                "a failed standalone Animation output must fail the command\n"
                f"{failed_family.stdout}{failed_family.stderr}"
            )

        expect_success(
            cli,
            "--mount",
            first_mount,
            "--mount",
            second_mount,
            "--write-mount",
            first_mount,
            "--overwrite",
            ANIMATION_TEA,
            "anim:fix_inter:provided",
        )
        named_animation = first_mount / "graph" / "obj3d" / "anims" / "fix_inter" / f"{animation_output_stem}.tea"
        if not named_animation.is_file():
            raise AssertionError(f"native TEA output must use the animation name: {named_animation}")

        if LEVEL_BUNDLE_AVAILABLE:
            level_input_mount = tmp / "level-input-mount"
            mounted_level = level_input_mount / "graph" / "levels" / LEVEL_BUNDLE_NAME
            mounted_level.mkdir(parents=True)
            mounted_game_level = level_input_mount / "game" / "graph" / "levels" / LEVEL_BUNDLE_NAME
            mounted_game_level.mkdir(parents=True)
            shutil.copyfile(LEVEL_DLF, mounted_level / f"{LEVEL_BUNDLE_NAME}.dlf")
            shutil.copyfile(LEVEL_LLF, mounted_level / f"{LEVEL_BUNDLE_NAME}.llf")
            shutil.copyfile(LEVEL_BUNDLE_FTS, mounted_game_level / "fast.fts")
            expect_success(
                cli,
                "--mount",
                level_input_mount,
                LEVEL_BUNDLE_SELECTOR,
                tmp / "mounted-level-input.glb",
            )
            if not any(
                path.startswith("textures/") for path in glb_image_paths(tmp / "mounted-level-input.glb")
            ):
                raise AssertionError("Level selector input must use the local authoring texture directory")

            expect_success(
                cli,
                "--mount",
                first_mount,
                "--mount",
                second_mount,
                "--write-mount",
                first_mount,
                "--overwrite",
                LEVEL_BUNDLE_FTS,
                LEVEL_LLF,
                LEVEL_DLF,
                "level:23",
            )
            expected_level_outputs = (
                first_mount / "graph" / "levels" / "level23" / "level23.dlf",
                first_mount / "graph" / "levels" / "level23" / "level23.llf",
                first_mount / "game" / "graph" / "levels" / "level23" / "fast.fts",
            )
            if not all(path.is_file() for path in expected_level_outputs):
                raise AssertionError(f"expected mounted Level bundle: {expected_level_outputs}")
            if (second_mount / "graph" / "levels" / "level23" / "level23.dlf").exists():
                raise AssertionError("mounted Level output must not fall through to the second mount")

        if LEVEL_FTS_AVAILABLE:
            unmounted_level_glb = tmp / "unmounted-level.glb"
            expect_success_contains(
                cli,
                "Level texture image not found:",
                LEVEL_FTS,
                unmounted_level_glb,
            )

            no_read_mount = tmp / "no-read-mount"
            expect_success_contains(
                cli,
                "Referenced Level texture image was not found because there are no readable mount folders:",
                "--mount",
                no_read_mount,
                LEVEL_FTS,
                tmp / "no-read-mount.glb",
            )

        log_result = run(cli, "--log-level", "In", MODEL_FTL, tmp / "log-info.json")
        if log_result.returncode != 0 or "[INFO/PISTORIS]" not in log_result.stderr or "\x1b" in log_result.stderr:
            raise AssertionError(f"captured logs must remain plain text\n{log_result.stdout}{log_result.stderr}")
        expect_success_not_contains(cli, "[INFO/PISTORIS]", "--log-level", "w", MODEL_FTL, tmp / "log-warn.json")

        dry_parent = tmp / "missing-parent"
        dry_out = dry_parent / "dry-run.ftl"
        expect_success_contains(cli, "[INFO/CLI] dry-run: would write", "--dry-run", MODEL_FTL, dry_out)
        if dry_parent.exists() or dry_out.exists():
            raise AssertionError("dry-run must not create output directories or files")

        existing_dry_out = tmp / "existing-dry-run.ftl"
        existing_dry_out.write_text("keep", encoding="utf-8")
        expect_success_contains(cli, "[INFO/CLI] dry-run: would write", "--dry-run", "--no-overwrite", MODEL_FTL, existing_dry_out)
        if existing_dry_out.read_text(encoding="utf-8") != "keep":
            raise AssertionError("dry-run must not overwrite an existing output")

        eof_prompt_out = tmp / "eof-prompt.ftl"
        eof_prompt_out.write_text("keep", encoding="utf-8")
        expect_success_contains(cli, "no overwrite response; skipping", MODEL_FTL, eof_prompt_out)
        if eof_prompt_out.read_text(encoding="utf-8") != "keep":
            raise AssertionError("EOF at the overwrite prompt must skip the existing output")

        expect_success(cli, "--ki", "ani", ANIMATION_TEA, out_json, "--pret", "--overwrite")
        expect_success(
            cli,
            "--kind",
            "ani",
            ANIMATION_TEA,
            tmp / "rotated.json",
            "--rotate",
            "-1",
            "0",
            "0",
            "--pretty",
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
