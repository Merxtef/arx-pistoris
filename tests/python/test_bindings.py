# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

import gc
import importlib
import json
import re
import tempfile
import unittest
from collections.abc import Collection, ItemsView, KeysView, MutableMapping, MutableSequence, MutableSet, Sequence, ValuesView
from pathlib import Path, PurePosixPath, PureWindowsPath
from unittest import mock

import pistoris


FIXTURES = Path(__file__).resolve().parents[2] / "data" / "fixtures"
FIXTURE_CATALOG = json.loads((FIXTURES / "catalog.json").read_text(encoding="utf-8"))


def fixture(path: str) -> bytes:
    return (FIXTURES / path).read_bytes()


def fixture_text(path: str) -> str:
    return (FIXTURES / path).read_text(encoding="utf-8")


def catalog_fixture_entry(section: str, name: str) -> dict[str, object]:
    entries = FIXTURE_CATALOG.get(section)
    if not isinstance(entries, list):
        raise TypeError(f"Fixture catalog section {section!r} must be an array")
    for entry in entries:
        if isinstance(entry, dict) and entry.get("name") == name:
            return entry
    raise KeyError(f"Fixture catalog has no {name!r} entry in {section!r}")


def catalog_fixture_path(section: str, name: str, field: str) -> str:
    value = catalog_fixture_entry(section, name).get(field)
    if isinstance(value, dict):
        value = value.get("path")
    if not isinstance(value, str):
        raise TypeError(f"Fixture catalog field {section}.{name}.{field} must resolve to a path string")
    return value


def catalog_fixture(section: str, name: str, field: str) -> bytes:
    return fixture(catalog_fixture_path(section, name, field))


def catalog_glb_fixture(section: str, name: str) -> tuple[bytes, float]:
    value = catalog_fixture_entry(section, name).get("glb")
    if not isinstance(value, dict):
        raise TypeError(f"Fixture catalog field {section}.{name}.glb must contain GLB metadata")
    path = value.get("path")
    units = value.get("arx_units_per_glb_unit")
    if not isinstance(path, str):
        raise TypeError(f"Fixture catalog field {section}.{name}.glb.path must be a string")
    if isinstance(units, bool) or not isinstance(units, (int, float)):
        raise TypeError(f"Fixture catalog field {section}.{name}.glb.arx_units_per_glb_unit must be a number")
    if not 1 <= units <= 1000:
        raise ValueError(
            f"Fixture catalog field {section}.{name}.glb.arx_units_per_glb_unit must be inside inclusive [1, 1000]"
        )
    return fixture(path), float(units)


def fixture_relative_text(primary_path: str, referenced_path: str) -> str:
    for label, path in (("Primary fixture", primary_path), ("Fixture sidecar", referenced_path)):
        if PurePosixPath(path).anchor or PureWindowsPath(path).anchor:
            raise ValueError(f"{label} path must be relative: {path}")

    fixtures_root = FIXTURES.resolve()
    primary = (fixtures_root / primary_path).resolve()
    if not primary.is_relative_to(fixtures_root):
        raise ValueError(f"Primary fixture escapes the fixture root: {primary_path}")
    project = primary.parent
    referenced = (project / referenced_path).resolve()
    if not referenced.is_relative_to(project):
        raise ValueError(f"Fixture sidecar escapes its project: {referenced_path}")
    return referenced.read_text(encoding="utf-8")


class FixtureCatalogTests(unittest.TestCase):
    def test_glb_units_follow_conversion_range(self) -> None:
        entry = {"name": "range", "glb": {"path": "unused.glb", "arx_units_per_glb_unit": 1}}
        with (
            mock.patch.dict(FIXTURE_CATALOG, {"range_test": [entry]}),
            mock.patch(f"{__name__}.fixture", return_value=b"glb"),
        ):
            for units in (1, 1000):
                with self.subTest(units=units):
                    entry["glb"]["arx_units_per_glb_unit"] = units
                    self.assertEqual(catalog_glb_fixture("range_test", "range"), (b"glb", float(units)))

            for units in (0, 1001, float("inf"), float("nan")):
                with self.subTest(units=units):
                    entry["glb"]["arx_units_per_glb_unit"] = units
                    with self.assertRaises(ValueError):
                        catalog_glb_fixture("range_test", "range")

            for units in (True, "100"):
                with self.subTest(units=units):
                    entry["glb"]["arx_units_per_glb_unit"] = units
                    with self.assertRaises(TypeError):
                        catalog_glb_fixture("range_test", "range")

    def test_fixture_relative_text_rejects_anchored_paths(self) -> None:
        for primary, referenced in (
            ("C:\\fixtures\\model.obj", "material.mtl"),
            ("/fixtures/model.obj", "material.mtl"),
            ("models/model.obj", "C:\\fixtures\\material.mtl"),
            ("models/model.obj", "/fixtures/material.mtl"),
        ):
            with self.subTest(primary=primary, referenced=referenced):
                with self.assertRaises(ValueError):
                    fixture_relative_text(primary, referenced)


class ErrorTests(unittest.TestCase):
    def test_native_read_exposes_structured_error(self) -> None:
        with self.assertRaises(pistoris.PistorisError) as raised:
            pistoris.native.ftl.read(b"")

        error = raised.exception
        self.assertNotEqual(error.code, 0)
        self.assertEqual(str(error), error.message)
        self.assertIn("unexpected end of data", error.message)
        self.assertEqual(error.detail, "")
        self.assertEqual(error.location.domain, "ftl")
        self.assertEqual(error.location.element_name, "FTL header")
        self.assertEqual(error.location.byte_offset, 0)
        self.assertEqual(error.location.field, "identifier")

    def test_glb_error_exposes_format_location(self) -> None:
        with self.assertRaises(pistoris.PistorisError) as raised:
            pistoris.Model.from_glb(b"")

        self.assertEqual(raised.exception.location.domain, "glb")
        self.assertEqual(raised.exception.location.element_name, "document")

    def test_json_error_exposes_byte_offset(self) -> None:
        with self.assertRaises(pistoris.PistorisError) as raised:
            pistoris.native.ftl.from_json("{")

        self.assertEqual(raised.exception.location.domain, "json")
        self.assertEqual(raised.exception.location.byte_offset, 1)

    def test_resource_error_exposes_semantic_element(self) -> None:
        with self.assertRaises(pistoris.PistorisError) as raised:
            pistoris.Model().validate()

        self.assertEqual(raised.exception.location.domain, "model")
        self.assertEqual(raised.exception.location.element_name, "resource")
        self.assertIsNone(raised.exception.location.index)

    def test_native_carrier_error_exposes_field(self) -> None:
        with self.assertRaises(pistoris.PistorisError) as raised:
            pistoris.native.ftl.validate(pistoris.native.ftl.Data())

        self.assertEqual(raised.exception.location.domain, "ftl")
        self.assertEqual(raised.exception.location.element_name, "FTL header")
        self.assertEqual(raised.exception.location.field, "vertices")

    def test_dlf_embedded_llf_error_preserves_variant_location(self) -> None:
        dlf = pistoris.native.dlf.read(
            fixture("mount/graph/levels/level9/level9.dlf")
        ).dlf
        lighting = pistoris.native.llf.Data()
        light = pistoris.native.llf.Light()
        light.color = pistoris.math.Color3(2.0, 0.0, 0.0)
        lighting.lights.append(light)

        with self.assertRaises(pistoris.PistorisError) as raised:
            pistoris.native.dlf.write(dlf, lighting, compress=False)

        self.assertEqual(raised.exception.location.domain, "llf")
        self.assertEqual(raised.exception.location.element_name, "LLF light")
        self.assertEqual(raised.exception.location.index, 0)
        self.assertEqual(raised.exception.location.field, "color")


class ResourceIoTests(unittest.TestCase):
    def test_mounts_are_live_and_masks_select_providers(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            high = root / "high"
            low = root / "low"
            (high / "data").mkdir(parents=True)
            (low / "data").mkdir(parents=True)
            (high / "data" / "shared.txt").write_bytes(b"high")
            (low / "data" / "shared.txt").write_bytes(b"low")

            mounts = pistoris.resource_io.ResourceMounts([high, low])
            self.assertEqual([mount.id for mount in mounts.read_mounts], [1, 2])
            self.assertEqual(mounts.available_mount_mask, 3)
            self.assertEqual(mounts.highest_priority_mount(3), mounts.read_mounts[0])
            self.assertIsNone(mounts.highest_priority_mount(0))
            self.assertEqual(
                repr(mounts),
                "ResourceMounts(read_mounts=2, write_mount=None)",
            )
            self.assertEqual(mounts.read("data/shared.txt").data, b"high")
            self.assertEqual(mounts.read("data/shared.txt", mount_mask=2).data, b"low")

            (low / "data" / "added.ftl").write_bytes(b"later")
            files = mounts.list_files("data", max_depth=1)
            added = next(file for file in files if file.logical_path == "data/added.ftl")
            self.assertEqual(added.provider_mask, 2)
            self.assertEqual(
                added,
                next(file for file in mounts.list_files("data", max_depth=1) if file.logical_path == "data/added.ftl"),
            )
            self.assertEqual(repr(added), "ResourceFile(logical_path='data/added.ftl', provider_mask=2)")

            entries = {entry.name: entry for entry in mounts.list_directory("data")}
            self.assertEqual(entries["shared.txt"].provider_mask, 3)
            self.assertEqual(entries["added.ftl"].kind, pistoris.resource_io.mounts.DirectoryEntryKind.FTL)
            self.assertEqual(
                entries["added.ftl"],
                next(entry for entry in mounts.list_directory("data") if entry.name == "added.ftl"),
            )
            self.assertEqual(
                repr(entries["added.ftl"]),
                "DirectoryEntry(name='added.ftl', kind=DirectoryEntryKind.FTL, provider_mask=2)",
            )

    def test_catalog_is_a_fresh_immutable_snapshot(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            model = root / "game" / "graph" / "obj3d" / "interactive" / "npc" / "tool"
            model.mkdir(parents=True)
            (model / "tool.ftl").write_bytes(b"fixture")

            resources = pistoris.resource_io.Resources([root])
            first = resources.scan_catalog()
            self.assertIsInstance(first, Sequence)
            expected = pistoris.paths.ModelSelector(pistoris.paths.ModelType.NPC, "tool")
            self.assertEqual([entry.selector for entry in first], [expected])
            self.assertEqual(first.get(expected), first[0])
            self.assertEqual(list(first.models()), [first[0]])
            self.assertEqual(list(first.animations()), [])
            self.assertEqual(list(first.levels()), [])
            self.assertEqual(list(first.cinematics()), [])
            self.assertEqual(list(first.ambiances()), [])
            with self.assertRaises(KeyError):
                first.get(pistoris.paths.CinematicSelector("missing"))
            self.assertEqual(first[0].provider_mask, 1)
            self.assertEqual([entry.selector for entry in first[:]], [expected])
            self.assertEqual(repr(first), "<pistoris.resource_io.catalog.Catalog len=1>")
            self.assertEqual(first[0], resources.scan_catalog()[0])
            self.assertEqual(
                repr(first[0]),
                "Entry(selector=ModelSelector(type=ModelType.NPC, name='tool', tweak=''), provider_mask=1)",
            )

            other = model.parent / "other"
            other.mkdir()
            (other / "other.ftl").write_bytes(b"fixture")
            self.assertEqual([str(entry.selector) for entry in first], ["model:npc:tool"])
            self.assertEqual(
                [str(entry.selector) for entry in resources.scan_catalog()],
                ["model:npc:other", "model:npc:tool"],
            )

    def test_resources_load_mounted_level(self) -> None:
        resources = pistoris.resource_io.Resources()
        resources.mounts.add_read_mount(FIXTURES / "mount")
        self.assertIn("Resources(mounts=ResourceMounts(read_mounts=1", repr(resources))

        catalog = resources.scan_catalog()
        self.assertIn("level:9", [str(entry.selector) for entry in catalog])
        level = resources.load_level(9)
        self.assertIsInstance(level, pistoris.Level)
        self.assertIsNotNone(level.minimap.encoded_image)
        self.assertIsNotNone(level.loading_screen.encoded_image)
        self.assertIsInstance(resources.load_level(pistoris.paths.LevelSelector(9)), pistoris.Level)
        self.assertIs(resources.mounts, resources.mounts)

    def test_resources_load_complete_native_resources(self) -> None:
        resources = pistoris.resource_io.Resources([FIXTURES / "mount"])

        imported = resources.load_model("model:weapons:sword_00")
        self.assertIsInstance(imported, pistoris.model.Import)
        model = imported.model
        self.assertEqual(imported.animations, ())
        self.assertTrue(model.mesh.textures)
        self.assertTrue(all(texture.encoded_image for texture in model.mesh.textures))
        self.assertIsNotNone(model.inventory_icon.copy())

        animation = resources.load_animation("anim:npc:human_male_gathering")
        self.assertEqual(len(animation.sounds), 2)
        self.assertTrue(all(sound.encoded_audio for sound in animation.sounds))

        ambiance = resources.load_ambiance("ambiance:explore")
        self.assertTrue(ambiance.sounds)
        self.assertTrue(all(sound.encoded_audio for sound in ambiance.sounds))

        cinematic = resources.load_cinematic("cinematic:numbers")
        self.assertEqual(len(cinematic.illustrations), 3)
        self.assertTrue(all(illustration.encoded_image for illustration in cinematic.illustrations))
        self.assertEqual({language.name for language in cinematic.languages}, {"deutsch", "english", "francais"})
        self.assertEqual(sum(1 for sound in cinematic.sfx if sound.encoded_audio), 1)
        self.assertEqual(sum(len(sound.encodings) for sound in cinematic.speech), 27)

    def test_resources_write_concrete_selector_targets(self) -> None:
        resources = pistoris.resource_io.Resources([FIXTURES / "mount"])
        model_selector = pistoris.paths.ModelSelector(pistoris.paths.ModelType.WEAPONS, "sword_00")
        animation_selector = pistoris.paths.AnimationSelector(
            pistoris.paths.AnimationType.NPC, "human_male_gathering"
        )
        level_selector = pistoris.paths.LevelSelector(9)
        ambiance_selector = pistoris.paths.AmbianceSelector("explore")
        cinematic_selector = pistoris.paths.CinematicSelector("numbers")

        model = resources.load_model(model_selector).model
        animation = resources.load_animation(animation_selector)
        level = resources.load_level(level_selector)
        ambiance = resources.load_ambiance(ambiance_selector)
        cinematic = resources.load_cinematic(cinematic_selector)

        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            resources.mounts.write_mount = output
            primary = pistoris.resource_io.OutputPart.PRIMARY

            report = resources.write_model(model, model_selector, outputs=primary)
            self.assertTrue(report)
            self.assertEqual(report[0].status, pistoris.resource_io.output.WriteStatus.WRITTEN)
            ambiance_plan = resources.prepare_ambiance_write(ambiance, ambiance_selector, outputs=primary)
            self.assertIsInstance(ambiance_plan, pistoris.resource_io.output.WritePlan)
            resources.write_animation(animation, animation_selector, outputs=primary)
            resources.write_level(level, level_selector, outputs=primary)
            resources.write_ambiance(ambiance, ambiance_selector, outputs=primary)
            resources.write_cinematic(cinematic, cinematic_selector, outputs=primary)

            for selector in (
                model_selector,
                animation_selector,
                level_selector,
                ambiance_selector,
                cinematic_selector,
            ):
                self.assertTrue((output / selector.to_path()).is_file())

    def test_resources_accept_pythonic_sources_and_named_read_only_properties(self) -> None:
        resources = pistoris.resource_io.Resources([FIXTURES / "mount"])
        with self.assertRaisesRegex(AttributeError, "property 'mounts'"):
            resources.mounts = pistoris.resource_io.ResourceMounts()

        semantic = pistoris.paths.ModelSelector(pistoris.paths.ModelType.WEAPONS, "sword_00")
        self.assertIsInstance(resources.load_model(semantic), pistoris.model.Import)
        logical = semantic.to_path()
        self.assertIsInstance(resources.load_model(Path(logical[:-4])), pistoris.model.Import)

        model_path = FIXTURES / FIXTURE_CATALOG["models"][0]["ftl"]
        self.assertIsInstance(resources.load_model_file(model_path), pistoris.model.Import)
        self.assertIsInstance(resources.load_model_file(str(model_path)), pistoris.model.Import)

        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            resources.mounts.write_mount = output / "mounted"
            model = resources.load_model(semantic).model
            resources.write_model(model, Path("exports/sword"))
            self.assertTrue((output / "mounted" / "exports" / "sword.ftl").is_file())
            resources.write_model_file(model, output / "loose" / "sword.glb")
            self.assertTrue((output / "loose" / "sword.glb").is_file())
            self.assertTrue((output / "loose" / "sword[icon].png").is_file())

            primary = output / "primary" / "sword.glb"
            initial_report = resources.write_model_file(
                model,
                primary,
                outputs=pistoris.resource_io.OutputPart.PRIMARY,
            )
            self.assertEqual(len(initial_report), 1)
            self.assertEqual(
                initial_report[0].status,
                pistoris.resource_io.output.WriteStatus.WRITTEN,
            )
            self.assertTrue(primary.is_file())
            self.assertFalse((output / "primary" / "sword[icon].png").exists())

            primary.write_bytes(b"existing")
            with self.assertRaisesRegex(pistoris.PistorisError, "PRESERVE or OVERWRITE"):
                resources.write_model_file(model, primary, outputs=pistoris.resource_io.OutputPart.PRIMARY)
            self.assertEqual(primary.read_bytes(), b"existing")

            plan = resources.prepare_model_file_write(
                model,
                primary,
                outputs=pistoris.resource_io.OutputPart.PRIMARY,
            )
            preflight = plan.preflight()
            self.assertEqual(
                preflight[0].status,
                pistoris.resource_io.output.WriteStatus.NEEDS_EXISTING_FILE_POLICY,
            )
            self.assertIsNone(plan[0].if_exists)
            plan[0].if_exists = pistoris.resource_io.ExistingFilePolicy.PRESERVE
            planned = plan.execute()
            self.assertEqual(planned[0].status, pistoris.resource_io.output.WriteStatus.PRESERVED)
            self.assertEqual(primary.read_bytes(), b"existing")
            plan[0].if_exists = None
            self.assertIsNone(plan[0].if_exists)
            self.assertEqual(
                plan.preflight()[0].status,
                pistoris.resource_io.output.WriteStatus.NEEDS_EXISTING_FILE_POLICY,
            )

            resources.write_model_file(
                model,
                primary,
                outputs=pistoris.resource_io.OutputPart.PRIMARY,
                if_exists=pistoris.resource_io.ExistingFilePolicy.PRESERVE,
            )
            self.assertEqual(primary.read_bytes(), b"existing")
            resources.write_model_file(
                model,
                primary,
                outputs=pistoris.resource_io.OutputPart.PRIMARY,
                if_exists=pistoris.resource_io.ExistingFilePolicy.OVERWRITE,
            )
            self.assertNotEqual(primary.read_bytes(), b"existing")

            with self.assertRaisesRegex(ValueError, "compress does not apply to GLB output"):
                resources.write_model_file(model, output / "invalid.glb", compress=True)
            with self.assertRaisesRegex(ValueError, "arx_units_per_glb_unit does not apply to native output"):
                resources.write_model_file(model, output / "invalid.ftl", arx_units_per_glb_unit=10.0)

            primary_and_images = output / "selected" / "sword.glb"
            resources.write_model_file(
                model,
                primary_and_images,
                outputs=(
                    pistoris.resource_io.OutputPart.PRIMARY
                    | pistoris.resource_io.OutputPart.IMAGES
                ),
            )
            self.assertTrue(primary_and_images.is_file())
            self.assertTrue((output / "selected" / "sword[icon].png").is_file())

            glb, _ = catalog_glb_fixture("models", "human_male")
            source = output / "animated.glb"
            source.write_bytes(glb)
            with self.assertRaises(pistoris.PistorisError):
                resources.load_model_file(source, arx_units_per_glb_unit=0.0)
            animated = resources.load_model_file(source)
            self.assertGreater(len(animated.animations), 0)
            animated_copy = output / "animated-copy.glb"
            with self.assertRaises(pistoris.PistorisError):
                resources.write_model_file(animated, animated_copy, arx_units_per_glb_unit=0.0)
            resources.write_model_file(animated, animated_copy, outputs=pistoris.resource_io.OutputPart.PRIMARY)
            roundtrip = resources.load_model_file(animated_copy)
            self.assertEqual(len(roundtrip.animations), len(animated.animations))

            with self.assertRaises(pistoris.PistorisError):
                resources.load_model(model_path)
            with self.assertRaises(pistoris.PistorisError):
                resources.write_model(model, output / "loose" / "other.glb")

    def test_loose_fts_json_level_identity_round_trips_through_resources(self) -> None:
        fts_path = FIXTURES / catalog_fixture_path("levels", "level9", "fts")
        fts = pistoris.native.fts.read(fts_path.read_bytes())

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source.fts.json"
            source.write_text(pistoris.native.fts.to_json(fts, level=9), encoding="utf-8")
            resources = pistoris.resource_io.Resources()
            level = resources.load_level_file(source)
            self.assertEqual(level.resource_path, pistoris.paths.LevelSelector(9).to_path())

            inferred = root / "inferred.fts.json"
            resources.write_level_file(level, inferred)
            self.assertEqual(pistoris.native.fts.from_json(inferred.read_text(encoding="utf-8")).level, 9)

            loose = resources.load_level_file(fts_path)
            self.assertEqual(loose.resource_path, "")
            with self.assertRaisesRegex(pistoris.PistorisError, "level_index"):
                resources.write_level_file(loose, root / "missing.fts.json")
            explicit = root / "explicit.fts.json"
            resources.write_level_file(loose, explicit, level_index=7)
            self.assertEqual(pistoris.native.fts.from_json(explicit.read_text(encoding="utf-8")).level, 7)

            resources.mounts.write_mount = root / "mounted"
            resources.write_level(
                loose,
                "editing/explicit.fts.json",
                level_index=8,
                outputs=pistoris.resource_io.OutputPart.PRIMARY,
            )
            mounted = root / "mounted" / "editing" / "explicit.fts.json"
            self.assertEqual(pistoris.native.fts.from_json(mounted.read_text(encoding="utf-8")).level, 8)

    def test_mount_configuration_is_mutable_and_reports_missing_reads_as_warnings(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            read = root / "read"
            read.mkdir()
            mounts = pistoris.resource_io.ResourceMounts()

            first = mounts.add_read_mount(read)
            duplicate = mounts.add_read_mount(read)
            self.assertIsNotNone(first)
            self.assertEqual(first, duplicate)
            self.assertEqual(first.path, read.resolve())
            self.assertEqual(len(mounts.read_mounts), 1)
            self.assertIsInstance(mounts.read_mounts, tuple)

            with self.assertWarnsRegex(UserWarning, "read mount does not exist"):
                missing = mounts.add_read_mount(root / "missing")
            self.assertIsNone(missing)
            self.assertEqual(len(mounts.read_mounts), 1)

            mounts.write_mount = root / "output"
            self.assertEqual(mounts.write_mount, root.resolve() / "output")
            mounts.write_mount = None
            self.assertIsNone(mounts.write_mount)

    def test_errors_preserve_logical_resource_path(self) -> None:
        mounts = pistoris.resource_io.ResourceMounts()
        with self.assertRaises(pistoris.PistorisError) as raised:
            mounts.read("missing/file.ftl")

        self.assertEqual(raised.exception.location.domain, "resource_io")
        self.assertEqual(raised.exception.location.resource_path, "missing/file.ftl")
        self.assertIsNone(raised.exception.location.native_path)
        self.assertEqual(raised.exception.location.operation, pistoris.resource_io.Operation.READ)
        self.assertEqual(raised.exception.location.mount_mask, pistoris.resource_io.ALL_MOUNTS)

    def test_resource_conversion_errors_preserve_typed_content_locations(self) -> None:
        resources = pistoris.resource_io.Resources()
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "broken.glb"
            source.write_bytes(b"glTF")
            with self.assertRaises(pistoris.PistorisError) as raised:
                resources.load_model_file(source)

        self.assertEqual(raised.exception.location.domain, "glb")
        self.assertEqual(raised.exception.location.element_name, "document")
        self.assertEqual(raised.exception.location.native_path, source)
        self.assertEqual(raised.exception.location.operation, pistoris.resource_io.Operation.READ)

    def test_case_collisions_fail_by_default_and_can_be_recovered(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = root / "data"
            directory.mkdir()
            (directory / "VALUE.FTL").write_bytes(b"upper")
            (directory / "value.ftl").write_bytes(b"lower")
            if len(list(directory.iterdir())) < 2:
                self.skipTest("filesystem does not preserve case-colliding filenames")

            mounts = pistoris.resource_io.ResourceMounts([root])
            with self.assertRaises(pistoris.PistorisError) as raised:
                mounts.read("data/value.ftl")

            self.assertEqual(raised.exception.location.resource_path, "data/value.ftl")
            self.assertEqual(raised.exception.location.native_path, directory)
            self.assertEqual(
                mounts.read("data/value.ftl", recover_case_collisions=True).data,
                b"upper",
            )


class NativeCarrierTests(unittest.TestCase):
    def test_format_modules_are_importable(self) -> None:
        module = importlib.import_module("pistoris.native.ftl")
        self.assertIs(module, pistoris.native.ftl)
        self.assertEqual(
            set(pistoris.native.__all__),
            {"amb", "cin", "dlf", "ftl", "fts", "llf", "tea"},
        )
        self.assertFalse(hasattr(pistoris.native, "FtlVertexList"))
        self.assertFalse(hasattr(pistoris.native, "FtsTextureMap"))

    def test_collections_are_mutable_and_text_fields_are_bytes(self) -> None:
        data = pistoris.native.ftl.Data()
        data.vertices.append(pistoris.native.ftl.Vertex())
        self.assertEqual(len(data.vertices), 1)
        self.assertIsInstance(data.vertices, MutableSequence)
        data.vertices = [pistoris.native.ftl.Vertex(), pistoris.native.ftl.Vertex()]
        self.assertEqual(len(data.vertices), 2)
        data.vertices = (pistoris.native.ftl.Vertex() for _ in range(3))
        self.assertEqual(len(data.vertices), 3)

        fts = pistoris.native.fts.Data()
        self.assertIsInstance(fts.textures, MutableMapping)
        with self.assertRaises(TypeError):
            hash(fts.textures)
        fts.textures = {7: pistoris.native.fts.Texture()}
        self.assertEqual(list(fts.textures), [7])
        fts.textures.update()
        fts.textures.update([(9, pistoris.native.fts.Texture())])
        texture_keys = fts.textures.keys()
        self.assertIsInstance(texture_keys, KeysView)
        self.assertIsInstance(fts.textures.items(), ItemsView)
        self.assertIsInstance(fts.textures.values(), ValuesView)
        self.assertEqual(texture_keys & {7, 9}, {7, 9})
        del fts.textures[9]
        self.assertEqual(set(texture_keys), {7})

        ids = pistoris.native.fts.Cell().anchor_ids
        ids.extend([1, 2, 3])
        self.assertEqual(list(reversed(ids)), [3, 2, 1])
        self.assertEqual(ids.index(2), 1)
        ids.reverse()
        ids += [4, 5]
        self.assertEqual(list(ids), [3, 2, 1, 4, 5])

        self.assertIsNone(fts.textures.get(8))
        fts.textures.setdefault(8, pistoris.native.fts.Texture())
        self.assertIn(8, fts.textures)
        self.assertIsInstance(fts.textures.pop(7), pistoris.native.fts.Texture)
        key, value = fts.textures.popitem()
        self.assertEqual(key, 8)
        self.assertIsInstance(value, pistoris.native.fts.Texture)

        data.vertices[0].position = pistoris.math.Vector3(1.0, 2.0, 3.0)
        self.assertEqual(data.vertices[0].position, pistoris.math.Vector3(1.0, 2.0, 3.0))

        ambiance = pistoris.native.amb.Data()
        ambiance.tracks.append(pistoris.native.amb.Track())
        ambiance.tracks[0].keys.append(pistoris.native.amb.Key())
        ambiance.tracks[0].keys[0].volume.min = 0.25
        self.assertEqual(ambiance.tracks[0].keys[0].volume.min, 0.25)

        cinematic = pistoris.native.cin.Data()
        cinematic.keyframes.append(pistoris.native.cin.Keyframe())
        cinematic.keyframes[0].light.intensity = 0.5
        self.assertEqual(cinematic.keyframes[0].light.intensity, 0.5)

        level = pistoris.native.fts.Data()
        level.portals.append(pistoris.native.fts.Portal())
        level.portals[0].polygon.transval = 0.75
        self.assertEqual(level.portals[0].polygon.transval, 0.75)

        entity = pistoris.native.dlf.Entity()
        entity.class_path = b"graph/obj3d/interactive/test"
        self.assertEqual(entity.class_path, b"graph/obj3d/interactive/test")
        with self.assertRaises(ValueError):
            entity.class_path = b"graph/obj3d\0hidden"

    def test_json_uses_native_carriers(self) -> None:
        formats = (
            (pistoris.native.amb, "mount/sfx/ambiance/dark.amb"),
            (
                pistoris.native.ftl,
                "mount/game/graph/obj3d/interactive/npc/human_male/human_male.ftl",
            ),
            (
                pistoris.native.tea,
                "mount/graph/obj3d/anims/fix_inter/json_dice.tea",
            ),
            (pistoris.native.llf, "mount/graph/levels/level9/level9.llf"),
        )

        for module, path in formats:
            with self.subTest(format=module.__name__):
                data = module.read(fixture(path))
                encoded = module.to_json(data)
                module.validate(module.from_json(encoded))

        fts = pistoris.native.fts.read(
            fixture("mount/game/graph/levels/level9/fast.fts")
        )
        encoded = pistoris.native.fts.to_json(fts, level=9)
        imported = pistoris.native.fts.from_json(encoded)
        self.assertEqual(imported.level, 9)
        pistoris.native.fts.validate(imported.data)

    def test_native_formats_round_trip_through_binary(self) -> None:
        formats = (
            ("amb", pistoris.native.amb, "mount/sfx/ambiance/dark.amb"),
            (
                "cin",
                pistoris.native.cin,
                "mount/graph/interface/illustrations/numbers.cin",
            ),
            (
                "ftl",
                pistoris.native.ftl,
                "mount/game/graph/obj3d/interactive/npc/human_male/human_male.ftl",
            ),
            (
                "tea",
                pistoris.native.tea,
                "mount/graph/obj3d/anims/fix_inter/json_dice.tea",
            ),
            (
                "fts",
                pistoris.native.fts,
                "mount/game/graph/levels/level9/fast.fts",
            ),
            (
                "llf",
                pistoris.native.llf,
                "mount/graph/levels/level9/level9.llf",
            ),
        )

        for name, module, path in formats:
            with self.subTest(format=name):
                data = module.read(fixture(path))
                module.validate(data)

                encoded = module.write(data)
                self.assertGreater(len(encoded), 0)
                module.validate(module.read(encoded))

    def test_dlf_round_trips_bundle_and_json(self) -> None:
        bundle = pistoris.native.dlf.read(
            fixture("mount/graph/levels/level9/level9.dlf")
        )
        pistoris.native.dlf.validate(bundle.dlf)
        if bundle.embedded_lighting is not None:
            pistoris.native.llf.validate(bundle.embedded_lighting)

        encoded = pistoris.native.dlf.write(bundle.dlf, bundle.embedded_lighting)
        self.assertGreater(len(encoded), 0)
        decoded = pistoris.native.dlf.read(encoded)
        pistoris.native.dlf.validate(decoded.dlf)
        if decoded.embedded_lighting is not None:
            pistoris.native.llf.validate(decoded.embedded_lighting)

        json_data = pistoris.native.dlf.to_json(bundle.dlf)
        pistoris.native.dlf.validate(pistoris.native.dlf.from_json(json_data))


class ResourceConversionTests(unittest.TestCase):
    def test_model_native_and_glb_inputs(self) -> None:
        ftl_bytes = fixture("mount/game/graph/obj3d/interactive/npc/human_male/human_male.ftl")
        carrier = pistoris.native.ftl.read(ftl_bytes)
        model = pistoris.Model.from_ftl(carrier).model
        self.assertIsInstance(model.mesh.vertices, pistoris.model.VertexCollection)
        self.assertGreater(len(model.mesh.vertices), 0)
        native_output = model.to_ftl_bytes(include_sidecars=False)
        self.assertGreater(len(native_output.ftl), 0)
        self.assertIsInstance(native_output.texture_files, pistoris.TextureFileSequence)

        glb, units = catalog_glb_fixture("models", "human_male")
        glb_model = pistoris.Model.from_glb(glb, arx_units_per_glb_unit=units).model
        self.assertEqual(len(glb_model.mesh.vertices), len(model.mesh.vertices))

        copied = model.copy()
        copied.resource_path = "model:npc:copy"
        self.assertNotEqual(copied.resource_path, model.resource_path)

    def test_animation_native_input(self) -> None:
        animation = pistoris.Animation.from_tea_bytes(
            fixture("mount/graph/obj3d/anims/fix_inter/json_dice.tea")
        ).animation
        self.assertGreater(len(animation.keyframes), 0)
        self.assertGreater(len(animation.to_tea_bytes(include_sidecars=False).tea), 0)

        carrier = pistoris.native.tea.read(
            fixture("mount/graph/obj3d/anims/npc/human_male_gathering.tea")
        )
        imported = pistoris.Animation.from_tea(carrier)
        self.assertGreater(len(imported.animation.keyframes), 0)
        self.assertEqual(
            [(source.sound_path, source.source_path) for source in imported.sound_sources],
            [("sfx/misc_04.wav", "misc_04"), ("sfx/misc_08.wav", "misc_08")],
        )

    def test_obj_input_can_return_texture_sources(self) -> None:
        obj_path = catalog_fixture_path("models", "custom_dagger", "obj")
        obj = fixture_text(obj_path)
        material_paths = pistoris.model.obj_material_library_paths(obj)
        self.assertEqual(material_paths, ["custom_dagger.mtl"])
        material_path = material_paths[0]
        mtl = fixture_relative_text(obj_path, material_path)
        imported = pistoris.Model.from_obj(
            obj,
            mtl,
        )
        self.assertGreater(len(imported.model.mesh.faces), 0)
        self.assertEqual(imported.texture_source_paths, ("custom_dagger_texture.png",))

        library = pistoris.model.ObjMaterialLibrary()
        library.path = material_path
        library.text = mtl
        path_aware = pistoris.Model.from_obj(obj, [library])
        self.assertEqual(len(path_aware.model.mesh.faces), len(imported.model.mesh.faces))

        class GeneratedLibraries:
            def __len__(self) -> int:
                return 1

            def __getitem__(self, index: int) -> pistoris.model.ObjMaterialLibrary:
                if index != 0:
                    raise IndexError
                value = pistoris.model.ObjMaterialLibrary()
                value.path = material_path
                value.text = mtl
                return value

        generated = pistoris.Model.from_obj(obj, GeneratedLibraries())
        self.assertEqual(len(generated.model.mesh.faces), len(imported.model.mesh.faces))

        broken_library = pistoris.model.ObjMaterialLibrary()
        broken_library.path = "broken.mtl"
        broken_library.text = "newmtl\n"
        with self.assertRaises(pistoris.PistorisError) as raised:
            pistoris.Model.from_obj("", [broken_library])
        self.assertEqual(raised.exception.location.domain, "mtl")
        self.assertEqual(raised.exception.location.source_index, 0)
        self.assertEqual(raised.exception.location.line, 1)
        self.assertEqual(raised.exception.location.source_path, "broken.mtl")

    def test_ambiance_native_and_glb_inputs(self) -> None:
        carrier = pistoris.native.amb.read(fixture("mount/sfx/ambiance/dark.amb"))
        ambiance = pistoris.Ambiance.from_amb(carrier).ambiance
        self.assertGreater(len(ambiance.tracks), 0)
        self.assertGreater(len(ambiance.to_amb_bytes(include_sidecars=False).amb), 0)

        glb, units = catalog_glb_fixture("ambiances", "dark")
        glb_ambiance = pistoris.Ambiance.from_glb(glb, arx_units_per_glb_unit=units).ambiance
        self.assertGreater(len(glb_ambiance.tracks), 0)

    def test_cinematic_native_and_glb_inputs(self) -> None:
        cinematic = pistoris.Cinematic.from_cin_bytes(
            fixture("mount/graph/interface/illustrations/numbers.cin")
        ).cinematic
        self.assertGreater(len(cinematic.keyframes), 0)
        self.assertGreater(
            len(
                cinematic.to_cin_bytes(
                    include_illustration_sidecars=False,
                    include_sound_sidecars=False,
                ).cin
            ),
            0,
        )

        imported = pistoris.Cinematic.from_glb(
            catalog_fixture("cinematics", "numbers", "glb")
        )
        self.assertEqual(len(imported.cinematic.keyframes), len(cinematic.keyframes))
        self.assertGreater(len(imported.sound_sources), 0)
        kinds = {source.kind for source in imported.sound_sources}
        self.assertIn(pistoris.cinematic.SoundKind.EFFECT, kinds)
        self.assertIn(pistoris.cinematic.SoundKind.SPEECH, kinds)

    def test_conversion_sidecars_are_immutable(self) -> None:
        glb, units = catalog_glb_fixture("models", "human_male")
        model = pistoris.Model.from_glb(glb, arx_units_per_glb_unit=units).model
        texture_files = model.to_ftl_bytes().texture_files
        texture_file = texture_files[0]

        animation = pistoris.Animation.from_tea_bytes(
            fixture("mount/graph/obj3d/anims/npc/human_male_gathering.tea")
        ).animation
        audio = fixture("mount/speech/english/one.wav")
        animation.sounds[0].encoded_audio = audio
        sound_files = animation.to_tea_bytes().sound_files
        sound_file = sound_files[0]

        cinematic = pistoris.Cinematic.from_cin_bytes(
            fixture("mount/graph/interface/illustrations/numbers.cin")
        ).cinematic
        cinematic.sfx.append(pistoris.Sound(path="test"))
        sound = cinematic.sfx[-1]
        sound.encoded_audio = audio
        cinematic.keyframes[0].sound = sound
        cinematic_sound_files = cinematic.to_cin_bytes().sound_files
        cinematic_sound_file = cinematic_sound_files[0]
        self.assertEqual(
            repr(texture_files),
            f"<pistoris.TextureFileSequence len={len(texture_files)}>",
        )
        self.assertEqual(
            repr(sound_files),
            f"<pistoris.SoundFileSequence len={len(sound_files)}>",
        )
        self.assertEqual(
            repr(cinematic_sound_files),
            f"<pistoris.cinematic.SoundFileSequence len={len(cinematic_sound_files)}>",
        )
        self.assertEqual(sound_file.source_path, animation.sounds[0].path)
        self.assertIsNone(cinematic_sound_file.language)
        self.assertIs(cinematic_sound_file.kind, pistoris.cinematic.SoundKind.EFFECT)

        for sequence, value, metadata, media in (
            (texture_files, texture_file, "path", "encoded_image"),
            (sound_files, sound_file, "path", "encoded_audio"),
            (cinematic_sound_files, cinematic_sound_file, "path", "encoded_audio"),
        ):
            self.assertIn(value, sequence)
            self.assertEqual(sequence.count(value), 1)
            self.assertEqual(sequence.index(value), 0)
            self.assertEqual(next(reversed(sequence)), sequence[-1])
            self.assertNotIn(" object at ", repr(value))
            with self.assertRaises(AttributeError):
                setattr(value, metadata, "changed")
            with self.assertRaises(AttributeError):
                setattr(value, media, b"changed")

    def test_level_native_and_glb_inputs(self) -> None:
        fts = fixture("mount/game/graph/levels/level9/fast.fts")
        llf = fixture("mount/graph/levels/level9/level9.llf")
        dlf = fixture("mount/graph/levels/level9/level9.dlf")
        level = pistoris.Level.from_native_bytes(fts, llf, dlf).level
        self.assertGreater(len(level.mesh.faces), 0)

        minimap_image = fixture("mount/graph/levels/level9/map.png")
        level.minimap.set_from_projection(minimap_image, pistoris.math.Vector2())
        rendered = level.minimap.render(
            projection_offset=pistoris.math.Vector2(), format=pistoris.ImageFormat.BMP
        )
        self.assertIsInstance(rendered, pistoris.level.RenderedMinimap)
        self.assertTrue(rendered.encoded_image.startswith(b"BM"))
        self.assertEqual(rendered.projection_offset, pistoris.math.Vector2())
        game_rendered = level.minimap.render(
            mode=pistoris.level.MinimapRenderMode.GAME,
            projection_offset=pistoris.math.Vector2(),
        )
        self.assertTrue(game_rendered.encoded_image.startswith(b"\x89PNG\r\n\x1a\n"))
        with self.assertRaises(pistoris.PistorisError):
            level.minimap.render(border_color=pistoris.math.Color3(1.0, 1.0, 1.0))

        loading_image = fixture("mount/graph/levels/level9/loading.png")
        level.loading_screen.encoded_image = loading_image
        self.assertTrue(
            level.loading_screen.render(
                layout=pistoris.level.LoadingScreenLayout.NORMAL,
                format=pistoris.ImageFormat.BMP,
            ).startswith(b"BM")
        )
        self.assertEqual(
            pistoris.level.projection_offset_from_mini_offset(pistoris.math.Vector2(1.0, 2.0)),
            pistoris.math.Vector2(65.0, 124.0),
        )
        self.assertEqual(
            pistoris.level.projection_offset_for_level(15, pistoris.math.Vector2()),
            pistoris.math.Vector2(2015.0, -217.0),
        )

        native = level.to_native_bytes(level_name="level9", include_sidecars=False)
        self.assertGreater(len(native.fts), 0)
        self.assertGreater(len(native.llf), 0)
        self.assertGreater(len(native.dlf), 0)

        glb, units = catalog_glb_fixture("levels", "level9")
        glb_import = pistoris.Level.from_glb(glb, arx_units_per_glb_unit=units)
        self.assertIsInstance(glb_import.glb_info, pistoris.level.GlbImportInfo)
        glb_level = glb_import.level
        self.assertEqual(len(glb_level.mesh.faces), len(level.mesh.faces))
        glb_output = glb_level.to_glb()
        self.assertIsInstance(glb_output, pistoris.level.GlbOutput)
        self.assertGreater(len(glb_output.glb), 0)
        self.assertIsInstance(glb_output.model_preview_report, pistoris.level.ModelPreviewReport)

    def test_native_byte_inputs_can_return_sources(self) -> None:
        model = pistoris.Model.from_ftl_bytes(
            fixture("mount/game/graph/obj3d/interactive/npc/human_male/human_male.ftl")
        )
        self.assertIsInstance(model.model, pistoris.Model)
        self.assertIsInstance(model.texture_source_paths, tuple)
        self.assertGreater(len(model.texture_source_paths), 0)

        animation = pistoris.Animation.from_tea_bytes(
            fixture("mount/graph/obj3d/anims/npc/human_male_gathering.tea")
        )
        self.assertIsInstance(animation.animation, pistoris.Animation)
        self.assertIsInstance(animation.sound_sources, tuple)
        self.assertEqual(
            [(source.sound_path, source.source_path) for source in animation.sound_sources],
            [("sfx/misc_04.wav", "misc_04"), ("sfx/misc_08.wav", "misc_08")],
        )

        ambiance = pistoris.Ambiance.from_amb_bytes(fixture("mount/sfx/ambiance/dark.amb"))
        self.assertIsInstance(ambiance.ambiance, pistoris.Ambiance)
        self.assertIsInstance(ambiance.sound_sources, tuple)
        self.assertGreater(len(ambiance.sound_sources), 0)

        cinematic = pistoris.Cinematic.from_cin_bytes(
            fixture("mount/graph/interface/illustrations/numbers.cin")
        )
        self.assertIsInstance(cinematic.cinematic, pistoris.Cinematic)
        self.assertIsInstance(cinematic.illustration_source_paths, tuple)
        self.assertGreater(len(cinematic.illustration_source_paths), 0)

        level = pistoris.Level.from_native_bytes(
            fixture("mount/game/graph/levels/level9/fast.fts"),
            fixture("mount/graph/levels/level9/level9.llf"),
            fixture("mount/graph/levels/level9/level9.dlf"),
        )
        self.assertIsInstance(level.level, pistoris.Level)
        self.assertIsInstance(level.texture_source_paths, tuple)
        self.assertGreater(len(level.texture_source_paths), 0)

    def test_import_options_keep_stable_bundle_shapes(self) -> None:
        model = pistoris.Model.from_ftl_bytes(
            fixture("mount/game/graph/obj3d/interactive/npc/human_male/human_male.ftl"),
            include_texture_sources=False,
        )
        self.assertIsInstance(model.model, pistoris.Model)
        self.assertEqual(model.animations, ())
        self.assertEqual(model.texture_source_paths, ())
        self.assertEqual(model.sound_sources, ())
        self.assertIsNone(model.animation_report)

        glb, units = catalog_glb_fixture("models", "human_male")
        glb_model = pistoris.Model.from_glb(
            glb,
            include_animations=False,
            include_texture_sources=False,
            include_sound_sources=False,
            arx_units_per_glb_unit=units,
        )
        self.assertIsInstance(glb_model.model, pistoris.Model)
        self.assertEqual(glb_model.animations, ())
        self.assertEqual(glb_model.texture_source_paths, ())
        self.assertEqual(glb_model.sound_sources, ())
        self.assertIsNone(glb_model.animation_report)

        animation = pistoris.Animation.from_tea_bytes(
            fixture("mount/graph/obj3d/anims/npc/human_male_gathering.tea"),
            include_sound_sources=False,
        )
        self.assertEqual(animation.sound_sources, ())

        ambiance = pistoris.Ambiance.from_amb_bytes(
            fixture("mount/sfx/ambiance/dark.amb"),
            include_sound_sources=False,
        )
        self.assertEqual(ambiance.sound_sources, ())

        cinematic = pistoris.Cinematic.from_cin_bytes(
            fixture("mount/graph/interface/illustrations/numbers.cin"),
            include_illustration_sources=False,
            include_sound_sources=False,
        )
        self.assertEqual(cinematic.illustration_source_paths, ())
        self.assertEqual(cinematic.sound_sources, ())

        level = pistoris.Level.from_native_bytes(
            fixture("mount/game/graph/levels/level9/fast.fts"),
            fixture("mount/graph/levels/level9/level9.llf"),
            fixture("mount/graph/levels/level9/level9.dlf"),
            include_texture_sources=False,
        )
        self.assertEqual(level.texture_source_paths, ())
        self.assertIsNone(level.glb_info)


class PythonErgonomicsTests(unittest.TestCase):
    def test_domain_modules_and_root_aliases(self) -> None:
        self.assertIs(pistoris.Model, pistoris.model.Model)
        self.assertIs(pistoris.Animation, pistoris.animation.Animation)
        self.assertIs(pistoris.Ambiance, pistoris.ambiance.Ambiance)
        self.assertIs(pistoris.Cinematic, pistoris.cinematic.Cinematic)
        self.assertIs(pistoris.Level, pistoris.level.Level)
        self.assertIs(pistoris.DegenerateFacePolicy, pistoris.model.DegenerateFacePolicy)
        self.assertIs(pistoris.PositionWeldMetric, pistoris.level.PositionWeldMetric)
        self.assertEqual(pistoris.Model.__module__, "pistoris")
        self.assertEqual(pistoris.model.Vertex.__module__, "pistoris.model")
        self.assertEqual(pistoris.model.Vertex.__name__, "Vertex")
        self.assertEqual(
            repr(pistoris.Model().mesh),
            "<pistoris.model.Mesh vertices=0 faces=0 textures=0>",
        )
        self.assertEqual(
            repr(pistoris.Model().skeleton),
            "<pistoris.model.Skeleton bones=0>",
        )
        self.assertEqual(
            repr(pistoris.Level().mesh),
            "<pistoris.level.Mesh vertices=0 faces=0 textures=0>",
        )
        self.assertEqual(
            repr(pistoris.Level().nav_surface),
            "<pistoris.level.NavSurface present=False vertices=0 triangles=0>",
        )
        self.assertEqual(
            repr(pistoris.Animation().groups),
            "<pistoris.animation.GroupCollection len=0>",
        )
        self.assertEqual(
            repr(pistoris.Level().room_distances),
            "<pistoris.level.RoomDistanceCollection len=0>",
        )
        self.assertEqual(
            repr(pistoris.model.Vertex()),
            "Vertex(position=Vector3(x=0, y=0, z=0), bone=None, selections=<0 items>)",
        )
        self.assertNotIn("ModelVertex", pistoris.__all__)
        self.assertFalse(hasattr(pistoris, "ModelVertex"))
        self.assertIsInstance(pistoris.__version__, str)
        self.assertIsInstance(pistoris.build_time, str)
        self.assertFalse(hasattr(pistoris, "version"))
        self.assertFalse(hasattr(pistoris, "error_string"))
        self.assertFalse(hasattr(pistoris, "SOUND_EFFECTS_LANGUAGE_ID"))
        self.assertFalse(hasattr(pistoris, "SOUND_EFFECTS_LANGUAGE"))

        for name in ("INVALID_INDEX", "NO_SOUND", "NO_SOUND_HANDLE", "NO_TEXTURE"):
            self.assertFalse(hasattr(pistoris, name))
            self.assertFalse(hasattr(pistoris.native, name))
        self.assertEqual(pistoris.native.ftl.Face().texture_id, -1)
        self.assertEqual(pistoris.native.cin.Keyframe().sound, -1)

    def test_path_helpers_use_typed_resource_families(self) -> None:
        paths = pistoris.paths
        self.assertIs(paths, importlib.import_module("pistoris.paths"))
        for removed in ("ModelPath", "AnimationPath", "CinematicPath", "AmbiancePath", "parse_resource_selector"):
            self.assertFalse(hasattr(paths, removed))
        self.assertFalse(callable(paths.ResourceSelector))
        self.assertEqual(paths.selector_from_string("MODEL:npc:human").kind, paths.ResourceKind.MODEL)
        with self.assertRaises(ValueError):
            paths.selector_from_string("model.ftl")
        self.assertTrue(paths.is_portable_filename("wall_[metal].png"))
        self.assertFalse(paths.is_portable_filename("folder/texture.png"))
        self.assertEqual(paths.sanitize_portable_filename("my??__texture.png"), "my_texture.png")
        self.assertTrue(paths.is_portable_resource_path_component("My texture__01.png"))
        self.assertEqual(paths.texture_directory(), "graph/obj3d/textures")
        self.assertEqual(paths.sound_directory(), "sfx")
        self.assertEqual(paths.ambiance_sound_directory(), "sfx/ambiance")
        self.assertEqual(paths.normalize_zone_ambiance(r"Cave\Water.AMB"), "cave/water")
        self.assertEqual(paths.amb_from_zone_ambiance("cave/water"), "sfx/ambiance/cave/water.amb")

        level = paths.LevelSelector(17)
        self.assertEqual(str(level), "level:17")
        self.assertEqual(level.to_path(), "graph/levels/level17/level17.dlf")
        self.assertEqual(level.associated_llf(), "graph/levels/level17/level17.llf")
        self.assertEqual(level.associated_fts(), "game/graph/levels/level17/fast.fts")
        self.assertEqual(paths.LevelSelector.parse("LEVEL:17"), level)
        self.assertEqual(paths.LevelSelector.from_path(level.to_path()), level)
        with self.assertRaises(ValueError):
            paths.LevelSelector.from_path(level.associated_llf())
        with self.assertRaises(ValueError):
            paths.selector_from_path(level.associated_fts())
        self.assertEqual(paths.minimap_resource_level(14), 1)
        self.assertEqual(paths.level_minimap(14), "graph/levels/level1/map")
        self.assertEqual(paths.level_loading_screen(14), "graph/levels/level14/loading")
        self.assertEqual(paths.minimap_offsets_file(), "graph/levels/mini_offsets.ini")
        self.assertEqual(paths.dlf_scene_from_level_name("scene.v2"), "graph/levels/scene.v2")
        self.assertEqual(paths.fts_from_dlf_scene("graph/levels/scene.v2"), "game/graph/levels/scene.v2/fast.fts")
        self.assertEqual(paths.fts_from_dlf_scene("../custom"), "custom/fast.fts")
        with self.assertRaises(ValueError):
            paths.fts_from_dlf_scene("../../custom")

        model = paths.ModelSelector(paths.ModelType.NPC, "human_base.ftl", "skins/red.ftl")
        model_ftl = "game/graph/obj3d/interactive/npc/human_base/tweaks/skins/red.ftl"
        model_class = "graph/obj3d/interactive/npc/human_base/tweaks/skins/red"
        self.assertEqual(model.name, "human_base")
        self.assertEqual(model.tweak, "skins/red")
        self.assertEqual(model.to_path(), model_ftl)
        self.assertEqual(paths.ModelSelector.from_path(Path(model_ftl)), model)
        self.assertEqual(paths.entity_class_from_ftl(model_ftl), model_class)
        self.assertEqual(paths.ftl_from_entity_class(model_class), model_ftl)
        self.assertEqual(paths.entity_class_kind(model_class), paths.EntityClassKind.NPC)
        item_class = "graph/obj3d/interactive/items/armor/chest/chest"
        self.assertEqual(paths.item_icon_from_entity_class(item_class), f"{item_class}[icon]")
        self.assertIsNone(paths.item_icon_from_entity_class(model_class))
        self.assertEqual(paths.entity_class_from_model(model), model_class)
        self.assertEqual(paths.base_entity_class_from_model(model), "graph/obj3d/interactive/npc/human_base/human_base")
        self.assertEqual(paths.model_from_entity_class(model_class), model)
        self.assertEqual(str(model), "model:npc:human_base:skins/red")
        parsed_model = paths.ModelSelector.parse("MODEL:NPC:human_base:skins/red")
        self.assertEqual(parsed_model, model)
        self.assertEqual(hash(parsed_model), hash(model))
        self.assertIsInstance(model, paths.ResourceSelector)
        self.assertEqual(parsed_model.type, paths.ModelType.NPC)
        self.assertEqual(str(parsed_model.type), "npc")
        with self.assertRaisesRegex(AttributeError, "property 'name'"):
            parsed_model.name = "other"
        with self.assertRaises(ValueError):
            paths.ModelSelector(paths.ModelType.NPC, "folder/name")
        with self.assertRaises(ValueError):
            paths.ModelSelector.from_path(model_ftl[:-4])

        animation = paths.AnimationSelector(paths.AnimationType.NPC, "walk.tea")
        animation_tea = "graph/obj3d/anims/npc/walk.tea"
        self.assertEqual(paths.animation_directory(paths.ModelType.ARMOR), "graph/obj3d/anims/fix_inter")
        self.assertEqual(paths.animation_directory(paths.AnimationType.NPC), "graph/obj3d/anims/npc")
        self.assertEqual(animation.to_path(), animation_tea)
        self.assertEqual(paths.AnimationSelector.from_path(animation_tea), animation)
        self.assertEqual(str(animation), "anim:npc:walk")
        self.assertEqual(paths.AnimationSelector.parse("ANIM:NPC:walk"), animation)

        cinematic = paths.CinematicSelector("intro.cin")
        cinematic_cin = "graph/interface/illustrations/intro.cin"
        self.assertEqual(paths.cinematic_illustration_directory(), "graph/interface/illustrations")
        self.assertEqual(cinematic.to_path(), cinematic_cin)
        self.assertEqual(paths.CinematicSelector.from_path(cinematic_cin), cinematic)
        self.assertEqual(str(cinematic), "cinematic:intro")
        self.assertEqual(paths.CinematicSelector.parse("CINEMATIC:intro"), cinematic)

        ambiance = paths.AmbianceSelector("cave/water.amb")
        ambiance_amb = "sfx/ambiance/cave/water.amb"
        self.assertEqual(ambiance.to_path(), ambiance_amb)
        self.assertEqual(paths.AmbianceSelector.from_path(ambiance_amb), ambiance)
        self.assertEqual(str(ambiance), "ambiance:cave/water")
        self.assertEqual(paths.AmbianceSelector.parse("AMBIANCE:cave/water"), ambiance)

        for selector in (model, animation, level, cinematic, ambiance):
            self.assertEqual(paths.selector_from_string(str(selector)), selector)
            self.assertEqual(paths.selector_from_path(selector.to_path()), selector)

        location = paths.model_search_location(paths.ModelType.UI_MENUS)
        self.assertEqual(location.base_path, "game/graph/interface/menus")
        self.assertEqual(location.max_discovery_depth, 1)
        self.assertEqual(paths.animation_search_location(paths.AnimationType.NPC).base_path, "graph/obj3d/anims/npc")
        self.assertEqual(paths.level_search_location().base_path, "graph/levels")
        self.assertEqual(paths.cinematic_search_location().base_path, "graph/interface/illustrations")
        self.assertEqual(paths.ambiance_search_location().base_path, "sfx/ambiance")
        with self.assertRaises(ValueError):
            paths.ModelSelector.parse("model:items:armor:chest")

    def test_small_values_are_immutable_value_objects(self) -> None:
        self.assertFalse(hasattr(pistoris, "Vector3"))
        vector = pistoris.math.Vector3(1.0, 2.0, 3.0)
        self.assertEqual(tuple(vector), (1.0, 2.0, 3.0))
        self.assertEqual(vector[-1], 3.0)
        self.assertEqual(vector, pistoris.math.Vector3(1.0, 2.0, 3.0))
        self.assertEqual(repr(vector), "Vector3(x=1, y=2, z=3)")
        self.assertNotEqual(vector, (1.0, 2.0, 3.0))
        with self.assertRaisesRegex(
            AttributeError, "property 'x' of 'Vector3' object has no setter"
        ):
            vector.x = 4.0
        self.assertEqual(vector + pistoris.math.Vector3(3.0, 2.0, 1.0), pistoris.math.Vector3(4.0, 4.0, 4.0))
        self.assertEqual(vector - pistoris.math.Vector3(1.0, 1.0, 1.0), pistoris.math.Vector3(0.0, 1.0, 2.0))
        self.assertEqual(-vector, pistoris.math.Vector3(-1.0, -2.0, -3.0))
        self.assertEqual(2.0 * vector, pistoris.math.Vector3(2.0, 4.0, 6.0))
        self.assertEqual(vector / 2.0, pistoris.math.Vector3(0.5, 1.0, 1.5))
        self.assertEqual(vector.dot(pistoris.math.Vector3(1.0, 0.0, 0.0)), 1.0)
        self.assertEqual(
            pistoris.math.Vector3(1.0, 0.0, 0.0).cross(pistoris.math.Vector3(0.0, 1.0, 0.0)),
            pistoris.math.Vector3(0.0, 0.0, 1.0),
        )
        self.assertAlmostEqual(vector.length_squared(), 14.0)
        self.assertAlmostEqual(pistoris.math.Vector3(0.0, 3.0, 4.0).length(), 5.0)
        self.assertEqual(
            pistoris.math.Vector3(0.0, 3.0, 4.0).normalized(),
            pistoris.math.Vector3(0.0, 0.6, 0.8),
        )
        with self.assertRaises(ZeroDivisionError):
            _ = vector / 0.0
        with self.assertRaises(ValueError):
            pistoris.math.Vector3().normalized()

        origin = pistoris.model.Origin(bone="root")
        self.assertEqual(repr(origin), "Origin(bone='root', selections=<0 items>)")
        origin.bone = "renamed"
        self.assertEqual(origin.bone, "renamed")

        spawn = pistoris.level.PlayerSpawn(
            position=pistoris.math.Vector3(1.0, 2.0, 3.0),
            rotation=pistoris.math.Quat(1.0, 0.0, 0.0, 0.0),
        )
        with self.assertRaisesRegex(
            AttributeError,
            "property 'position' of 'PlayerSpawn' object has no setter",
        ):
            spawn.position = pistoris.math.Vector3()
        self.assertFalse(hasattr(spawn, "is_usable"))

        equal_pairs = (
            (pistoris.math.Vector2(1.0, 2.0), pistoris.math.Vector2(1.0, 2.0)),
            (vector, pistoris.math.Vector3(1.0, 2.0, 3.0)),
            (pistoris.math.Angle(1.0, 2.0, 3.0), pistoris.math.Angle(1.0, 2.0, 3.0)),
            (
                pistoris.math.Rect(pistoris.math.Vector2(1.0, 2.0), pistoris.math.Vector2(3.0, 4.0)),
                pistoris.math.Rect(pistoris.math.Vector2(1.0, 2.0), pistoris.math.Vector2(3.0, 4.0)),
            ),
            (
                pistoris.math.Aabb(pistoris.math.Vector3(1.0, 2.0, 3.0), pistoris.math.Vector3(4.0, 5.0, 6.0)),
                pistoris.math.Aabb(pistoris.math.Vector3(1.0, 2.0, 3.0), pistoris.math.Vector3(4.0, 5.0, 6.0)),
            ),
            (pistoris.math.Color3(0.1, 0.2, 0.3), pistoris.math.Color3(0.1, 0.2, 0.3)),
            (pistoris.math.Quat(1.0, 0.1, 0.2, 0.3), pistoris.math.Quat(1.0, 0.1, 0.2, 0.3)),
            (
                pistoris.level.ZoneAmbiance(name="cave", volume=50.0),
                pistoris.level.ZoneAmbiance(name="cave", volume=50.0),
            ),
            (
                spawn,
                pistoris.level.PlayerSpawn(
                    position=pistoris.math.Vector3(1.0, 2.0, 3.0),
                    rotation=pistoris.math.Quat(1.0, 0.0, 0.0, 0.0),
                ),
            ),
        )
        for left, right in equal_pairs:
            with self.subTest(value=type(left).__name__):
                self.assertEqual(left, right)
                self.assertEqual(hash(left), hash(right))
                self.assertEqual(len({left, right}), 1)
                self.assertNotEqual(left, object())

    def test_domain_records_are_keyword_only_and_use_none(self) -> None:
        vertex = pistoris.model.Vertex(position=pistoris.math.Vector3(1.0, 2.0, 3.0))
        self.assertIsNone(vertex.bone)
        self.assertIsNone(pistoris.model.Face().texture)
        self.assertIsNone(pistoris.cinematic.Keyframe().sound_path)
        self.assertIsNone(pistoris.animation.Keyframe().sound_path)
        self.assertIsNone(pistoris.ambiance.PannedTrack().sound_path)
        self.assertIsNone(pistoris.ambiance.PositionedTrack().sound_path)
        portal = pistoris.level.Portal()
        self.assertIsNone(portal.room_1)
        self.assertIsNone(portal.room_2)
        portal.room_1 = "room 2"
        self.assertEqual(portal.room_1, "room-2")
        portal.room_1 = None
        self.assertIsNone(portal.room_1)
        selection = pistoris.model.Selection(name="CUT_HEAD")
        self.assertEqual(selection.name, "cut_head")
        self.assertEqual(selection, pistoris.model.Selection(name="cut_head"))
        self.assertEqual(hash(selection), hash(pistoris.model.Selection(name="CUT_HEAD")))
        with self.assertRaises(AttributeError):
            selection.name = "body"
        leading_vertex = pistoris.model.SelectionLeadingVertex(
            position=pistoris.math.Vector3(1.0, 2.0, 3.0), bone="root"
        )
        self.assertEqual(leading_vertex.position, pistoris.math.Vector3(1.0, 2.0, 3.0))
        self.assertEqual(leading_vertex.bone, "root")
        with self.assertRaises(TypeError):
            pistoris.model.Vertex(pistoris.math.Vector3())

    def test_detached_semantic_identities_are_canonical(self) -> None:
        self.assertEqual(pistoris.Texture(path=r"Textures\Door.PNG").path, "textures/door.png")
        self.assertEqual(pistoris.Sound(path=r"SFX\Door.WAV").path, "sfx/door.wav")

        vertex = pistoris.model.Vertex(bone="Root Bone")
        self.assertEqual(vertex.bone, "root-bone")
        face = pistoris.model.Face(texture=r"Textures\Door.PNG")
        self.assertEqual(face.texture, "textures/door.png")
        bone = pistoris.model.Bone(name="Root Bone", parent="Parent Bone")
        self.assertEqual((bone.name, bone.parent), ("root-bone", "parent-bone"))

        portal = pistoris.level.Portal(name="Main Portal", room_1="Room One")
        self.assertEqual((portal.name, portal.room_1), ("Main-Portal", "Room-One"))
        anchor = pistoris.level.Anchor()
        self.assertEqual(anchor.name, "")
        self.assertEqual(pistoris.level.Light(name="Main Light").name, "Main-Light")
        entity = pistoris.level.Entity(class_path=r"ITEMS\Test.FTL", name="Main Entity")
        self.assertEqual((entity.class_path, entity.name), ("items/test", "Main-Entity"))
        self.assertEqual(pistoris.level.Fog().name, "")
        self.assertEqual(pistoris.level.Fog(name="Main Fog").name, "Main-Fog")
        self.assertEqual(pistoris.level.Zone(name="Main Zone").name, "main-zone")
        self.assertEqual(pistoris.level.Path(name="Main Path").name, "main-path")
        ambiance = pistoris.level.ZoneAmbiance(name=r"Ambient\Cave.AMB")
        self.assertEqual(ambiance.name, "ambient/cave")

        sound = pistoris.Sound(path=r"SFX\Door.WAV")
        language = pistoris.cinematic.Language(name="English Voice")
        self.assertEqual(sound.path, "sfx/door.wav")
        self.assertEqual(language.name, "english-voice")
        sound.path = r"VOICE\Greeting.OGG"
        language.name = "French Voice"
        self.assertEqual(sound.path, "voice/greeting.ogg")
        self.assertEqual(language.name, "french-voice")

    def test_detached_face_vertices_are_live_nested_references(self) -> None:
        selection = pistoris.model.Selection(name="Upper Body")
        face = pistoris.model.Face()
        vertex = face.corners[0].vertex
        self.assertIsInstance(vertex, pistoris.model.FaceCornerVertexRef)
        vertex.position = pistoris.math.Vector3(1.0, 2.0, 3.0)
        vertex.bone = "Root Bone"
        vertex.selections.add(selection)
        self.assertEqual(face.corners[0].vertex.position, pistoris.math.Vector3(1.0, 2.0, 3.0))
        self.assertEqual(face.corners[0].vertex.bone, "root-bone")
        self.assertEqual(set(face.corners[0].vertex.selections), {selection})

        level_face = pistoris.level.Face()
        level_vertex = level_face.corners[0].vertex
        self.assertIsInstance(level_vertex, pistoris.level.FaceCornerVertexRef)
        level_vertex.position = pistoris.math.Vector3(4.0, 5.0, 6.0)
        self.assertEqual(level_face.corners[0].vertex.position, pistoris.math.Vector3(4.0, 5.0, 6.0))

    def test_zone_ambiance_is_an_immutable_value_for_snapshots_and_live_zones(self) -> None:
        ambiance = pistoris.level.ZoneAmbiance(name="cave", volume=50.0)
        self.assertEqual(ambiance.name, "cave")
        self.assertEqual(ambiance.volume, 50.0)
        self.assertEqual(repr(ambiance), "ZoneAmbiance(name='cave', volume=50.0)")
        with self.assertRaises(AttributeError):
            ambiance.volume = 25.0

        zone = pistoris.level.Zone(
            perimeter_xz=[
                pistoris.math.Vector2(0.0, 0.0),
                pistoris.math.Vector2(1.0, 0.0),
                pistoris.math.Vector2(0.0, 1.0),
            ],
            height=1.0,
            ambiance=ambiance,
        )
        self.assertEqual(zone.ambiance, ambiance)

        level = pistoris.Level()
        level.zones.append(zone)
        zone_index = 0
        self.assertEqual(level.zones[zone_index].ambiance, ambiance)
        replacement = pistoris.level.ZoneAmbiance(name="crypt", volume=75.0)
        level.zones[zone_index].ambiance = replacement
        self.assertEqual(level.zones[zone_index].ambiance, replacement)
        level.zones[zone_index].ambiance = None
        self.assertIsNone(level.zones[zone_index].ambiance)

    def test_binary_inputs_accept_contiguous_readable_buffers(self) -> None:
        source = bytearray(
            fixture("mount/game/graph/obj3d/interactive/npc/human_male/human_male.ftl")
        )
        model = pistoris.Model.from_ftl_bytes(memoryview(source)).model
        self.assertGreater(len(model.mesh.vertices), 0)

        sound = pistoris.Sound(encoded_audio=memoryview(b"audio"))
        self.assertEqual(sound.encoded_audio, b"audio")

        level = pistoris.Level()
        level.mesh.textures.append(pistoris.Texture(path="test"))
        texture_index = 0
        encoded_image = fixture("mount/graph/obj3d/textures/sword02_00.png")
        level.mesh.textures[texture_index].encoded_image = memoryview(encoded_image)
        self.assertEqual(level.mesh.textures[texture_index].encoded_image, encoded_image)

        self.assertIs(pistoris.classify_text_encoding(bytearray(b"ascii")), pistoris.TextEncoding.ASCII)
        with self.assertRaises(BufferError):
            pistoris.classify_text_encoding(memoryview(bytearray(b"abcd"))[::2])

    def test_nested_resource_fields_are_live_views(self) -> None:
        model = pistoris.Model.from_ftl_bytes(
            fixture("mount/game/graph/obj3d/interactive/npc/human_male/human_male.ftl")
        ).model
        self.assertEqual(repr(model.mesh.vertices[0]), "<pistoris.model.VertexRef>")
        self.assertIn(" path=", repr(model.mesh.textures[0]))
        self.assertNotIn(" name=", repr(model.mesh.textures[0]))
        self.assertIn(" name=", repr(model.skeleton.bones[0]))
        corner = model.mesh.faces[0].corners[0]
        self.assertEqual(repr(corner), "<pistoris.model.FaceCornerRef index=0>")
        corner.u += 0.125
        self.assertEqual(model.mesh.faces[0].corners[0].u, corner.u)

        ambiance = pistoris.Ambiance()
        ambiance.sounds.append(pistoris.Sound(path="sfx/test"))
        sound_path = ambiance.sounds[0].path
        key = pistoris.ambiance.PannedKey(
            play_count=1,
            volume=pistoris.ambiance.Automation(first=1.0, second=1.0),
            pitch=pistoris.ambiance.Automation(first=1.0, second=1.0),
        )
        ambiance.tracks.append(
            pistoris.ambiance.PannedTrack(sound_path=sound_path, keys=[key])
        )
        track = 0
        keys = ambiance.tracks[track].keys
        volume = keys[0].volume
        self.assertEqual(repr(volume), "<pistoris.ambiance.AutomationRef>")
        volume.first = 0.5
        self.assertEqual(ambiance.tracks[track].keys[0].volume.first, 0.5)

        replacement_key = key
        replacement_key.play_count = 2
        keys[0] = replacement_key
        self.assertEqual(ambiance.tracks[track].keys[0].play_count, 2)

        ambiance.tracks.append(
            pistoris.ambiance.PositionedTrack(
                sound_path=sound_path, keys=[pistoris.ambiance.PositionedKey()]
            )
        )
        positioned_track = 1
        self.assertIsInstance(
            ambiance.tracks[positioned_track].keys,
            pistoris.ambiance.KeyCollection,
        )

        level = pistoris.Level()
        level.entities.append(pistoris.level.Entity(class_path="items/test"))
        self.assertIn(" class_path='items/test'", repr(level.entities[0]))
        path = pistoris.level.Path(
            name="test",
            nodes=[pistoris.level.PathNode(), pistoris.level.PathNode(time_ms=100)],
        )
        level.paths.append(path)
        path_index = 0
        path_ref = level.paths[path_index]
        node = path_ref.nodes[1]
        node.time_ms = 250
        self.assertEqual(path_ref.nodes[1].time_ms, 250)

        path_ref.name = "renamed"
        self.assertEqual(node.time_ms, 250)

        replacement_node = pistoris.level.PathNode(time_ms=300)
        path_ref.nodes[1] = replacement_node
        self.assertEqual(node.time_ms, 300)

        level.paths[path_index] = pistoris.level.Path(
            name="replacement",
            nodes=[pistoris.level.PathNode(), replacement_node],
        )
        with self.assertRaises(ReferenceError):
            _ = node.time_ms
        self.assertEqual(path_ref.name, "replacement")

        level.paths.append(
            pistoris.level.Path(
                name="shifted",
                nodes=[pistoris.level.PathNode(), pistoris.level.PathNode(time_ms=400)],
            )
        )
        shifted_path = 1
        shifted_node = level.paths[shifted_path].nodes[1]
        del level.paths[path_index]
        self.assertEqual(shifted_node.time_ms, 400)

        level.zones.append(
            pistoris.level.Zone(
                perimeter_xz=[
                    pistoris.math.Vector2(0.0, 0.0),
                    pistoris.math.Vector2(1.0, 0.0),
                    pistoris.math.Vector2(0.0, 1.0),
                ],
                height=1.0,
            )
        )
        zone_index = 0
        zone = level.zones[zone_index]
        self.assertIsInstance(zone.perimeter_xz, tuple)
        with self.assertRaises(AttributeError):
            zone.perimeter_xz.append(pistoris.math.Vector2())
        zone.perimeter_xz = [
            pistoris.math.Vector2(0.0, 0.0),
            pistoris.math.Vector2(2.0, 0.0),
            pistoris.math.Vector2(0.0, 2.0),
        ]
        self.assertEqual(zone.perimeter_xz[1], pistoris.math.Vector2(2.0, 0.0))

    def test_ambiance_track_kind_conversion_preserves_logical_references(self) -> None:
        ambiance = pistoris.Ambiance()
        ambiance.sounds.append(pistoris.Sound(path="sfx/test"))
        sound_path = ambiance.sounds[0].path
        key_value = pistoris.ambiance.PannedKey(
            start_delay_ms=125,
            play_count=3,
            delay_min_ms=250,
            delay_max_ms=500,
            volume=pistoris.ambiance.Automation(
                first=0.75,
                second=0.5,
                interval_ms=1_000,
                mode=pistoris.ambiance.AutomationMode.INTERPOLATED,
            ),
            pitch=pistoris.ambiance.Automation(first=1.0, second=1.25),
            pan=pistoris.ambiance.Automation(first=-0.5, second=0.5),
        )
        track_value = pistoris.ambiance.PannedTrack(
            sound_path=sound_path, keys=[key_value]
        )
        self.assertEqual(len(track_value.keys), 1)
        ambiance.tracks.append(track_value)
        track_index = 0

        track = ambiance.tracks[track_index]
        keys = track.keys
        key = keys[0]
        volume = key.volume
        pan = key.pan
        self.assertIsNotNone(pan)
        with self.assertRaises(AttributeError):
            key.x = pistoris.ambiance.Automation()
        with self.assertRaises(TypeError):
            keys[0] = pistoris.ambiance.PositionedKey()

        track.kind = pistoris.ambiance.TrackKind.POSITIONED
        self.assertIs(track.kind, pistoris.ambiance.TrackKind.POSITIONED)
        self.assertIsInstance(track.copy(), pistoris.ambiance.PositionedTrack)
        self.assertIsInstance(key.copy(), pistoris.ambiance.PositionedKey)
        self.assertEqual(key.start_delay_ms, 125)
        self.assertEqual(key.play_count, 3)
        self.assertEqual(volume.first, 0.75)
        self.assertIsNone(key.pan)
        self.assertIsNotNone(key.x)
        self.assertEqual(key.x.first, 0.0)
        with self.assertRaises(AttributeError):
            key.pan = pistoris.ambiance.Automation()
        with self.assertRaises(ReferenceError):
            _ = pan.first
        self.assertFalse(pan == pan)

        volume.second = 0.625
        self.assertEqual(key.volume.second, 0.625)
        x = key.x
        self.assertIsNotNone(x)

        track.kind = pistoris.ambiance.TrackKind.PANNED
        self.assertIsInstance(key.copy(), pistoris.ambiance.PannedKey)
        self.assertIsNone(key.x)
        self.assertIsNotNone(key.pan)
        self.assertEqual(key.pan.first, 0.0)
        self.assertEqual(volume.second, 0.625)
        with self.assertRaises(ReferenceError):
            _ = x.first
        self.assertFalse(x == x)

        ambiance.tracks[track_index] = pistoris.ambiance.PannedTrack(
            sound_path=sound_path, keys=[key_value]
        )
        self.assertIsInstance(track.copy(), pistoris.ambiance.PannedTrack)
        with self.assertRaises(ReferenceError):
            _ = key.play_count
        with self.assertRaises(ReferenceError):
            _ = volume.first

    def test_collection_slices_preserve_reference_semantics(self) -> None:
        model = pistoris.Model.from_ftl_bytes(
            fixture("mount/game/graph/obj3d/interactive/npc/human_male/human_male.ftl")
        ).model
        faces = model.mesh.faces[:2]
        self.assertEqual(len(faces), 2)
        faces[0].transval = 0.25
        self.assertEqual(model.mesh.faces[0].transval, 0.25)

        sidecars = model.to_ftl_bytes().texture_files
        sliced = sidecars[:1]
        self.assertIsInstance(sliced, pistoris.TextureFileSequence)
        self.assertEqual(len(sliced), min(1, len(sidecars)))
        if sidecars:
            self.assertEqual(sidecars[0].source_path, model.mesh.textures[0].path)
            with self.assertRaises(AttributeError):
                sidecars[0].source_path = "changed"


class BindingRegressionTests(unittest.TestCase):
    def test_face_replacement_compares_selection_membership_semantically(self) -> None:
        model = pistoris.Model()
        model.skeleton.bones.append(pistoris.model.Bone(name="root"))
        first = model.selections.add(pistoris.model.Selection(name="first"))
        second = model.selections.add(pistoris.model.Selection(name="second"))
        normal = pistoris.math.Vector3(0.0, 1.0, 0.0)
        vertices = [
            pistoris.model.Vertex(
                position=position,
                bone="root",
                selections=[pistoris.model.Selection(name="first"), pistoris.model.Selection(name="second")],
            )
            for position in (
                pistoris.math.Vector3(0.0, 0.0, 0.0),
                pistoris.math.Vector3(1.0, 0.0, 0.0),
                pistoris.math.Vector3(0.0, 0.0, 1.0),
            )
        ]
        model.mesh.faces.append(
            pistoris.model.Face(
                corners=[pistoris.model.Corner(vertex=vertex, normal=normal) for vertex in vertices],
                normal=normal,
            )
        )
        self.assertEqual(len(model.mesh.vertices), 3)

        replacement = model.mesh.faces[0].copy()
        for corner in replacement.corners:
            corner.vertex.selections = [pistoris.model.Selection(name="second"), pistoris.model.Selection(name="first")]
        model.mesh.faces[0] = replacement

        self.assertEqual(len(model.mesh.vertices), 3)
        self.assertEqual(set(model.mesh.vertices[0].selections), {first, second})

    def test_detached_relationships_resolve_by_semantic_identity(self) -> None:
        model = pistoris.Model()
        model.skeleton.bones.append(pistoris.model.Bone(name="root"))
        selection = model.selections.add(pistoris.model.Selection(name="selected"))
        model.mesh.textures.append(pistoris.Texture(path="textures/test"))
        normal = pistoris.math.Vector3(0.0, 1.0, 0.0)
        selected = pistoris.model.Selection(name="selected")
        vertices = (
            pistoris.model.Vertex(
                position=pistoris.math.Vector3(0.0, 0.0, 0.0), bone="root", selections=[selected]
            ),
            pistoris.model.Vertex(
                position=pistoris.math.Vector3(1.0, 0.0, 0.0), bone="root", selections=[selected]
            ),
            pistoris.model.Vertex(
                position=pistoris.math.Vector3(0.0, 0.0, 1.0), bone="root", selections=[selected]
            ),
        )
        face = pistoris.model.Face(
            corners=[pistoris.model.Corner(vertex=vertex, normal=normal) for vertex in vertices],
            normal=normal,
            texture="textures/test",
        )

        model.mesh.faces.append(face)

        self.assertEqual(len(model.mesh.vertices), 3)
        self.assertEqual(model.mesh.faces[0].texture, model.mesh.textures[0])
        self.assertEqual(model.mesh.faces[0].corners[0].vertex.bone, model.skeleton.bones[0])
        self.assertEqual(selection.vertices, tuple(model.mesh.vertices))

        stale_vertex = model.mesh.vertices[0]
        model.mesh.vertices.append(
            pistoris.model.Vertex(
                position=pistoris.math.Vector3(0.001, 0.0, 0.0), bone="root", selections=[selected]
            )
        )
        model.mesh.weld_vertices(radius=0.01)
        self.assertEqual(len(model.mesh.vertices), 3)
        with self.assertRaises(ReferenceError):
            _ = stale_vertex.position

        vertex_count = len(model.mesh.vertices)
        with self.assertRaises(KeyError):
            model.mesh.vertices.append(pistoris.model.Vertex(bone="missing"))
        self.assertEqual(len(model.mesh.vertices), vertex_count)

        level = pistoris.Level()
        face = pistoris.level.Face(
            corners=[
                pistoris.level.Corner(vertex=pistoris.level.Vertex(position=position), normal=normal)
                for position in (
                    pistoris.math.Vector3(0.0, 0.0, 0.0),
                    pistoris.math.Vector3(1.0, 0.0, 0.0),
                    pistoris.math.Vector3(0.0, 0.0, 1.0),
                )
            ]
        )
        self.assertIsNone(face.room)
        level.rooms.append(pistoris.level.Room(name="room"))
        face.room = "room"
        level.mesh.faces.append(face)
        self.assertEqual(level.mesh.faces[0].room, level.rooms[0])
        self.assertEqual(level.mesh.faces[0].copy().room, "room")

        level.anchors.append(pistoris.level.Anchor())
        self.assertEqual(level.anchors[0].name, "anchor_0")

    def test_semantic_state_is_fully_editable_and_inspectable(self) -> None:
        model = pistoris.Model()
        model.mesh.vertices.extend((pistoris.model.Vertex(), pistoris.model.Vertex()))
        first_vertex = 0
        second_vertex = 1
        model.skeleton.bones.append(pistoris.model.Bone(name="root"))
        bone = model.skeleton.bones[0]
        self.assertEqual(model.skeleton.bones.by_name("ROOT"), bone)
        with self.assertRaises(KeyError):
            model.skeleton.bones.by_name("missing")
        action_point_value = pistoris.model.ActionPoint(name="attach", bone="root")
        model.action_points.append(action_point_value)
        action_point = model.action_points[0]
        selection_ref = model.selections.add(pistoris.model.Selection(name="test"))
        with self.assertRaises(AttributeError):
            _ = selection_ref.index
        self.assertIsNone(selection_ref.leading_vertex)
        selection_ref.leading_vertex = pistoris.model.SelectionLeadingVertex(
            position=pistoris.math.Vector3(1.0, 2.0, 3.0), bone="root"
        )
        leading_vertex = selection_ref.leading_vertex
        self.assertIsNotNone(leading_vertex)
        assert leading_vertex is not None
        self.assertEqual(leading_vertex.position, pistoris.math.Vector3(1.0, 2.0, 3.0))
        self.assertEqual(leading_vertex.bone, bone)
        self.assertEqual(leading_vertex.selection, selection_ref)
        self.assertEqual(leading_vertex.copy().bone, "root")
        model.origin.selections.add(selection_ref)
        self.assertIs(selection_ref.includes_origin, True)
        model.mesh.vertices[first_vertex].selections.add(selection_ref)
        bone.selections.add(selection_ref)
        action_point.selections.add(selection_ref)
        self.assertEqual(selection_ref.vertices, (model.mesh.vertices[first_vertex],))
        self.assertEqual(selection_ref.bones, (bone,))
        self.assertEqual(selection_ref.action_points, (action_point,))
        model.mesh.vertices[second_vertex].selections.add(selection_ref)
        model.mesh.vertices[first_vertex].selections.discard(selection_ref)
        bone.selections.clear()
        self.assertEqual(selection_ref.vertices, (model.mesh.vertices[second_vertex],))
        self.assertEqual(selection_ref.bones, ())
        self.assertFalse(hasattr(selection_ref.copy(), "vertices"))

        model.origin.bone = bone
        self.assertEqual(model.origin.bone, bone)

        model.mesh.vertices[second_vertex].bone = bone
        model.skeleton.replace([pistoris.model.Bone(name="replacement")])
        self.assertIsNone(model.mesh.vertices[second_vertex].bone)
        self.assertIsNone(model.origin.bone)
        self.assertIsNone(model.action_points[0].bone)
        replaced_leading_vertex = selection_ref.leading_vertex
        self.assertIsNotNone(replaced_leading_vertex)
        assert replaced_leading_vertex is not None
        self.assertIsNone(replaced_leading_vertex.bone)
        self.assertTrue(selection_ref.includes_origin)
        self.assertEqual(model.skeleton.bones.by_name("replacement"), model.skeleton.bones[0])

        animation = pistoris.Animation.from_tea_bytes(
            fixture("mount/graph/obj3d/anims/fix_inter/json_dice.tea")
        ).animation
        group = animation.groups[0]
        group.claimed = True
        self.assertTrue(group.claimed)
        self.assertFalse(group.is_void)
        group.make_void()
        self.assertFalse(group.claimed)
        self.assertTrue(group.is_void)
        animation.frame_length += 1
        self.assertGreater(animation.frame_length, 0)

        level = pistoris.Level()
        self.assertIsNone(level.player_spawn)
        spawn = pistoris.level.PlayerSpawn(position=pistoris.math.Vector3(1.0, 2.0, 3.0))
        level.player_spawn = spawn
        spawn_ref = level.player_spawn
        self.assertIsNotNone(spawn_ref)
        assert spawn_ref is not None
        same_spawn_ref = level.player_spawn
        self.assertIsNotNone(same_spawn_ref)
        assert same_spawn_ref is not None
        self.assertIsNot(spawn_ref, same_spawn_ref)
        self.assertEqual(spawn_ref, same_spawn_ref)
        self.assertEqual(spawn_ref.copy(), spawn)
        spawn_ref.position = pistoris.math.Vector3(4.0, 5.0, 6.0)
        self.assertEqual(spawn_ref.position, pistoris.math.Vector3(4.0, 5.0, 6.0))
        level.player_spawn = None
        self.assertIsNone(level.player_spawn)
        self.assertNotEqual(spawn_ref, same_spawn_ref)
        with self.assertRaises(ReferenceError):
            spawn_ref.validate()

        self.assertIsNone(level.loading_screen.encoded_image)
        loading_screen = fixture("mount/graph/levels/level9/loading.png")
        level.loading_screen.encoded_image = memoryview(loading_screen)
        self.assertEqual(level.loading_screen.encoded_image, loading_screen)
        level.loading_screen.encoded_image = None
        self.assertIsNone(level.loading_screen.encoded_image)
        self.assertIsNone(level.minimap.encoded_image)

        nav_surface = level.nav_surface.info
        self.assertFalse(nav_surface.has_surface)
        self.assertEqual(nav_surface.vertex_count, 0)
        self.assertEqual(nav_surface.triangle_count, 0)

    def test_optional_media_inspection_uses_none_for_absence(self) -> None:
        texture = pistoris.Texture()
        sound = pistoris.Sound()
        sampler = pistoris.level.MinimapSampler()
        self.assertIsNone(texture.encoded_image)
        self.assertIsNone(sound.encoded_audio)
        self.assertIsNone(sampler.encoded_image)
        texture.encoded_image = b"image"
        sound.encoded_audio = b"audio"
        sampler.encoded_image = b"sampler"
        texture.encoded_image = None
        sound.encoded_audio = None
        sampler.encoded_image = None
        self.assertIsNone(texture.encoded_image)
        self.assertIsNone(sound.encoded_audio)
        self.assertIsNone(sampler.encoded_image)
        for value, attribute in (
            (texture, "encoded_image"),
            (sound, "encoded_audio"),
            (sampler, "encoded_image"),
        ):
            with self.assertRaises(ValueError):
                setattr(value, attribute, b"")
        with self.assertRaises(ValueError):
            pistoris.Texture(encoded_image=b"")
        with self.assertRaises(ValueError):
            pistoris.Sound(encoded_audio=b"")
        with self.assertRaises(ValueError):
            pistoris.level.MinimapSampler(encoded_image=b"")

        model = pistoris.Model()
        self.assertIsNone(model.inventory_icon.copy())
        icon = fixture("mount/graph/obj3d/interactive/items/weapons/sword_00/sword_00[icon].png")
        model.inventory_icon.set(pistoris.model.InventoryIcon(encoded_image=icon))
        self.assertIsInstance(model.inventory_icon.copy(), pistoris.model.InventoryIcon)
        model.inventory_icon.clear()
        self.assertIsNone(model.inventory_icon.copy())

        model.mesh.textures.append(pistoris.Texture(path="test"))
        model_texture = model.mesh.textures[0]
        self.assertIsNone(model_texture.encoded_image)
        model_texture.encoded_image = icon
        model_texture.encoded_image = None
        self.assertIsNone(model_texture.encoded_image)

        level = pistoris.Level()
        self.assertIsNone(level.minimap.copy())
        minimap = fixture("mount/graph/levels/level9/map.png")
        bounds = pistoris.math.Rect(
            pistoris.math.Vector2(0.0, 0.0), pistoris.math.Vector2(1.0, 1.0)
        )
        level.minimap.set(minimap, bounds)
        self.assertIsInstance(level.minimap.copy(), pistoris.level.Minimap)
        level.minimap.clear()
        self.assertIsNone(level.minimap.copy())

        level.mesh.textures.append(pistoris.Texture(path="test"))
        level_texture = level.mesh.textures[0]
        self.assertIsNone(level_texture.encoded_image)
        level_texture.encoded_image = minimap
        level_texture.encoded_image = None
        self.assertIsNone(level_texture.encoded_image)

        animation = pistoris.Animation()
        animation.sounds.append(pistoris.Sound(path="test"))
        animation_sound = animation.sounds[0]
        self.assertIsNone(animation_sound.encoded_audio)
        audio = fixture("mount/speech/english/one.wav")
        animation_sound.encoded_audio = audio
        animation_sound.encoded_audio = None
        self.assertIsNone(animation_sound.encoded_audio)

        ambiance = pistoris.Ambiance()
        ambiance.sounds.append(pistoris.Sound(path="test"))
        ambiance_sound = ambiance.sounds[0]
        self.assertIsNone(ambiance_sound.encoded_audio)
        ambiance_sound.encoded_audio = audio
        ambiance_sound.encoded_audio = None
        self.assertIsNone(ambiance_sound.encoded_audio)

        cinematic = pistoris.Cinematic()
        cinematic.illustrations.append(pistoris.cinematic.Illustration(path="test"))
        illustration = cinematic.illustrations[0]
        self.assertIsNone(illustration.encoded_image)
        illustration.encoded_image = minimap
        illustration.encoded_image = None
        self.assertIsNone(illustration.encoded_image)

    def test_cinematic_timeline_properties_are_writable(self) -> None:
        cinematic = pistoris.Cinematic()
        cinematic.end_frame = 90
        cinematic.fps = 30.0
        self.assertEqual(cinematic.end_frame, 90)
        self.assertEqual(cinematic.fps, 30.0)

        previous = (cinematic.end_frame, cinematic.fps)
        with self.assertRaises(pistoris.PistorisError):
            cinematic.fps = 0.0
        self.assertEqual((cinematic.end_frame, cinematic.fps), previous)

    def test_model_glb_report_without_sound_files(self) -> None:
        glb, units = catalog_glb_fixture("models", "human_male")
        imported = pistoris.Model.from_glb(
            glb,
            arx_units_per_glb_unit=units,
        )
        self.assertGreater(len(imported.animations), 0)
        self.assertEqual(len(imported.sound_sources), 2)
        self.assertIsNotNone(imported.animation_report)
        for sound_source in imported.sound_sources:
            self.assertIsInstance(sound_source, pistoris.animation.SoundSource)
            self.assertIsInstance(sound_source.source, pistoris.SoundSourceReference)
            self.assertTrue(any(sound_source.animation is animation for animation in imported.animations))

        imported.animations[0].name = "mutable_imported_animation"
        self.assertEqual(imported.animations[0].name, "mutable_imported_animation")

        output = imported.model.to_glb(imported.animations, include_sidecars=False)
        self.assertEqual(len(output.sound_files), 0)
        self.assertEqual(
            repr(output.sound_files),
            "<pistoris.animation.SoundFileSequence len=0>",
        )
        self.assertGreater(output.animation_report.converted + output.animation_report.skipped, 0)

    def test_animation_conversion_reports_are_output_only(self) -> None:
        with self.assertRaises(TypeError):
            pistoris.animation.ConversionReport()

    def test_resource_collections_are_live_and_track_element_identity(self) -> None:
        model = pistoris.Model.from_ftl_bytes(
            fixture("mount/game/graph/obj3d/interactive/npc/human_male/human_male.ftl")
        ).model

        texture = model.mesh.textures[0]
        texture.path = "custom/live_texture"
        self.assertEqual(model.mesh.textures[0].path, "custom/live_texture")

        detached = texture.copy()
        detached.path = "custom/detached"
        self.assertEqual(texture.path, "custom/live_texture")

        removed = model.mesh.faces[0]
        shifted = model.mesh.faces[1]
        self.assertEqual(model.mesh.faces[-1].index, len(model.mesh.faces) - 1)
        with self.assertRaises(IndexError):
            _ = model.mesh.faces[len(model.mesh.faces)]

        del model.mesh.faces[0]
        with self.assertRaises(ReferenceError):
            _ = removed.index
        self.assertEqual(shifted.index, 0)

        model.reset()
        with self.assertRaises(ReferenceError):
            _ = texture.path

        animation = pistoris.Animation.from_tea_bytes(
            fixture("mount/graph/obj3d/anims/npc/human_male_gathering.tea")
        ).animation
        removed_transforms = animation.keyframes[0].group_transforms
        removed_transform = removed_transforms[0]
        shifted_transforms = animation.keyframes[1].group_transforms
        shifted_transform = shifted_transforms[0]
        expected_translation = shifted_transform.translation

        replacement_transform = removed_transform.copy()
        replacement_transform.scale = pistoris.math.Vector3(2.0, 2.0, 2.0)
        removed_transforms[0] = replacement_transform
        self.assertEqual(removed_transform.scale, replacement_transform.scale)

        del animation.keyframes[0]
        with self.assertRaises(ReferenceError):
            _ = removed_transform.index
        self.assertEqual(shifted_transform.index, 0)
        self.assertEqual(len(shifted_transforms), len(animation.groups))
        self.assertEqual(shifted_transform.translation.x, expected_translation.x)
        self.assertEqual(shifted_transform.translation.y, expected_translation.y)
        self.assertEqual(shifted_transform.translation.z, expected_translation.z)

    def test_live_references_keep_the_resource_alive(self) -> None:
        texture = pistoris.Model.from_ftl_bytes(
            fixture("mount/game/graph/obj3d/interactive/npc/human_male/human_male.ftl")
        ).model.mesh.textures[0]
        gc.collect()
        self.assertTrue(texture.path)

    def test_selection_memberships_are_live_mutable_sets(self) -> None:
        body = pistoris.model.Selection(name="BODY")
        hands = pistoris.model.Selection(name="hands")
        vertex = pistoris.model.Vertex(selections=[body])
        selections = vertex.selections

        self.assertIsInstance(selections, MutableSet)
        self.assertNotIn("body", selections)
        selections.add(hands)
        self.assertEqual(set(vertex.selections), {body, hands})

        head = pistoris.model.Selection(name="head")
        expected = {body, hands}
        self.assertEqual(selections, expected)
        self.assertEqual(expected, selections)
        self.assertLessEqual(selections, expected)
        self.assertLess(selections, expected | {head})
        self.assertGreaterEqual(selections, {body})
        self.assertGreater(selections, {body})
        self.assertEqual(selections & {hands, head}, {hands})
        self.assertEqual(selections | {head}, expected | {head})
        self.assertEqual(selections - {hands}, {body})
        self.assertEqual(selections ^ {hands, head}, {body, head})
        self.assertEqual({hands, head} & selections, {hands})
        self.assertEqual({head} | selections, expected | {head})
        self.assertEqual({body, head} - selections, {head})
        self.assertEqual({hands, head} ^ selections, {body, head})

        selections |= {head}
        self.assertEqual(selections, {body, hands, head})
        selections &= {body, hands}
        self.assertEqual(selections, {body, hands})
        selections ^= {hands, head}
        self.assertEqual(selections, {body, head})
        selections -= {body}
        self.assertEqual(selections, {head})

        vertex.selections = (head, head)
        self.assertEqual(set(selections), {head})
        self.assertEqual(selections.pop(), head)
        self.assertEqual(set(vertex.selections), set())

        model = pistoris.Model()
        model.mesh.vertices.append(pistoris.model.Vertex())
        selection = model.selections.add(pistoris.model.Selection(name="selected"))
        membership = model.mesh.vertices[0].selections
        self.assertIsInstance(membership, MutableSet)
        self.assertNotIn("selected", membership)
        with self.assertRaises(TypeError):
            hash(membership)
        membership.add(selection)
        self.assertIn(selection, membership)
        self.assertEqual(membership, {selection})
        self.assertEqual(membership & {selection}, {selection})
        membership.remove(selection)
        self.assertNotIn(selection, membership)

        face = pistoris.model.Selection(name="face")
        selections |= {face}
        self.assertEqual(set(selections), {face})

    def test_selection_references_have_stable_identity_and_leading_vertex_lifetime(self) -> None:
        model = pistoris.Model()
        model.skeleton.bones.append(pistoris.model.Bone(name="root"))
        selection = model.selections.add(pistoris.model.Selection(name="body"))
        same_selection = model.selections["body"]

        self.assertEqual(selection, same_selection)
        self.assertEqual(hash(selection), hash(same_selection))
        self.assertIsInstance(model.selections, Collection)
        self.assertNotIsInstance(model.selections, Sequence)
        self.assertIn("body", model.selections)
        self.assertIn(selection, model.selections)
        with self.assertRaises(TypeError):
            _ = model.selections[0]
        identity_hash = hash(selection)
        references = {selection}

        selection.name = "torso"
        self.assertEqual(hash(selection), identity_hash)
        self.assertIn(same_selection, references)
        selection.leading_vertex = pistoris.model.SelectionLeadingVertex(bone="root")
        leading_vertex = selection.leading_vertex
        assert leading_vertex is not None
        same_leading_vertex = same_selection.leading_vertex
        assert same_leading_vertex is not None
        self.assertEqual(leading_vertex, same_leading_vertex)

        selection.leading_vertex = pistoris.model.SelectionLeadingVertex(
            position=pistoris.math.Vector3(1.0, 2.0, 3.0), bone="root"
        )
        self.assertEqual(leading_vertex.position, pistoris.math.Vector3(1.0, 2.0, 3.0))
        selection.leading_vertex = None
        with self.assertRaises(ReferenceError):
            _ = leading_vertex.position

        selection.leading_vertex = pistoris.model.SelectionLeadingVertex(bone="root")
        replacement = selection.leading_vertex
        assert replacement is not None
        self.assertNotEqual(leading_vertex, replacement)

        del model.selections["torso"]
        self.assertEqual(hash(selection), identity_hash)
        self.assertEqual(selection, same_selection)
        self.assertIn(selection, references)
        with self.assertRaises(ReferenceError):
            _ = selection.name
        with self.assertRaises(ReferenceError):
            _ = replacement.position

        recreated = model.selections.add(pistoris.model.Selection(name="torso"))
        self.assertNotEqual(selection, recreated)

    def test_anchor_name_repair_preserves_authored_names_and_allocates_unique_defaults(self) -> None:
        level = pistoris.Level()
        level.anchors.replace(
            [
                pistoris.level.Anchor(name="anchor_1"),
                pistoris.level.Anchor(),
                pistoris.level.Anchor(name="anchor_3"),
                pistoris.level.Anchor(),
            ]
        )
        names = [anchor.name for anchor in level.anchors]
        self.assertEqual(names[0], "anchor_1")
        self.assertEqual(names[2], "anchor_3")
        self.assertEqual(len(set(names)), len(names))
        self.assertTrue(all(re.fullmatch(r"anchor_\d+", name) for name in names))

        del level.anchors[3]
        added = level.anchors.append(pistoris.level.Anchor())
        self.assertIsNone(added)
        names = [anchor.name for anchor in level.anchors]
        self.assertEqual(len(set(names)), len(names))
        self.assertRegex(level.anchors[-1].name, r"^anchor_\d+$")

    def test_semantic_collection_operations_have_collection_homes(self) -> None:
        model = pistoris.Model()
        self.assertTrue(callable(model.action_points.replace))
        self.assertTrue(callable(model.skeleton.infer_selection_memberships))
        self.assertTrue(callable(model.inventory_icon.render))
        self.assertTrue(callable(pistoris.Ambiance().tracks.trim_to_master))
        level = pistoris.Level()
        self.assertTrue(callable(level.mesh.generate_static_lighting))
        self.assertTrue(callable(level.portals.flatten))
        self.assertTrue(callable(level.room_distances.replace))
        self.assertTrue(callable(level.room_distances.reset))
        self.assertFalse(hasattr(level.room_distances, "clear"))
        self.assertTrue(callable(level.room_distances.generate))
        self.assertTrue(callable(level.anchors.replace))
        self.assertTrue(callable(level.anchors.generate))
        self.assertTrue(callable(level.anchors.prune_islands))
        self.assertTrue(callable(level.anchor_connections.generate))

    def test_cinematic_keyframe_sound_accepts_effect_and_speech_references(self) -> None:
        cinematic = pistoris.Cinematic.from_cin_bytes(
            fixture("mount/graph/interface/illustrations/numbers.cin")
        ).cinematic
        cinematic.sfx.append(pistoris.Sound(path="tests/effect"))
        cinematic.speech.append(pistoris.cinematic.Speech(path="tests/speech"))
        effect = cinematic.sfx[-1]
        speech = cinematic.speech[-1]
        keyframe = cinematic.keyframes[0]

        keyframe.sound = effect
        self.assertEqual(keyframe.sound, effect)
        keyframe.sound = speech
        self.assertEqual(keyframe.sound, speech)
        keyframe.sound = None
        self.assertIsNone(keyframe.sound)

    def test_cinematic_keyframe_light_is_optional(self) -> None:
        cinematic = pistoris.Cinematic.from_cin_bytes(
            fixture("mount/graph/interface/illustrations/numbers.cin")
        ).cinematic
        keyframe = cinematic.keyframes[0]
        keyframe.light = pistoris.cinematic.Light(intensity=0.75)
        light = keyframe.light
        self.assertIsNotNone(light)
        assert light is not None
        self.assertEqual(light.intensity, 0.75)
        light.intensity = 0.5
        self.assertEqual(keyframe.light, light)

        keyframe.light = None
        self.assertIsNone(keyframe.light)
        with self.assertRaises(ReferenceError):
            _ = light.intensity

    def test_live_references_track_sorted_insertions(self) -> None:
        cinematic = pistoris.Cinematic.from_cin_bytes(
            fixture("mount/graph/interface/illustrations/numbers.cin")
        ).cinematic
        insertion = next(
            index
            for index in range(1, len(cinematic.keyframes))
            if cinematic.keyframes[index].frame - cinematic.keyframes[index - 1].frame > 1
        )
        shifted_keyframe = cinematic.keyframes[insertion]
        shifted_frame = shifted_keyframe.frame
        keyframe = shifted_keyframe.copy()
        keyframe.frame = cinematic.keyframes[insertion - 1].frame + 1

        self.assertIsInstance(
            cinematic.keyframes.add(keyframe, illustration=shifted_keyframe.illustration),
            pistoris.cinematic.KeyframeRef,
        )
        self.assertEqual(shifted_keyframe.index, insertion + 1)
        self.assertEqual(shifted_keyframe.frame, shifted_frame)

        cinematic = pistoris.Cinematic()
        placeholder = cinematic.languages.add("placeholder")
        second = cinematic.languages.add("second")
        self.assertIsInstance(cinematic.languages, Collection)
        self.assertNotIsInstance(cinematic.languages, Sequence)
        del cinematic.languages[placeholder.name]
        first = cinematic.languages.add("first")
        second.name = "renamed"
        self.assertEqual(cinematic.languages["renamed"], second)
        self.assertEqual(first.name, "first")
        self.assertIn("renamed", cinematic.languages)
        self.assertIn(second, cinematic.languages)

        third = cinematic.languages.add("third")
        fourth = cinematic.languages.add("fourth")
        del cinematic.languages[third.name]
        cinematic.languages.add("third")
        self.assertEqual(cinematic.languages["fourth"], fourth)
        del cinematic.languages["third"]
        with self.assertRaises(KeyError) as missing_language:
            _ = cinematic.languages["third"]
        self.assertEqual(missing_language.exception.args, ("third",))
        with self.assertRaises(KeyError):
            del cinematic.languages["third"]

        level = pistoris.Level()
        anchors = [pistoris.level.Anchor(name=f"anchor_{index}") for index in range(3)]
        late = pistoris.level.AnchorConnection(first="anchor_1", second="anchor_2")
        level.anchors.replace(anchors, [late])
        shifted_connection = level.anchor_connections[0]
        early = pistoris.level.AnchorConnection(first="anchor_0", second="anchor_1")

        self.assertIsNone(level.anchor_connections.append(early))
        self.assertEqual(shifted_connection.index, 1)
        self.assertEqual(shifted_connection.first.name, "anchor_1")
        self.assertEqual(shifted_connection.second.name, "anchor_2")

    def test_live_references_distinguish_updates_from_removals(self) -> None:
        cinematic = pistoris.Cinematic()
        sounds = cinematic.sfx
        speech_sounds = cinematic.speech
        self.assertIsInstance(sounds, Sequence)
        self.assertIsInstance(speech_sounds, Sequence)
        audio = fixture("mount/speech/english/one.wav")
        sounds.extend(
            [
                pistoris.Sound(path="first", encoded_audio=audio),
                pistoris.Sound(path="second"),
            ]
        )
        first_sound_ref = sounds[0]
        second_sound_ref = sounds[1]
        first_sound_ref.path = "first/updated"
        self.assertEqual(sounds.by_path("first/updated"), first_sound_ref)
        self.assertEqual(first_sound_ref.encoded_audio, audio)
        self.assertIn(first_sound_ref, sounds)
        with self.assertRaises(KeyError) as missing_sound_error:
            sounds.by_path("missing")
        self.assertEqual(missing_sound_error.exception.args, ("missing",))
        second_sound_ref.encoded_audio = audio
        replacement_audio = fixture("mount/speech/english/two.wav")
        sounds[0] = pistoris.Sound(
            path="first/replaced", encoded_audio=replacement_audio
        )
        self.assertEqual(first_sound_ref.path, "first/replaced")
        self.assertEqual(first_sound_ref.encoded_audio, replacement_audio)
        first_sound_ref.encoded_audio = None
        self.assertIsNone(first_sound_ref.encoded_audio)
        self.assertEqual(second_sound_ref.encoded_audio, audio)

        english = cinematic.languages.add("English")
        self.assertEqual(english.name, "english")
        french = cinematic.languages.add("french")
        detached_speech = pistoris.cinematic.Speech(
            path="guard/greeting", encodings={"ENGLISH": audio}
        )
        detached_encodings = detached_speech.encodings
        detached_speech.encodings = {"english": replacement_audio}
        self.assertEqual(detached_encodings["english"], replacement_audio)
        cinematic.speech.append(detached_speech)
        speech = cinematic.speech.by_path("guard/greeting")
        self.assertIsInstance(speech.encodings, MutableMapping)
        self.assertEqual(speech.encodings, {"english": replacement_audio})
        del speech.encodings["english"]
        self.assertEqual(speech.encodings, {})
        with self.assertRaises(TypeError):
            hash(speech.encodings)
        speech.encodings[english.name] = audio
        speech.encodings[french.name] = replacement_audio
        preserved_encodings = dict(speech.encodings)
        with self.assertRaises(KeyError):
            cinematic.speech[0] = pistoris.cinematic.Speech(
                path="guard/changed", encodings={"unregistered": audio}
            )
        self.assertEqual(speech.path, "guard/greeting")
        self.assertEqual(speech.encodings, preserved_encodings)
        with self.assertRaises(KeyError):
            cinematic.speech.append(
                pistoris.cinematic.Speech(
                    path="guard/other", encodings={"unregistered": audio}
                )
            )
        self.assertEqual(len(cinematic.speech), 1)
        keys = speech.encodings.keys()
        self.assertIsInstance(keys, KeysView)
        self.assertIsInstance(speech.encodings.items(), ItemsView)
        self.assertIsInstance(speech.encodings.values(), ValuesView)
        self.assertEqual(set(keys), {"english", "french"})
        self.assertEqual(speech.encodings["english"], audio)
        self.assertEqual(speech.encodings["ENGLISH"], audio)
        self.assertEqual(dict(speech.encodings), {"english": audio, "french": replacement_audio})
        self.assertEqual(speech.encodings, {"english": audio, "french": replacement_audio})
        self.assertEqual({"english": audio, "french": replacement_audio}, speech.encodings)
        speech.encodings.update()
        speech.encodings.update([("english", replacement_audio)])
        self.assertEqual(speech.encodings.pop("english"), replacement_audio)
        self.assertEqual(set(keys), {"french"})
        with self.assertRaises(KeyError) as missing_encoding:
            _ = speech.encodings["english"]
        self.assertEqual(missing_encoding.exception.args, ("english",))
        with self.assertRaises(KeyError):
            del speech.encodings["english"]

        model = pistoris.Model()
        selection = model.selections.add(pistoris.model.Selection(name="temporary"))
        del model.selections[selection.name]
        with self.assertRaises(KeyError):
            _ = model.selections["temporary"]
        with self.assertRaises(KeyError):
            del model.selections["temporary"]

        del sounds[0]
        with self.assertRaises(ReferenceError):
            _ = first_sound_ref.path
        self.assertEqual(second_sound_ref.path, "second")

        level = pistoris.Level.from_native_bytes(
            fixture("mount/game/graph/levels/level30/fast.fts"),
            fixture("mount/graph/levels/level30/level30.llf"),
            fixture("mount/graph/levels/level30/level30.dlf"),
        ).level
        room_a = level.rooms[0]
        room_b = level.rooms[1]
        room_distance = level.room_distances[room_a, room_b]
        self.assertEqual(level.room_distances[room_b, room_a], room_distance)
        original_distance = room_distance.distance
        room_distance.distance = original_distance
        self.assertEqual(room_distance.distance, original_distance)
        with self.assertRaises(AttributeError):
            room_distance.room_a = room_distance.room_b

        replacement = room_distance.copy()
        level.room_distances[room_b, room_a] = replacement
        self.assertEqual(level.room_distances[0].distance, original_distance)

        portal = level.portals[0]
        vertices = list(portal.vertices)
        vertices = [pistoris.math.Vector3(vertex.x + 0.25, vertex.y, vertex.z) for vertex in vertices]
        portal.vertices = vertices
        self.assertEqual(room_distance.distance, -1.0)
        self.assertIsNone(room_distance.portal_a)
        self.assertIsNone(room_distance.portal_b)

        room_distance.reset()
        self.assertEqual(room_distance.distance, -1.0)
        with self.assertRaises(ValueError):
            _ = level.room_distances[(room_a,)]
        with self.assertRaises(ValueError):
            level.room_distances[(room_a,)] = replacement
        with self.assertRaises(ValueError):
            level.room_distances.replace([])

        level.room_distances.replace([entry.copy() for entry in level.room_distances])
        room_distance.distance = original_distance
        self.assertEqual(room_distance.distance, original_distance)
        count = len(level.room_distances)
        level.room_distances.reset()
        self.assertEqual(len(level.room_distances), count)
        self.assertEqual(room_distance.distance, -1.0)

    def test_ambiance_trim_preserves_retained_references(self) -> None:
        def automation(value: float) -> pistoris.ambiance.Automation:
            result = pistoris.ambiance.Automation()
            result.first = value
            result.second = value
            result.mode = pistoris.ambiance.AutomationMode.CONSTANT
            return result

        def key(play_count: int, delay_ms: int) -> pistoris.ambiance.PannedKey:
            result = pistoris.ambiance.PannedKey()
            result.play_count = play_count
            result.delay_min_ms = delay_ms
            result.delay_max_ms = delay_ms
            result.volume = automation(1.0)
            result.pitch = automation(1.0)
            result.pan = automation(0.0)
            return result

        ambiance = pistoris.Ambiance()
        self.assertIsNone(ambiance.master_track)
        with self.assertRaises(TypeError):
            ambiance.master_track = 0
        with self.assertRaises(TypeError):
            ambiance.master_track = None

        sound = pistoris.Sound()
        sound.path = "sfx/timing.wav"
        sound.encoded_audio = fixture("mount/speech/english/one.wav")
        ambiance.sounds.append(sound)
        sound_path = ambiance.sounds[0].path
        ambiance.tracks.append(
            pistoris.ambiance.PannedTrack(
                sound_path=sound_path, keys=[key(1, 1_000_000)]
            )
        )
        master = ambiance.tracks[0]
        ambiance.tracks.append(
            pistoris.ambiance.PannedTrack(
                sound_path=sound_path,
                keys=[key(1, 400_000), key(2, 400_000), key(1, 400_000)],
            )
        )
        child = ambiance.tracks[1]
        self.assertEqual(ambiance.master_track, master)
        ambiance.master_track = child
        self.assertEqual(ambiance.master_track, child)
        ambiance.master_track = master
        self.assertEqual(ambiance.master_track, master)

        other = pistoris.Ambiance()
        other.sounds.append(pistoris.Sound(path="sfx/timing"))
        other.tracks.append(
            pistoris.ambiance.PannedTrack(
                sound_path="sfx/timing",
                keys=[key(1, 0)],
            )
        )
        with self.assertRaises(ValueError):
            ambiance.master_track = other.tracks[0]

        track = child
        first = track.keys[0]
        shortened = track.keys[1]
        removed = track.keys[2]

        self.assertEqual(ambiance.tracks.trim_to_master(), 1)
        self.assertEqual(track.index, 1)
        self.assertEqual(len(track.keys), 2)
        self.assertEqual(first.index, 0)
        self.assertEqual(first.play_count, 1)
        self.assertEqual(shortened.index, 1)
        self.assertEqual(shortened.play_count, 1)
        with self.assertRaises(ReferenceError):
            _ = removed.index

    def test_ambiance_master_track_follows_track_removals(self) -> None:
        ambiance = pistoris.Ambiance()
        ambiance.sounds.append(pistoris.Sound(path="sfx/master"))
        track = pistoris.ambiance.PannedTrack(
            sound_path="sfx/master", keys=[pistoris.ambiance.PannedKey()]
        )
        ambiance.tracks.extend([track, track, track])
        master = ambiance.tracks[2]
        ambiance.master_track = master

        del ambiance.tracks[0]
        self.assertEqual(master.index, 1)
        self.assertEqual(ambiance.master_track, master)
        del ambiance.tracks[1]
        with self.assertRaises(ReferenceError):
            _ = master.index
        self.assertEqual(ambiance.master_track, ambiance.tracks[0])
        with self.assertRaises(ReferenceError):
            ambiance.master_track = master
        ambiance.tracks.clear()
        self.assertIsNone(ambiance.master_track)

    def test_dependent_collection_references_are_invalidated(self) -> None:
        animation = pistoris.Animation.from_tea_bytes(
            fixture("mount/graph/obj3d/anims/npc/human_male_gathering.tea")
        ).animation
        group = animation.groups[0]
        transform = animation.keyframes[0].group_transforms[0]
        frame = animation.keyframes[0].copy()
        frame.group_transforms = [pistoris.animation.GroupTransform() for _ in animation.groups]
        animation.keyframes[0] = frame
        with self.assertRaises(ReferenceError):
            _ = transform.index

        keyframe = animation.keyframes[0]
        replacement = keyframe.copy()
        frame_length = animation.frame_length
        animation.keyframes.replace([replacement], frame_length=frame_length)
        self.assertEqual(animation.frame_length, frame_length)
        self.assertEqual(len(animation.keyframes), 1)
        with self.assertRaises(ReferenceError):
            _ = keyframe.index
        with self.assertRaises(ReferenceError):
            _ = group.index

        group = animation.groups[0]
        animation.keyframes.clear()
        with self.assertRaises(ReferenceError):
            _ = group.index

        level = pistoris.Level.from_native_bytes(
            fixture("mount/game/graph/levels/level30/fast.fts"),
            fixture("mount/graph/levels/level30/level30.llf"),
            fixture("mount/graph/levels/level30/level30.dlf"),
        ).level
        nav_vertices = [pistoris.level.Vertex() for _ in range(3)]
        nav_vertices[0].position = pistoris.math.Vector3(0.0, 0.0, 0.0)
        nav_vertices[1].position = pistoris.math.Vector3(1.0, 0.0, 0.0)
        nav_vertices[2].position = pistoris.math.Vector3(0.0, 0.0, 1.0)
        nav_triangle = pistoris.level.NavSurfaceTriangle(vertices=nav_vertices)
        level.nav_surface.replace([], [nav_triangle])

        anchors = [pistoris.level.Anchor(name=f"anchor_{index}") for index in range(2)]
        connection_value = pistoris.level.AnchorConnection(first="anchor_0", second="anchor_1")
        level.anchors.replace(anchors, [connection_value])

        nav_vertex = level.nav_surface.vertices[0]
        anchor = level.anchors[0]
        connection = level.anchor_connections[0]

        distance_level = pistoris.Level()
        for name in ("first", "second", "third"):
            distance_level.rooms.append(pistoris.level.Room(name=name))
        room_distance = distance_level.room_distances[distance_level.rooms[1], distance_level.rooms[2]]
        distance_level.rooms.append(pistoris.level.Room(name="fourth"))
        added_room = distance_level.rooms[3]
        self.assertEqual(room_distance.room_a.name, "second")
        del distance_level.rooms[added_room.index]
        self.assertEqual(room_distance.room_b.name, "third")
        del distance_level.rooms[0]
        self.assertEqual(room_distance.room_a.name, "second")
        self.assertEqual(room_distance.room_b.name, "third")
        del distance_level.rooms[room_distance.room_a.index]
        with self.assertRaises(ReferenceError):
            _ = room_distance.distance

        cascade_level = pistoris.Level()
        for name in ("first", "second", "third"):
            cascade_level.rooms.append(pistoris.level.Room(name=name))
        for name, room_1, room_2, offset in (
            ("removed", "first", "second", 0.0),
            ("retained", "second", "third", 2.0),
        ):
            portal = pistoris.level.Portal()
            portal.name = name
            portal.room_1 = room_1
            portal.room_2 = room_2
            portal.vertices = [
                pistoris.math.Vector3(offset, 0.0, 0.0),
                pistoris.math.Vector3(offset + 1.0, 0.0, 0.0),
                pistoris.math.Vector3(offset + 1.0, 0.0, 1.0),
                pistoris.math.Vector3(offset, 0.0, 1.0),
            ]
            cascade_level.portals.append(portal)
        removed_portal = cascade_level.portals[0]
        retained_portal = cascade_level.portals[1]

        del cascade_level.rooms[0]
        with self.assertRaises(ReferenceError):
            _ = removed_portal.index
        self.assertEqual(retained_portal.index, 0)
        self.assertEqual(retained_portal.name, "retained")
        self.assertEqual(retained_portal.room_1.name, "second")
        self.assertEqual(retained_portal.room_2.name, "third")

        cascade_level.anchors.replace(
            [pistoris.level.Anchor(name=f"anchor_{index}") for index in range(3)],
            [
                pistoris.level.AnchorConnection(first="anchor_0", second="anchor_1"),
                pistoris.level.AnchorConnection(first="anchor_1", second="anchor_2"),
            ],
        )
        removed_connection = cascade_level.anchor_connections[0]
        retained_connection = cascade_level.anchor_connections[1]

        del cascade_level.anchors[0]
        with self.assertRaises(ReferenceError):
            _ = removed_connection.index
        self.assertEqual(retained_connection.index, 0)
        self.assertEqual(retained_connection.first.name, "anchor_1")
        self.assertEqual(retained_connection.second.name, "anchor_2")

        level.mesh.clear()
        for reference in (nav_vertex, anchor, connection):
            with self.assertRaises(ReferenceError):
                _ = reference.index

    def test_live_collections_cover_each_semantic_resource(self) -> None:
        animation = pistoris.Animation.from_tea_bytes(
            fixture("mount/graph/obj3d/anims/npc/human_male_gathering.tea")
        ).animation
        animation_sound = animation.sounds[0]
        animation_sound.path = "custom/animation_sound"
        self.assertEqual(animation.sounds[0].path, "custom/animation_sound")

        ambiance = pistoris.Ambiance.from_amb_bytes(fixture("mount/sfx/ambiance/dark.amb")).ambiance
        ambiance_sound = ambiance.sounds[0]
        ambiance_sound.path = "custom/ambiance_sound"
        self.assertEqual(ambiance.sounds[0].path, "custom/ambiance_sound")

        cinematic = pistoris.Cinematic.from_cin_bytes(
            fixture("mount/graph/interface/illustrations/numbers.cin")
        ).cinematic
        illustration = cinematic.illustrations[0]
        illustration.path = "custom/cinematic_texture"
        self.assertEqual(cinematic.illustrations[0].path, "custom/cinematic_texture")

        level = pistoris.Level.from_native_bytes(
            fixture("mount/game/graph/levels/level9/fast.fts"),
            fixture("mount/graph/levels/level9/level9.llf"),
            fixture("mount/graph/levels/level9/level9.dlf"),
        ).level
        level_texture = level.mesh.textures[0]
        level_texture.path = "custom/level_texture"
        self.assertEqual(level.mesh.textures[0].path, "custom/level_texture")

    def test_live_corner_color_assignment_updates_level_storage(self) -> None:
        level = pistoris.Level.from_native_bytes(
            fixture("mount/game/graph/levels/level9/fast.fts"),
            fixture("mount/graph/levels/level9/level9.llf"),
            fixture("mount/graph/levels/level9/level9.dlf"),
        ).level
        level.mesh.reset_corner_colors()
        face = level.mesh.faces[0]
        self.assertEqual(face.corners[0].color, pistoris.math.Color3(0.5, 0.5, 0.5))

        color = pistoris.math.Color3(0.25, 0.5, 0.75)
        face.corners[0].color = color
        self.assertEqual(level.mesh.faces[0].corners[0].color, color)

        level.mesh.reset_corner_colors()
        self.assertEqual(face.corners[0].color, pistoris.math.Color3(0.5, 0.5, 0.5))

    def test_inventory_icon_slot_count_does_not_wrap(self) -> None:
        icon = fixture("mount/graph/obj3d/interactive/items/weapons/sword_00/sword_00[icon].png")
        authored = pistoris.model.InventoryIcon(
            encoded_image=icon, width_slots=2, height_slots=None
        )
        self.assertEqual(authored.width_slots, 2)
        self.assertIsNone(authored.height_slots)

        model = pistoris.Model()
        model.inventory_icon.set(authored)
        stored = model.inventory_icon.copy()
        self.assertIsNotNone(stored)
        assert stored is not None
        self.assertEqual(stored.width_slots, 2)
        self.assertIsNotNone(stored.height_slots)
        self.assertTrue(model.inventory_icon.render().startswith(b"\x89PNG\r\n\x1a\n"))
        self.assertTrue(
            model.inventory_icon.render(format=pistoris.ImageFormat.BMP).startswith(b"BM")
        )
        self.assertGreater(len(model.inventory_icon.render(format=pistoris.ImageFormat.TGA)), 18)
        with self.assertRaises(pistoris.PistorisError):
            model.inventory_icon.render(format=pistoris.ImageFormat.JPEG)

        with self.assertRaises(ValueError):
            pistoris.model.InventoryIcon(encoded_image=icon, width_slots=0)
        with self.assertRaises(ValueError):
            pistoris.model.InventoryIcon(encoded_image=icon, width_slots=257)

    def test_text_conversion_and_flag_vocabulary(self) -> None:
        self.assertEqual(pistoris.utf8_to_latin1("café"), b"caf\xe9")
        flags = pistoris.FaceFlag.WATER | pistoris.FaceFlag.TRANS
        self.assertEqual(int(flags), 12)
        face = pistoris.model.Face()
        face.flags = flags
        self.assertIsInstance(face.flags, pistoris.FaceFlag)
        self.assertEqual(face.flags, flags)
        self.assertEqual(
            int(pistoris.level.LightFlag.SPAWN_FIRE | pistoris.level.LightFlag.SPAWN_SMOKE),
            24,
        )
        self.assertEqual(
            pistoris.FaceFlag.LEVEL_ALL,
            pistoris.FaceFlag.ALL & ~pistoris.FaceFlag.QUAD,
        )
        self.assertFalse(hasattr(pistoris, "LEVEL_FACE_FLAGS_ALL"))
        self.assertEqual(pistoris.level.AnchorFlag.BLOCKED, 8)
        automation = pistoris.ambiance.Automation()
        self.assertIs(automation.mode, pistoris.ambiance.AutomationMode.CONSTANT)
        automation.mode = pistoris.ambiance.AutomationMode.RANDOM_INTERPOLATED
        self.assertIs(
            automation.mode,
            pistoris.ambiance.AutomationMode.RANDOM_INTERPOLATED,
        )
        portal = pistoris.level.Portal()
        portal.shape = pistoris.level.PortalShape.TRIANGLE
        self.assertIs(portal.shape, pistoris.level.PortalShape.TRIANGLE)
        zone = pistoris.level.Zone()
        zone.height_mode = pistoris.level.ZoneHeightMode.INFINITE
        self.assertIs(zone.height_mode, pistoris.level.ZoneHeightMode.INFINITE)
        node = pistoris.level.PathNode()
        node.type = pistoris.level.PathNodeType.BEZIER
        self.assertIs(node.type, pistoris.level.PathNodeType.BEZIER)
        self.assertIs(
            pistoris.cinematic.IllustrationFormat.AUTO,
            pistoris.cinematic.IllustrationFormat.AUTO,
        )

        animation_key = pistoris.animation.Keyframe()
        animation_key.footstep = True
        self.assertIs(animation_key.footstep, True)
        cinematic_key = pistoris.cinematic.Keyframe()
        cinematic_key.crossfade = True
        cinematic_key.dream = True
        cinematic_key.light = pistoris.cinematic.Light(intensity=0.75)
        self.assertIs(cinematic_key.crossfade, True)
        self.assertIs(cinematic_key.dream, True)
        self.assertIsNotNone(cinematic_key.light)
        self.assertFalse(hasattr(pistoris.level.PlayerSpawn(), "is_usable"))

    def test_semantic_record_sequences_use_whole_value_assignment(self) -> None:
        frame = pistoris.animation.Frame()
        frame.group_transforms = [pistoris.animation.GroupTransform()]
        self.assertIsInstance(frame.group_transforms, Sequence)
        self.assertEqual(len(frame.group_transforms), 1)
        frame.group_transforms[0].scale = pistoris.math.Vector3(2.0, 3.0, 4.0)
        self.assertEqual(frame.group_transforms[0].scale, pistoris.math.Vector3(2.0, 3.0, 4.0))

        portal = pistoris.level.Portal()
        portal.vertices = [pistoris.math.Vector3() for _ in range(4)]
        self.assertIsInstance(portal.vertices, tuple)
        self.assertEqual(len(portal.vertices), 4)

        zone = pistoris.level.Zone()
        zone.perimeter_xz = [pistoris.math.Vector2(), pistoris.math.Vector2()]
        self.assertIsInstance(zone.perimeter_xz, tuple)
        self.assertEqual(len(zone.perimeter_xz), 2)

        path = pistoris.level.Path()
        path.nodes = [pistoris.level.PathNode()]
        self.assertIsInstance(path.nodes, Sequence)
        self.assertEqual(len(path.nodes), 1)
        path.nodes[0].time_ms = 10
        self.assertEqual(path.nodes[0].time_ms, 10)

    def test_detached_nested_references_are_live_and_invalidate_on_replacement(self) -> None:
        face = pistoris.model.Face()
        corner = face.corners[0]
        corner.u = 0.75
        self.assertEqual(face.corners[0].u, 0.75)
        self.assertEqual(corner, face.corners[0])

        track = pistoris.ambiance.PannedTrack(keys=[pistoris.ambiance.PannedKey()])
        key = track.keys[0]
        key.play_count = 3
        key.pan.first = 0.5
        self.assertEqual(track.keys[0].play_count, 3)
        self.assertEqual(track.keys[0].pan.first, 0.5)
        track.keys = []
        with self.assertRaises(ReferenceError):
            _ = key.play_count

        frame = pistoris.animation.Frame(group_transforms=[pistoris.animation.GroupTransform()])
        transform = frame.group_transforms[0]
        frame.group_transforms = []
        with self.assertRaises(ReferenceError):
            _ = transform.scale

    def test_collections_follow_the_sequence_protocol(self) -> None:
        model = pistoris.Model()
        self.assertIsNone(model.mesh.vertices.append(pistoris.model.Vertex()))
        self.assertIsNone(model.mesh.vertices.extend(pistoris.model.Vertex() for _ in range(2)))
        vertices = model.mesh.vertices
        reference = vertices[0]
        self.assertIsInstance(vertices, Sequence)
        self.assertEqual(len(vertices), 3)
        self.assertIn(reference, vertices)
        self.assertEqual(vertices.index(reference), 0)
        self.assertEqual(vertices.count(reference), 1)
        self.assertEqual(list(reversed(vertices)), vertices[::-1])

        model.mesh.vertices.extend(vertex.copy() for vertex in model.mesh.vertices)
        self.assertEqual(len(model.mesh.vertices), 6)

        ambiance = pistoris.Ambiance()
        ambiance.sounds.append(pistoris.Sound(path="test"))
        ambiance.tracks.append(
            pistoris.ambiance.PannedTrack(
                sound_path="test",
                keys=[pistoris.ambiance.PannedKey(play_count=1)],
            )
        )
        ambiance.tracks.extend(track.copy() for track in ambiance.tracks)
        self.assertEqual(len(ambiance.tracks), 2)

    def test_positional_mutators_accept_negative_indices(self) -> None:
        model = pistoris.Model()
        model.mesh.vertices.append(pistoris.model.Vertex())
        replacement_vertex = pistoris.model.Vertex(position=pistoris.math.Vector3(1.0, 2.0, 3.0))
        model.mesh.vertices[-1] = replacement_vertex
        self.assertEqual(model.mesh.vertices[0].position, replacement_vertex.position)
        with self.assertRaises(IndexError):
            model.mesh.vertices[-2] = replacement_vertex

        animation = pistoris.Animation()
        animation.sounds.append(pistoris.Sound(path="first"))
        animation.sounds[-1] = pistoris.Sound(path="last")
        self.assertEqual(animation.sounds[0].path, "last")
        del animation.sounds[-1]
        self.assertEqual(len(animation.sounds), 0)

        ambiance = pistoris.Ambiance()
        ambiance.sounds.append(pistoris.Sound(path="test"))
        panned_key = pistoris.ambiance.PannedKey(play_count=1)
        ambiance.tracks.append(
            pistoris.ambiance.PannedTrack(sound_path="test", keys=[panned_key])
        )
        positioned_key = pistoris.ambiance.PositionedKey(play_count=1)
        ambiance.tracks[-1] = pistoris.ambiance.PositionedTrack(
            sound_path="test",
            keys=[positioned_key],
        )
        self.assertIs(ambiance.tracks[0].kind, pistoris.ambiance.TrackKind.POSITIONED)
        del ambiance.tracks[-1]
        self.assertEqual(len(ambiance.tracks), 0)

        cinematic = pistoris.Cinematic()
        cinematic.illustrations.append(pistoris.cinematic.Illustration(path="test"))
        cinematic.illustrations[-1] = pistoris.cinematic.Illustration(
            path="test", subdivision_scale=2
        )
        self.assertEqual(cinematic.illustrations[0].subdivision_scale, 2)
        del cinematic.illustrations[-1]
        self.assertEqual(len(cinematic.illustrations), 0)

        level = pistoris.Level()
        level.rooms.append(pistoris.level.Room(name="first"))
        level.rooms[-1] = pistoris.level.Room(name="last")
        self.assertEqual(level.rooms[0].name, "last")
        del level.rooms[-1]
        self.assertEqual(len(level.rooms), 0)

    def test_cinematic_illustrations_compact_unused_images(self) -> None:
        cinematic = pistoris.Cinematic()
        cinematic.illustrations.append(pistoris.cinematic.Illustration(path="unused"))
        cinematic.illustrations.append(pistoris.cinematic.Illustration(path="retained"))
        del cinematic.illustrations[0]

        self.assertEqual(cinematic.illustrations.compact(), 1)
        self.assertEqual(len(cinematic.illustrations), 1)
        self.assertEqual(cinematic.illustrations[0].path, "retained")

    def test_detached_records_have_structural_value_semantics(self) -> None:
        left = pistoris.model.Face()
        right = pistoris.model.Face()
        self.assertEqual(left, right)
        right.corners[0].u = 0.25
        self.assertNotEqual(left, right)
        with self.assertRaises(TypeError):
            hash(left)
        icon = pistoris.model.InventoryIcon(encoded_image=b"x" * 64, width_slots=1, height_slots=1)
        self.assertIn("encoded_image=<64 bytes>", repr(icon))

        first_texture = pistoris.Texture(encoded_image=b"image")
        second_texture = pistoris.Texture(encoded_image=b"image")
        self.assertEqual(first_texture, second_texture)
        self.assertIn("encoded_image=<5 bytes>", repr(first_texture))
        second_texture.encoded_image = b"other"
        self.assertNotEqual(first_texture, second_texture)
        self.assertIn("encoded_image=None", repr(pistoris.Texture()))

        first_nav_info = pistoris.Level().nav_surface.info
        second_nav_info = pistoris.Level().nav_surface.info
        self.assertEqual(first_nav_info, second_nav_info)
        self.assertEqual(
            repr(first_nav_info),
            "NavSurfaceInfo(has_surface=False, vertex_count=0, triangle_count=0)",
        )
        with self.assertRaises(TypeError):
            hash(first_nav_info)

        locations = []
        for _ in range(2):
            with self.assertRaises(pistoris.PistorisError) as raised:
                pistoris.Model.from_glb(b"")
            locations.append(raised.exception.location)
        self.assertEqual(locations[0], locations[1])
        self.assertIn("domain='glb'", repr(locations[0]))
        with self.assertRaises(TypeError):
            hash(locations[0])

        cinematic = pistoris.Cinematic()
        cinematic.languages.add("english")
        first_language = cinematic.languages["english"].copy()
        second_language = cinematic.languages["english"].copy()
        self.assertEqual(first_language, second_language)
        self.assertEqual(repr(first_language), "Language(name='english')")
        with self.assertRaises(TypeError):
            hash(first_language)

    def test_conversion_bundles_have_bounded_identity_representations(self) -> None:
        bundle_types = (
            pistoris.model.NativeOutput,
            pistoris.model.BytesOutput,
            pistoris.model.Import,
            pistoris.model.GlbOutput,
            pistoris.model.ObjOutput,
            pistoris.animation.NativeOutput,
            pistoris.animation.BytesOutput,
            pistoris.animation.Import,
            pistoris.ambiance.NativeOutput,
            pistoris.ambiance.BytesOutput,
            pistoris.ambiance.GlbOutput,
            pistoris.ambiance.Import,
            pistoris.cinematic.NativeOutput,
            pistoris.cinematic.BytesOutput,
            pistoris.cinematic.GlbOutput,
            pistoris.cinematic.Import,
            pistoris.level.NativeOutput,
            pistoris.level.BytesOutput,
            pistoris.level.Import,
            pistoris.level.GlbOutput,
        )
        for bundle_type in bundle_types:
            with self.subTest(bundle_type=bundle_type.__name__):
                self.assertIsNot(bundle_type.__repr__, object.__repr__)
                self.assertIs(bundle_type.__eq__, object.__eq__)
                self.assertIs(bundle_type.__hash__, object.__hash__)

        data = fixture(
            "mount/game/graph/obj3d/interactive/npc/human_male/human_male.ftl"
        )
        first = pistoris.Model.from_ftl_bytes(data)
        second = pistoris.Model.from_ftl_bytes(data)
        self.assertNotEqual(first, second)
        self.assertIsInstance(hash(first), int)
        self.assertNotIn(" object at ", repr(first))
        self.assertIn("texture_source_paths=<", repr(first))

        output = first.model.to_ftl_bytes(include_sidecars=False)
        self.assertNotIn(" object at ", repr(output))
        self.assertEqual(repr(output), "BytesOutput(texture_files=<0 items>)")

    def test_runtime_docs_cover_resources_and_live_views(self) -> None:
        self.assertIn("from_*_bytes", pistoris.Model.__doc__ or "")
        self.assertIn("live sequence", pistoris.model.VertexCollection.__doc__ or "")
        self.assertIn("independent", pistoris.model.VertexRef.copy.__doc__ or "")
        self.assertIn("destructive reset", pistoris.animation.GroupRef.make_void.__doc__ or "")

    def test_semantic_surface_is_complete(self) -> None:
        expected = {
            pistoris.Model: {
                "copy",
                "reset",
                "from_ftl",
                "from_ftl_bytes",
                "from_obj",
                "from_glb",
                "to_ftl",
                "to_ftl_bytes",
                "to_glb",
                "to_level_preview_glb",
                "to_obj",
                "validate",
                "scale",
                "rotate",
                "translate",
                "apply_reference",
                "resource_path",
                "mesh",
                "skeleton",
                "origin",
                "action_points",
                "selections",
                "inventory_icon",
            },
            pistoris.Animation: {
                "copy",
                "reset",
                "from_tea",
                "from_tea_bytes",
                "to_tea",
                "to_tea_bytes",
                "validate",
                "scale",
                "rotate",
                "name",
                "resource_path",
                "frame_length",
                "groups",
                "keyframes",
                "sounds",
            },
            pistoris.Ambiance: {
                "copy",
                "reset",
                "from_amb",
                "from_amb_bytes",
                "from_glb",
                "to_amb",
                "to_amb_bytes",
                "to_glb",
                "validate",
                "resource_path",
                "tracks",
                "sounds",
                "master_track",
            },
            pistoris.Cinematic: {
                "copy",
                "reset",
                "from_cin",
                "from_cin_bytes",
                "from_glb",
                "to_cin",
                "to_cin_bytes",
                "to_glb",
                "validate",
                "resource_path",
                "end_frame",
                "fps",
                "illustrations",
                "keyframes",
                "sfx",
                "speech",
                "languages",
            },
            pistoris.Level: {
                "copy",
                "reset",
                "from_native",
                "from_native_bytes",
                "from_glb",
                "to_native",
                "to_native_bytes",
                "to_dlf",
                "to_glb",
                "validate",
                "resource_path",
                "bounds",
                "referenced_bounds",
                "mesh",
                "rooms",
                "portals",
                "room_distances",
                "anchors",
                "anchor_connections",
                "nav_surface",
                "lights",
                "player_spawn",
                "entities",
                "fogs",
                "zones",
                "paths",
                "minimap",
                "loading_screen",
            },
        }
        for resource, methods in expected.items():
            with self.subTest(resource=resource.__name__):
                actual = {name for name in vars(resource) if not name.startswith("_")}
                self.assertEqual(actual, methods)


if __name__ == "__main__":
    unittest.main()
