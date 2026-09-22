#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

import json
import shutil
import struct
import sys
from pathlib import Path


SEED_DIRECTORIES = (
    "amb",
    "ambiance-glb-source",
    "audio",
    "cin",
    "cinematic-glb-source",
    "dlf",
    "ftl",
    "fts",
    "glb-container",
    "image",
    "llf",
    "obj",
    "obj-mtl",
    "tea",
    "model-glb-source",
    "level-glb-source",
)

IMAGE_EXTENSIONS = {".bmp", ".jpeg", ".jpg", ".png", ".tga"}
AUDIO_EXTENSIONS = {".mp3", ".ogg", ".wav"}


def catalog_path(value, context):
    if value is None:
        return None
    if isinstance(value, dict):
        if "path" not in value:
            raise ValueError(f"{context} must contain 'path'")
        value = value["path"]
    if not isinstance(value, str) or not value:
        raise ValueError(f"{context} must be a non-empty path")
    path = Path(value)
    if path.is_absolute() or ".." in path.parts:
        raise ValueError(f"{context} must stay inside the fixtures directory: {value}")
    return path


def claim_target(claimed_targets, target, output_root):
    relative = target.relative_to(output_root).as_posix().casefold()
    if relative in claimed_targets:
        raise ValueError(f"Multiple fixtures target the same seed path: {target}")
    claimed_targets.add(relative)


def copy_fixture(fixtures_root, output_root, claimed_targets, value, directory, name, context):
    relative_path = catalog_path(value, context)
    if relative_path is None:
        return
    source = fixtures_root / relative_path
    if not source.is_file():
        raise FileNotFoundError(f"Missing fixture for {context}: {source}")
    target = output_root / directory / name
    claim_target(claimed_targets, target, output_root)
    shutil.copyfile(source, target)


def material_library_paths(obj_text):
    paths = []
    for line in obj_text.splitlines():
        directive = line.lstrip()
        if directive.startswith("mtllib "):
            paths.extend(directive[7:].split())
    return paths


def copy_obj_mtl_seed(fixtures_root, output_root, claimed_targets, model):
    relative_path = catalog_path(model.get("obj"), f"model {model['name']}.obj")
    if relative_path is None:
        return
    source = fixtures_root / relative_path
    if not source.is_file():
        raise FileNotFoundError(f"Missing OBJ fixture for model {model['name']}: {source}")
    obj_bytes = source.read_bytes()
    paths = material_library_paths(obj_bytes.decode("utf-8", errors="replace"))
    if not paths:
        return
    material_bytes = []
    for path in paths:
        library = source.parent / path
        if not library.is_file():
            raise FileNotFoundError(f"Missing OBJ material library: {library}")
        material_bytes.append(library.read_bytes())
    packed = bytearray(struct.pack("<I", len(obj_bytes)))
    packed.extend(obj_bytes)
    for library in material_bytes:
        packed.extend(struct.pack("<I", len(library)))
        packed.extend(library)
    target = output_root / "obj-mtl" / f"{model['name']}.obj-mtl"
    claim_target(claimed_targets, target, output_root)
    target.write_bytes(packed)


def collect_project_media(fixtures_root, value, context, image_sources, audio_sources):
    relative_path = catalog_path(value, context)
    if relative_path is None:
        return
    project_root = fixtures_root / relative_path.parent
    for source in project_root.rglob("*"):
        if not source.is_file():
            continue
        relative = source.relative_to(fixtures_root)
        extension = source.suffix.casefold()
        if extension in IMAGE_EXTENSIONS:
            image_sources[relative.as_posix().casefold()] = relative
        elif extension in AUDIO_EXTENSIONS:
            audio_sources[relative.as_posix().casefold()] = relative


def copy_media_seeds(fixtures_root, output_root, claimed_targets, sources, directory):
    for index, relative_path in enumerate(sources.values()):
        source = fixtures_root / relative_path
        if not source.is_file():
            raise FileNotFoundError(f"Missing {directory} fixture: {source}")
        target = output_root / directory / f"{index:04d}{source.suffix.casefold()}"
        claim_target(claimed_targets, target, output_root)
        shutil.copyfile(source, target)


def main():
    if len(sys.argv) != 3:
        print("Usage: prepare_fixture_seeds.py <catalog> <output-dir>")
        return 1

    catalog_file = Path(sys.argv[1])
    output_root = Path(sys.argv[2])
    fixtures_root = catalog_file.parent
    with catalog_file.open(encoding="utf-8") as stream:
        catalog = json.load(stream)

    if output_root.exists():
        shutil.rmtree(output_root)
    for directory in SEED_DIRECTORIES:
        (output_root / directory).mkdir(parents=True)

    claimed_targets = set()
    image_sources = {}
    audio_sources = {}

    for level in catalog["levels"]:
        name = level["name"]
        copy_fixture(
            fixtures_root, output_root, claimed_targets, level.get("fts"), "fts", f"{name}.fts", f"level {name}.fts"
        )
        copy_fixture(
            fixtures_root, output_root, claimed_targets, level.get("llf"), "llf", f"{name}.llf", f"level {name}.llf"
        )
        copy_fixture(
            fixtures_root, output_root, claimed_targets, level.get("dlf"), "dlf", f"{name}.dlf", f"level {name}.dlf"
        )
        copy_fixture(
            fixtures_root,
            output_root,
            claimed_targets,
            level.get("glb"),
            "level-glb-source",
            f"{name}.glb",
            f"level {name}.glb",
        )
        copy_fixture(
            fixtures_root,
            output_root,
            claimed_targets,
            level.get("glb"),
            "glb-container",
            f"level-{name}.glb",
            f"level {name}.glb container",
        )
        collect_project_media(
            fixtures_root, level.get("glb"), f"level {name}.glb", image_sources, audio_sources
        )

    for model in catalog["models"]:
        name = model["name"]
        copy_fixture(
            fixtures_root, output_root, claimed_targets, model.get("ftl"), "ftl", f"{name}.ftl", f"model {name}.ftl"
        )
        copy_fixture(
            fixtures_root, output_root, claimed_targets, model.get("obj"), "obj", f"{name}.obj", f"model {name}.obj"
        )
        copy_obj_mtl_seed(fixtures_root, output_root, claimed_targets, model)
        copy_fixture(
            fixtures_root,
            output_root,
            claimed_targets,
            model.get("glb"),
            "model-glb-source",
            f"{name}.glb",
            f"model {name}.glb",
        )
        copy_fixture(
            fixtures_root,
            output_root,
            claimed_targets,
            model.get("glb"),
            "glb-container",
            f"model-{name}.glb",
            f"model {name}.glb container",
        )
        collect_project_media(
            fixtures_root, model.get("glb"), f"model {name}.glb", image_sources, audio_sources
        )
        collect_project_media(
            fixtures_root, model.get("obj"), f"model {name}.obj", image_sources, audio_sources
        )

    for animation in catalog["animations"]:
        name = animation["name"]
        copy_fixture(
            fixtures_root,
            output_root,
            claimed_targets,
            animation.get("tea"),
            "tea",
            f"{name}.tea",
            f"animation {name}.tea",
        )

    for ambiance in catalog["ambiances"]:
        name = ambiance["name"]
        copy_fixture(
            fixtures_root,
            output_root,
            claimed_targets,
            ambiance.get("amb"),
            "amb",
            f"{name}.amb",
            f"ambiance {name}.amb",
        )
        copy_fixture(
            fixtures_root,
            output_root,
            claimed_targets,
            ambiance.get("glb"),
            "ambiance-glb-source",
            f"{name}.glb",
            f"ambiance {name}.glb",
        )
        copy_fixture(
            fixtures_root,
            output_root,
            claimed_targets,
            ambiance.get("glb"),
            "glb-container",
            f"ambiance-{name}.glb",
            f"ambiance {name}.glb container",
        )
        collect_project_media(
            fixtures_root, ambiance.get("glb"), f"ambiance {name}.glb", image_sources, audio_sources
        )

    for cinematic in catalog["cinematics"]:
        name = cinematic["name"]
        copy_fixture(
            fixtures_root,
            output_root,
            claimed_targets,
            cinematic.get("cin"),
            "cin",
            f"{name}.cin",
            f"cinematic {name}.cin",
        )
        copy_fixture(
            fixtures_root,
            output_root,
            claimed_targets,
            cinematic.get("glb"),
            "cinematic-glb-source",
            f"{name}.glb",
            f"cinematic {name}.glb",
        )
        copy_fixture(
            fixtures_root,
            output_root,
            claimed_targets,
            cinematic.get("glb"),
            "glb-container",
            f"cinematic-{name}.glb",
            f"cinematic {name}.glb container",
        )
        collect_project_media(
            fixtures_root, cinematic.get("glb"), f"cinematic {name}.glb", image_sources, audio_sources
        )

    for value in catalog["native_sidecars"]["images"]:
        relative = catalog_path(value, "native image sidecar")
        image_sources[relative.as_posix().casefold()] = relative
    for value in catalog["native_sidecars"]["audio"]:
        relative = catalog_path(value, "native audio sidecar")
        audio_sources[relative.as_posix().casefold()] = relative

    image_sources = dict(sorted(image_sources.items()))
    audio_sources = dict(sorted(audio_sources.items()))
    copy_media_seeds(fixtures_root, output_root, claimed_targets, image_sources, "image")
    copy_media_seeds(fixtures_root, output_root, claimed_targets, audio_sources, "audio")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
