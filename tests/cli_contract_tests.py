#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

from __future__ import annotations

import json
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MODEL_GLB = ROOT / "data" / "fixtures" / "model" / "glb" / "Adventurer.glb"
MODEL_FTL = ROOT / "data" / "fixtures" / "model" / "native" / "Adventurer.ftl"
ANIMATION_TEA = ROOT / "data" / "fixtures" / "animation" / "native" / "Adventurer_Idle.tea"


def discover_arx_files(extension: str, root: Path = ROOT) -> tuple[Path, ...]:
    directory = root / "data" / "arx" / extension.removeprefix(".")
    files = (path for path in directory.glob(f"*{extension}") if path.is_file())
    return tuple(sorted(files, key=lambda path: path.name))


def discover_level_triplets(root: Path = ROOT) -> tuple[tuple[Path, Path, Path], ...]:
    by_stem = []
    for extension in (".fts", ".llf", ".dlf"):
        by_stem.append({path.stem: path for path in discover_arx_files(extension, root)})

    complete_stems = sorted(set(by_stem[0]) & set(by_stem[1]) & set(by_stem[2]))
    return tuple((by_stem[0][stem], by_stem[1][stem], by_stem[2][stem]) for stem in complete_stems)


LEVEL_FTS_FILES = discover_arx_files(".fts")
LEVEL_TRIPLETS = discover_level_triplets()
LEVEL_FTS_AVAILABLE = bool(LEVEL_FTS_FILES)
LEVEL_BUNDLE_AVAILABLE = bool(LEVEL_TRIPLETS)
LEVEL_FTS = LEVEL_FTS_FILES[0] if LEVEL_FTS_AVAILABLE else MODEL_GLB
LEVEL_BUNDLE_FTS, LEVEL_LLF, LEVEL_DLF = LEVEL_TRIPLETS[0] if LEVEL_BUNDLE_AVAILABLE else (MODEL_GLB,) * 3


def run(cli: Path, *args: str | Path, cwd: Path = ROOT) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(cli), *(str(arg) for arg in args)],
        cwd=cwd,
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


def expect_success(cli: Path, *args: str | Path) -> None:
    proc = run(cli, *args)
    text = proc.stdout + proc.stderr
    if proc.returncode != 0:
        raise AssertionError(f"{args}: expected success, got {proc.returncode}\n{text}")


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


def expect_success_order(cli: Path, expected_texts: tuple[str, ...], *args: str | Path) -> None:
    proc = run(cli, *args)
    text = proc.stdout + proc.stderr
    if proc.returncode != 0:
        raise AssertionError(f"{args}: expected success, got {proc.returncode}\n{text}")
    offset = -1
    for expected_text in expected_texts:
        found = text.find(expected_text, offset + 1)
        if found == -1:
            raise AssertionError(f"{args}: expected {expected_text!r} after offset {offset}\n{text}")
        offset = found


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


def main() -> int:
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
        glb_as_fts = tmp / "glb-as-fts.fts"
        glb_as_fts.write_bytes(MODEL_GLB.read_bytes())
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

        help_result = run(cli, "--help")
        if help_result.returncode != 0 or not help_result.stdout or help_result.stderr:
            raise AssertionError(
                "explicit help must be successful stdout-only output\n"
                f"stdout={help_result.stdout}\nstderr={help_result.stderr}"
            )
        expect_success_order(
            cli,
            ("CLI options:", "Animation options:", "Model options:", "Level options:"),
            "--help",
        )
        expect_success_contains(cli, "--generate-room-distances", "--help")
        expect_success_contains(cli, "--weld-vertices", "--help")
        expect_success_contains(cli, "    --weld-radius", "--help")
        expect_success_contains(cli, "--weld-radius <UNITS=0.0001>", "--help")
        expect_success_contains(cli, "--weld-metric <MODE=euclidean>", "--help")
        expect_success_contains(cli, "--weld-degenerate-faces <MODE=preserve>", "--help")
        expect_success_contains(cli, "    --nav-from-floor", "--help")
        expect_success_contains(cli, "    --nav-radius", "--help")
        expect_success_contains(cli, "--nav-radius <UNITS=50>", "--help")
        expect_success_contains(cli, "--prune-nav-surface-islands", "--help")
        expect_success_contains(cli, "    --nav-prune-min-area", "--help")
        expect_success_contains(cli, "    --rdist-spacing", "--help")
        expect_success_contains(cli, "    --rdist-offset", "--help")
        expect_success_contains(cli, "    --rdist-height", "--help")
        expect_success_contains(cli, "    --rdist-link-distance", "--help")
        expect_success_contains(cli, "--connect-anchors", "--help")
        expect_success_contains(cli, "--prune-anchor-islands", "--help")
        expect_success_contains(cli, "    --anchor-prune-min-count", "--help")
        expect_success_contains(cli, "--anchor-spacing <UNITS=2*radius>", "--help")
        expect_success_contains(cli, "--generate-static-lighting", "--help")
        expect_success_contains(cli, "    --light-ambient", "--help")
        expect_success_contains(cli, "--mount <FOLDER>", "--help")
        expect_success_contains(cli, "level:<N>", "--help")
        expect_success_contains(cli, "current directory is the only mount", "--help")
        expect_success_contains(cli, "--no-compression", "--help")
        expect_success_contains(cli, "--glb-arx-units-per-unit <UNITS=10>", "--help", "model")
        expect_success_contains(cli, "--glb-arx-units-per-unit <UNITS=100>", "--help", "level")
        expect_success_contains(cli, "--glb-offset <X=0> <Y=0> <Z=0>", "--help", "model")
        expect_success_contains(cli, "--glb-offset <X=0> <Y=0> <Z=0>", "--help", "level")
        expect_success_not_contains(cli, "--glb-arx-units-per-unit", "--help", "animation")
        expect_success_not_contains(cli, "--output-texture-folder", "--help", "model")
        expect_success_not_contains(cli, "--overwrite-texture", "--help", "level")
        expect_success_contains(cli, "--dlf-only", "--help")
        expect_success_contains(cli, "--no-quad-reconstruction", "--help")
        expect_success_contains(cli, "--skip-texture-export", "--help")
        expect_success_contains(cli, "--input-texture-folder", "--help")
        expect_success_contains(cli, "--output-texture-folder", "--help")
        expect_success_contains(cli, "--fts-scene-directory", "--help")
        expect_success_contains(cli, "--pretty", "--help", "level")
        expect_success_contains(cli, "arx-pistor", "--version", "--ignored-after-version")
        expect_code(cli, 1, "[CLI_DUPLICATE_MODULE]", "--pretty", "--pretty")
        expect_code(cli, 1, "[CLI_AMBIGUOUS_OPTION]", "--overw")
        expect_code(cli, 1, "[CLI_UNKNOWN_OPTION]", "--definitely-missing")
        parse_error = run(cli, "--definitely-missing")
        if (
            parse_error.stdout
            or "Usage:" in parse_error.stderr
            or "Run " not in parse_error.stderr
            or "--help for usage." not in parse_error.stderr
        ):
            raise AssertionError(
                f"parse failure emitted the wrong streams or full usage\n{parse_error.stdout}{parse_error.stderr}"
            )
        expect_code(cli, 1, "[CLI_UNKNOWN_OPTION]", "--foo", "--help")
        expect_code(cli, 1, "[CLI_UNKNOWN_OPTION]", "--debug-rooms")
        expect_code(cli, 1, "[CLI_UNKNOWN_OPTION]", "--debug-fts-rooms")
        expect_code(cli, 1, "[CLI_UNKNOWN_OPTION]", "--weld-debug")
        expect_code(cli, 1, "[CLI_UNKNOWN_OPTION]", "--normal-weld-degrees")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--input-texture-folder")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--output-texture-folder")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--fts-scene-directory")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--mount")
        expect_code(cli, 1, "[CLI_MISSING_ARGUMENT]", "--log-level")
        expect_code(cli, 1, "[CLI_LOG_LEVEL_INVALID]", "--log-level", "verbose")
        expect_code(cli, 1, "[CLI_DUPLICATE_MODULE]", "--log-level", "info", "--log-level", "warn")
        expect_code(cli, 1, "[CLI_KIND_INVALID]", "--kind", "asset", MODEL_GLB, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_INPUT_OUTPUT]", str(MODEL_GLB))
        expect_code(cli, 1, "[CLI_UNSUPPORTED_OUTPUT_FORMAT]", MODEL_GLB, tmp / "out.bin")
        expect_code(cli, 1, "[CLI_RESOURCE_SELECTOR_INVALID]", MODEL_FTL, "anim:items:bad")
        expect_code(cli, 1, "[CLI_RESOURCE_SELECTOR_INVALID]", "level:not-a-number", out_glb)
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
            "--generate-anchors",
            "--debug-navigation",
            "--debug-room-distances",
            LEVEL_FTS,
            out_glb,
        )
        expect_code(
            cli,
            1,
            "[CLI_MISSING_DEPENDENCY]",
            "--snap-bone-origins-to-reference",
            "snap-origins",
            MODEL_GLB,
            out_glb,
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
        expect_code(
            cli,
            1,
            "[CLI_MODULE_OUTPUT_MISMATCH]",
            "--output-texture-folder",
            "graph/obj3d/textures",
            level_json,
            tmp / "level8.fts.json",
        )
        expect_code(
            cli,
            1,
            "[CLI_MODULE_OUTPUT_MISMATCH]",
            "--fts-scene-directory",
            "graph/levels/custom",
            level_json,
            tmp / "level8.fts.json",
        )
        expect_code(
            cli,
            1,
            "[CLI_MODULE_OUTPUT_MISMATCH]",
            "--skip-texture-export",
            level_json,
            tmp / "level8.fts.json",
        )

        expect_code(cli, 1, "[CLI_AMBIGUOUS_ROUTE]", unknown_json, ANIMATION_TEA, out_json)
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
            "--output-texture-folder",
            "graph/obj3d/textures",
            empty_glb,
            tmp / "empty.fts",
        )
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--generate-nav-surface", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--generate-nav-surface", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--prune-nav-surface-islands", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--prune-nav-surface-islands", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--generate-room-distances", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--generate-room-distances", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--generate-anchors", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--generate-anchors", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--connect-anchors", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--connect-anchors", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--prune-anchor-islands", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--prune-anchor-islands", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--generate-static-lighting", "--debug-cells", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INCOMPATIBLE_MODULES]", "--debug-cells", "--generate-static-lighting", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--nav-radius", "50", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--nav-from-floor", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--nav-prune-ratio", "0.1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--nav-prune-min-area", "100", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--anchor-prune-ratio", "0.1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--anchor-prune-min-count", "2", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--weld-radius", "0.1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--weld-metric", "euclidean", LEVEL_FTS, out_glb)
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
            "--generate-nav-surface",
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
            "--generate-nav-surface",
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
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--weld-vertices", "--weld-radius", "0", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--weld-vertices", "--weld-radius", "nan", LEVEL_FTS, out_glb)
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
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-nav-surface", "--nav-radius", "4", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-nav-surface", "--nav-height", "-1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-nav-surface", "--nav-clearance", "-1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--prune-nav-surface-islands", "--nav-prune-ratio", "1.1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--prune-nav-surface-islands", "--nav-prune-min-area", "-1", LEVEL_FTS, out_glb)
        expect_code(
            cli,
            1,
            "[CLI_INVALID_MODULE_VALUE]",
            "--generate-nav-surface",
            "--nav-max-slope-degrees",
            "91",
            LEVEL_FTS,
            out_glb,
        )
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-room-distances", "--rdist-spacing", "0", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-room-distances", "--rdist-spacing", "19", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-room-distances", "--rdist-offset", "0", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-room-distances", "--rdist-offset", "51", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-room-distances", "--rdist-height", "0", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-room-distances", "--rdist-height", "49", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-room-distances", "--rdist-link-distance", "0", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-room-distances", "--rdist-spacing", "100", "--rdist-link-distance", "109", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--anchor-radius", "50", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--anchor-link-distance", "150", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--anchor-link-radius-scale", "0.9", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--light-ambient", "0.25", "0.25", "0.25", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--light-global-factor", "0.85", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--light-no-normals", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_MISSING_DEPENDENCY]", "--light-no-shadows", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-anchors", "--anchor-spacing", "9", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-anchors", "--anchor-radius", "4", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-anchors", "--anchor-height", "-1", LEVEL_FTS, out_glb)
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
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-static-lighting", "--light-ambient", "-0.1", "0.25", "0.25", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--generate-static-lighting", "--light-global-factor", "-1", LEVEL_FTS, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_CONSTRAINT_CONFLICT]", "--kind", "level", "--overwrite-texture", "foo", MODEL_FTL, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--scale", "1", ANIMATION_TEA, out_json)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--scale", "0", MODEL_FTL, out_json)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--scale", "nan", MODEL_FTL, out_json)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--rotate", "inf", "0", "0", MODEL_FTL, out_json)
        expect_code(cli, 1, "[CLI_INVALID_MODULE_VALUE]", "--offset", "0", "nan", "0", MODEL_FTL, out_json)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--generate-room-distances", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--weld-vertices", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--generate-anchors", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--connect-anchors", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--prune-nav-surface-islands", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--prune-anchor-islands", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--generate-static-lighting", ANIMATION_TEA, out_glb)
        expect_code(cli, 1, "[CLI_MODULE_OUTPUT_MISMATCH]", "--pretty", ANIMATION_TEA, tmp / "out.tea")
        expect_code(cli, 1, "[CLI_MODULE_OUTPUT_MISMATCH]", "--no-compression", ANIMATION_TEA, tmp / "out.tea")
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
            "--generate-anchors",
            "--connect-anchors",
            "--debug-navigation",
            LEVEL_FTS,
            tmp / "debug.fts",
        )
        if LEVEL_FTS_AVAILABLE:
            expect_success(
                cli,
                "--generate-nav-surface",
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
        expect_code(cli, 1, "[CLI_MODULE_OUTPUT_MISMATCH]", "--output-texture-folder", "graph", LEVEL_FTS, out_glb)
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
        expect_code(cli, 1, "[CLI_ROUTE_INPUT_MISMATCH]", "--overwrite-texture", "foo", ANIMATION_TEA, out_json)
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

            direct_mount = tmp / "direct-level-mount"
            expect_success(
                cli,
                "--mount",
                direct_mount,
                "--no-compression",
                "--skip-texture-export",
                LEVEL_FTS,
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
            expect_code(cli, 1, "[CLI_RESOURCE_SELECTOR_INVALID]", direct_dlf, tmp / "absolute-dlf-input.glb")
            expect_code(cli, 1, "[CLI_LEVEL_OUTPUT_FAILED]", LEVEL_FTS, tmp / "absolute-dlf-output.dlf")

            dlf_only_fts = tmp / "dlf-only-level.fts"
            dlf_only_dlf = dlf_only_fts.with_suffix(".dlf")
            expect_success(
                cli,
                "--dlf-only",
                "--no-quad-reconstruction",
                "--skip-texture-export",
                "--output-texture-folder",
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
                "--fts-scene-directory",
                "custom-scenes/new-scene",
                LEVEL_FTS,
                custom_scene_fts,
            )
            if custom_scene.returncode != 0:
                raise AssertionError(f"custom FTS scene directory failed\n{custom_scene.stdout}{custom_scene.stderr}")
            custom_scene_dlf = custom_scene_fts.with_suffix(".dlf")
            if b"custom-scenes/new-scene/\0" not in custom_scene_dlf.read_bytes():
                raise AssertionError("custom FTS scene directory was not written into DLF")
            if "loose Level output uses physical FTS" not in custom_scene.stderr:
                raise AssertionError("loose native output must report its detached runtime FTS reference")

            custom_scene_mount = tmp / "custom-scene-mount"
            expect_success(
                cli,
                "--mount",
                custom_scene_mount,
                "--no-compression",
                "--skip-texture-export",
                "--fts-scene-directory",
                "graph/my-level-folder/placed-scene/",
                LEVEL_FTS,
                "placed.dlf",
            )
            if not (custom_scene_mount / "game" / "graph" / "my-level-folder" / "placed-scene" / "fast.fts").exists():
                raise AssertionError("DLF-pattern output must place FTS at the configured runtime resource path")

            expect_code(
                cli,
                1,
                "[CLI_LEVEL_OUTPUT_FAILED]",
                "--fts-scene-directory",
                "../invalid-scene",
                LEVEL_FTS,
                tmp / "invalid-scene.fts",
            )

            dlf_selector_mount = tmp / "dlf-selector-mount"
            expect_success(cli, "--mount", dlf_selector_mount, "--dlf-only", LEVEL_FTS, "level:24")
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
                "--output-texture-folder",
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
                "--output-texture-folder",
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
            listing_high / "game/graph/obj3d/interactive/npc/hero/hero.ftl",
            listing_low / "game/graph/obj3d/interactive/npc/human/tweaks/red.ftl",
            listing_low / "game/graph/obj3d/interactive/items/armor/chest/chest.ftl",
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
        expected_listing = sorted(
            (
                "ambiance:cave/water",
                "anim:fix_inter:open",
                "anim:npc:walk",
                "cinematic:intro",
                "level:2",
                "level:3",
                "model:armor:chest",
                "model:npc:hero",
                "model:npc:human:red",
            )
        )
        if listing.returncode != 0 or listing.stdout.splitlines() != expected_listing:
            raise AssertionError(
                "resource listing must merge mounts by priority and emit sorted exact selectors\n"
                f"{listing.stdout}{listing.stderr}"
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
            "--mount",
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
        missing_mount = tmp / "missing-mount"
        expect_success_contains(
            cli,
            "[INFO/CLI] write mount not found; using as write-only and will create on actual write",
            "--mount",
            missing_mount,
            MODEL_FTL,
            tmp / "invalid-mount-model.json",
        )
        if missing_mount.exists():
            raise AssertionError("a prospective mount must not be created for a raw-path output")

        missing_read_mount = tmp / "missing-read-mount"
        expect_success_contains(
            cli,
            "[WARN/CLI] mount not found; excluding from readable mounts",
            "--mount",
            first_mount,
            "--mount",
            missing_read_mount,
            MODEL_FTL,
            tmp / "missing-read-mount.json",
        )

        prospective_mount = tmp / "prospective-mount"
        expect_success(cli, "--mount", prospective_mount, MODEL_FTL, "model:npc:prospective")
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
            "--mount",
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
        expect_code(cli, 1, "[CLI_IO_CREATE_FAILED]", "--mount", first_mount, MODEL_FTL, "model:npc:NUL")

        model_input_mount = tmp / "model-input-mount"
        mounted_model = model_input_mount / "game" / "graph" / "obj3d" / "interactive" / "npc" / "Adventurer" / "Adventurer.ftl"
        mounted_model.parent.mkdir(parents=True)
        shutil.copyfile(MODEL_FTL, mounted_model)
        expect_success(cli, "--mount", model_input_mount, "model:npc:Adventurer", tmp / "mounted-model-input.json")

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
        written_animation = first_mount / "graph" / "obj3d" / "anims" / "npc" / "CharacterArmature_Idle.tea"
        if not written_animation.is_file():
            raise AssertionError(f"expected npc Model animation output at {written_animation}")

        collision_primary = tmp / "CharacterArmature_Idle.json"
        expect_success(cli, "--overwrite", MODEL_FTL, ANIMATION_TEA, collision_primary)
        collision_sidecar = tmp / "CharacterArmature_Idle2.json"
        if not collision_primary.is_file() or not collision_sidecar.is_file():
            raise AssertionError("model JSON and animation JSON targets must be disambiguated before writing")

        expect_success(
            cli,
            "--mount",
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
        fix_inter_animation = first_mount / "graph" / "obj3d" / "anims" / "fix_inter" / "CharacterArmature_Idle.tea"
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
            "--overwrite",
            ANIMATION_TEA,
            "anim:fix_inter:provided",
        )
        named_animation = first_mount / "graph" / "obj3d" / "anims" / "fix_inter" / "CharacterArmature_Idle.tea"
        if not named_animation.is_file():
            raise AssertionError(f"native TEA output must use the animation name: {named_animation}")

        if LEVEL_BUNDLE_AVAILABLE:
            level_input_mount = tmp / "level-input-mount"
            mounted_level = level_input_mount / "graph" / "levels" / "level1"
            mounted_level.mkdir(parents=True)
            mounted_game_level = level_input_mount / "game" / "graph" / "levels" / "level1"
            mounted_game_level.mkdir(parents=True)
            shutil.copyfile(LEVEL_DLF, mounted_level / "level1.dlf")
            shutil.copyfile(LEVEL_LLF, mounted_level / "level1.llf")
            shutil.copyfile(LEVEL_BUNDLE_FTS, mounted_game_level / "fast.fts")
            expect_success(cli, "--mount", level_input_mount, "level:1", tmp / "mounted-level-input.glb")

            expect_success(
                cli,
                "--mount",
                first_mount,
                "--mount",
                second_mount,
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
                "Level texture images were not found in the flat input folder",
                LEVEL_FTS,
                unmounted_level_glb,
            )

            no_read_mount = tmp / "no-read-mount"
            expect_success_contains(
                cli,
                "Level texture images were not found in the flat input folder",
                "--mount",
                no_read_mount,
                LEVEL_FTS,
                tmp / "no-read-mount.glb",
            )

        expect_success_contains(cli, "[INFO/PISTORIS]", "--log-level", "In", MODEL_FTL, tmp / "log-info.json")
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

        expect_success(cli, "--help", "level", "--foo")
        expect_success_contains(
            cli,
            "CLI conventions:",
            "--help",
            "lev",
            "c",
        )
        expect_success_contains(
            cli,
            "Level GLB examples: docs/AUTHORING_GUIDE.md.",
            "--help",
            "level",
            "conventions",
        )
        expect_success_contains(
            cli,
            "Exact Level GLB naming: docs/AUTHORING_REFERENCE.md.",
            "--help",
            "level",
            "conventions",
        )
        expect_success_contains(
            cli,
            "Level outputs:           GLB, JSON, loose FTS bundle, DLF game-resource bundle",
            "--help",
        )
        expect_success(cli, "--k", "a", ANIMATION_TEA, out_json, "--pre", "--overwrite")
        expect_success(
            cli,
            "--kind",
            "a",
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
