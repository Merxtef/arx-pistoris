#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CATALOG = ROOT / "data" / "fixtures" / "catalog.json"
DEFAULT_OUTPUT_MOUNT = ROOT / "data" / "fixtures" / "mount"
WRITTEN_PREFIX = "[INFO/CLI] written: "

OutputLedger = dict[Path, tuple[str, str]]


def default_cli() -> Path:
    name = "arx-pistor.exe" if os.name == "nt" else "arx-pistor"
    return ROOT / "build" / "bin" / name


def fixture_path(fixtures_root: Path, entry: dict[str, object], field: str) -> Path:
    value = entry[field]
    if isinstance(value, dict):
        value = value["path"]
    if not isinstance(value, str):
        raise TypeError(f"Fixture field {field!r} must resolve to a path string")
    return fixtures_root / value


def glb_units(entry: dict[str, object]) -> str:
    glb = entry["glb"]
    if not isinstance(glb, dict):
        raise TypeError("GLB fixture metadata must be an object")
    return str(glb["arx_units_per_glb_unit"])


def file_digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def record_output(mount: Path, output: Path, owner: str, ledger: OutputLedger) -> None:
    resolved_mount = mount.resolve()
    resolved_output = output.resolve(strict=True)
    try:
        relative = resolved_output.relative_to(resolved_mount)
    except ValueError as error:
        raise ValueError(f"Generated output is outside the temporary mount: {output}") from error

    digest = file_digest(resolved_output)
    previous = ledger.get(relative)
    if previous is None:
        ledger[relative] = (digest, owner)
        return
    if previous[0] != digest:
        raise ValueError(
            f"Generated output collision at {relative}: {previous[1]} and {owner} emitted different bytes"
        )


def run_conversion(cli: Path, mount: Path, arguments: list[str], owner: str, ledger: OutputLedger) -> None:
    command = [
        str(cli),
        "--log-level",
        "INFO",
        "--write-mount",
        str(mount),
        "--overwrite",
        *arguments,
    ]
    process = subprocess.Popen(command, cwd=ROOT, stderr=subprocess.PIPE, text=True, encoding="utf-8")
    if process.stderr is None:
        process.kill()
        raise RuntimeError("Cannot capture CLI diagnostics")

    write_count = 0
    collision: ValueError | None = None
    for line in process.stderr:
        sys.stderr.write(line)
        sys.stderr.flush()
        message = line.rstrip("\r\n")
        if not message.startswith(WRITTEN_PREFIX):
            continue
        write_count += 1
        try:
            record_output(mount, Path(message[len(WRITTEN_PREFIX) :]), owner, ledger)
        except ValueError as error:
            if collision is None:
                collision = error

    return_code = process.wait()
    if return_code != 0:
        raise subprocess.CalledProcessError(return_code, command)
    if write_count == 0:
        raise RuntimeError(f"{owner} conversion reported no written outputs")
    if collision is not None:
        raise collision


def level_arguments(entry: dict[str, object]) -> list[str]:
    regeneration = entry.get("regeneration", {})
    if not isinstance(regeneration, dict):
        raise TypeError("Level regeneration metadata must be an object")

    result: list[str] = []
    navigation = regeneration.get("navigation")
    if navigation:
        if not isinstance(navigation, dict):
            raise TypeError("Level navigation regeneration metadata must be an object")
        result.append("--gen-nav-surface")
        if navigation.get("prune"):
            result.append("--prune-nav-surface-islands")
    if regeneration.get("room_distances"):
        result.append("--gen-room-distances")

    anchors = regeneration.get("anchors")
    if anchors:
        if not isinstance(anchors, dict):
            raise TypeError("Level anchor regeneration metadata must be an object")
        result.append("--gen-anchors")
        if anchors.get("connect"):
            result.append("--connect-anchors")
        if anchors.get("prune"):
            result.append("--prune-anchor-islands")

    if regeneration.get("static_lighting"):
        result.append("--gen-static-lighting")
        ambient = regeneration.get("light_ambient")
        if ambient is not None:
            if not isinstance(ambient, list) or len(ambient) != 3:
                raise ValueError("Level light_ambient must contain three channels")
            result.extend(("--light-ambient", *(str(channel) for channel in ambient)))
    if regeneration.get("minimap"):
        result.append("--gen-minimap")
    return result


def mount_relative(path: str) -> Path:
    catalog_path = Path(path)
    if not catalog_path.parts or catalog_path.parts[0] != "mount":
        raise ValueError(f"Native fixture path must be below mount/: {path}")
    relative = Path(*catalog_path.parts[1:])
    if not relative.parts or relative.is_absolute() or relative.drive or ".." in relative.parts:
        raise ValueError(f"Native fixture path must stay below mount/: {path}")
    return relative


def native_alias_groups(catalog: dict[str, object]) -> list[tuple[str, list[str]]]:
    groups = catalog.get("native_aliases")
    if not isinstance(groups, list):
        raise TypeError("Native alias metadata must be an array")

    result: list[tuple[str, list[str]]] = []
    for group in groups:
        if not isinstance(group, dict):
            raise TypeError("Native alias group must be an object")
        source = group.get("source")
        aliases = group.get("aliases")
        if not isinstance(source, str):
            raise TypeError("Native alias source must be a string")
        if not isinstance(aliases, list) or not aliases:
            raise TypeError("Native alias list must be a non-empty array")
        if not all(isinstance(alias, str) for alias in aliases):
            raise TypeError("Native alias path must be a string")
        result.append((source, aliases))
    return result


def expected_generated_files(catalog: dict[str, object]) -> set[Path]:
    result: set[Path] = set()

    def add(path: str) -> None:
        relative = mount_relative(path)
        if relative in result:
            raise ValueError(f"Duplicate generated fixture path: {path}")
        result.add(relative)

    for level in catalog["levels"]:
        for field in ("fts", "llf", "dlf"):
            add(level[field])
    for model in catalog["models"]:
        add(model["ftl"])
    for animation in catalog["animations"]:
        add(animation["tea"])
    for ambiance in catalog["ambiances"]:
        add(ambiance["amb"])
    for cinematic in catalog["cinematics"]:
        add(cinematic["cin"])
    native_sidecars = catalog["native_sidecars"]
    if not isinstance(native_sidecars, dict):
        raise TypeError("Native sidecar metadata must be an object")
    for kind in ("images", "audio"):
        paths = native_sidecars.get(kind)
        if not isinstance(paths, list):
            raise TypeError(f"Native {kind} sidecars must be an array")
        for path in paths:
            if not isinstance(path, str):
                raise TypeError(f"Native {kind} sidecar path must be a string")
            add(path)

    primary_files = set(result)
    for source, aliases in native_alias_groups(catalog):
        source_path = mount_relative(source)
        if source_path not in primary_files:
            raise ValueError(f"Native alias source is not a generated fixture: {source}")
        for alias in aliases:
            if mount_relative(alias) == source_path:
                raise ValueError(f"Native alias matches its source: {alias}")
            add(alias)
    return result


def regenerate(cli: Path, catalog_path: Path, output_mount: Path) -> None:
    fixtures_root = catalog_path.parent
    with catalog_path.open(encoding="utf-8") as stream:
        catalog = json.load(stream)
    expected_files = expected_generated_files(catalog)

    with tempfile.TemporaryDirectory(prefix="arx-pistoris-fixtures-") as temporary:
        generated_mount = Path(temporary) / "mount"
        generated_mount.mkdir()
        output_ledger: OutputLedger = {}

        for level in catalog["levels"]:
            arguments = [
                "--glb-arx-units-per-unit",
                glb_units(level),
                *level_arguments(level),
                str(fixture_path(fixtures_root, level, "glb")),
                level["selector"],
            ]
            run_conversion(cli, generated_mount, arguments, f"Level {level['name']!r}", output_ledger)

        for model in catalog["models"]:
            source_field = "glb" if "glb" in model else "obj"
            arguments: list[str] = []
            if source_field == "glb":
                arguments.extend(("--glb-arx-units-per-unit", glb_units(model)))
            regeneration = model.get("regeneration", {})
            icon_slots = regeneration.get("icon_slots")
            if icon_slots is not None:
                if not isinstance(icon_slots, list) or len(icon_slots) != 2:
                    raise ValueError("Model icon_slots must contain width and height")
                arguments.extend(("--icon-slots", *(str(value) for value in icon_slots)))
            arguments.extend((str(fixture_path(fixtures_root, model, source_field)), model["selector"]))
            run_conversion(cli, generated_mount, arguments, f"Model {model['name']!r}", output_ledger)

        for ambiance in catalog["ambiances"]:
            run_conversion(
                cli,
                generated_mount,
                [
                    "--glb-arx-units-per-unit",
                    glb_units(ambiance),
                    str(fixture_path(fixtures_root, ambiance, "glb")),
                    ambiance["selector"],
                ],
                f"Ambiance {ambiance['name']!r}",
                output_ledger,
            )

        for cinematic in catalog["cinematics"]:
            run_conversion(
                cli,
                generated_mount,
                [str(fixture_path(fixtures_root, cinematic, "glb")), cinematic["selector"]],
                f"Cinematic {cinematic['name']!r}",
                output_ledger,
            )

        for source, aliases in native_alias_groups(catalog):
            source_path = generated_mount / mount_relative(source)
            if not source_path.is_file():
                raise ValueError(f"Native alias source was not generated: {source}")
            for alias in aliases:
                alias_path = generated_mount / mount_relative(alias)
                alias_path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source_path, alias_path)
                record_output(generated_mount, alias_path, f"Native alias of {source!r}", output_ledger)

        generated_files = {
            path.relative_to(generated_mount) for path in generated_mount.rglob("*") if path.is_file()
        }
        if generated_files != expected_files:
            missing = sorted(expected_files - generated_files)
            unexpected = sorted(generated_files - expected_files)
            raise ValueError(f"Generated fixture set differs from catalog: missing={missing}, unexpected={unexpected}")

        output_mount.mkdir(parents=True, exist_ok=True)
        shutil.copytree(generated_mount, output_mount, dirs_exist_ok=True)
        print(f"Regenerated {len(tuple(generated_mount.rglob('*')))} fixture mount entries in {output_mount}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Regenerate the cataloged native fixture mount")
    parser.add_argument("--cli", type=Path, default=default_cli())
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    parser.add_argument("--output-mount", type=Path, default=DEFAULT_OUTPUT_MOUNT)
    arguments = parser.parse_args()

    cli = arguments.cli.resolve()
    catalog = arguments.catalog.resolve()
    output_mount = arguments.output_mount.resolve()
    if not cli.is_file():
        parser.error(f"CLI executable not found: {cli}")
    regenerate(cli, catalog, output_mount)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
