# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

import unittest
from importlib import import_module
from importlib.util import find_spec
from pathlib import Path

import pistoris


class ImportTests(unittest.TestCase):
    def test_module_identity(self) -> None:
        self.assertEqual(pistoris.__name__, "pistoris")
        self.assertEqual(pistoris.native.__name__, "pistoris.native")
        self.assertIs(pistoris.native, import_module("pistoris.native"))
        for name in ("model", "animation", "ambiance", "cinematic", "level", "resource_io"):
            module = getattr(pistoris, name)
            self.assertEqual(module.__name__, f"pistoris.{name}")
            self.assertIs(module, import_module(f"pistoris.{name}"))
        for name in ("catalog", "mounts", "output"):
            module = getattr(pistoris.resource_io, name)
            self.assertEqual(module.__name__, f"pistoris.resource_io.{name}")
            self.assertIs(module, import_module(f"pistoris.resource_io.{name}"))
        for name in ("amb", "cin", "dlf", "ftl", "fts", "llf", "tea"):
            module = getattr(pistoris.native, name)
            self.assertEqual(module.__name__, f"pistoris.native.{name}")
            self.assertIs(module, import_module(f"pistoris.native.{name}"))
            self.assertEqual(find_spec(f"pistoris.native.{name}").name, module.__name__)
        self.assertEqual(pistoris.native.ftl.Data.__module__, "pistoris.native.ftl")

    def test_collection_stubs_preserve_element_types(self) -> None:
        private_stubs = Path(pistoris.__file__).with_name("_core")
        public_stubs = private_stubs.parent
        resource_stub = (private_stubs / "__init__.pyi").read_text(encoding="utf-8")
        math_stub = (private_stubs / "math.pyi").read_text(encoding="utf-8")
        native_stub = (private_stubs / "native" / "fts.pyi").read_text(encoding="utf-8")
        root_stub = (public_stubs / "__init__.pyi").read_text(encoding="utf-8")
        model_stub = (public_stubs / "model.pyi").read_text(encoding="utf-8")
        paths_stub = (public_stubs / "paths.pyi").read_text(encoding="utf-8")
        resource_io_stub = (public_stubs / "resource_io" / "__init__.pyi").read_text(encoding="utf-8")
        resource_io_catalog_stub = (public_stubs / "resource_io" / "catalog.pyi").read_text(encoding="utf-8")
        resource_io_mounts_stub = (public_stubs / "resource_io" / "mounts.pyi").read_text(encoding="utf-8")
        resource_io_output_stub = (public_stubs / "resource_io" / "output.pyi").read_text(encoding="utf-8")

        self.assertIn("def vertices(self) -> ModelVertexCollection: ...", resource_stub)
        self.assertIn("def __getitem__(self, index: int) -> ModelVertexRef: ...", resource_stub)
        self.assertIn("def copy(self) -> ModelVertex:", resource_stub)
        self.assertIn("def tracks(self) -> AmbianceTrackCollection: ...", resource_stub)
        self.assertIn("def utf8_to_latin1(text: str) -> bytes: ...", resource_stub)
        self.assertIn("class FaceFlag(enum.IntFlag):", resource_stub)
        self.assertIn("class LightFlag(enum.IntFlag):", resource_stub)
        face_flag_stub = resource_stub.split("class FaceFlag(enum.IntFlag):", 1)[1].split("\nclass ", 1)[0]
        self.assertIn("LEVEL_ALL = ", face_flag_stub)
        self.assertNotIn("LEVEL_FACE_FLAGS_ALL", resource_stub)
        self.assertNotIn("LEVEL_FACE_FLAGS_ALL", root_stub)
        self.assertIn("class AmbianceAutomationMode(enum.Enum):", resource_stub)
        self.assertIn("class AmbianceTrackKind(enum.Enum):", resource_stub)
        self.assertIn("class CinematicIllustrationFormat(enum.Enum):", resource_stub)
        self.assertIn("class PortalShape(enum.Enum):", resource_stub)
        self.assertIn("class ZoneHeightMode(enum.Enum):", resource_stub)
        self.assertIn("class PathNodeType(enum.Enum):", resource_stub)
        self.assertNotIn("std::", resource_stub)
        selection_stub = resource_stub.split("class ModelSelectionRef:", 1)[1].split("\nclass ", 1)[0]
        self.assertNotIn("def id(self) -> int: ...", selection_stub)
        self.assertNotIn("def index(self) -> int: ...", selection_stub)
        self.assertIn(
            "def leading_vertex(self) -> ModelSelectionLeadingVertexRef | None: ...",
            selection_stub,
        )
        self.assertIn("def vertices(self) -> tuple[VertexRef, ...]: ...", selection_stub)
        self.assertIn("def bones(self) -> tuple[BoneRef, ...]: ...", selection_stub)
        self.assertIn("def action_points(self) -> tuple[ActionPointRef, ...]: ...", selection_stub)
        self.assertIn("def includes_origin(self) -> bool: ...", selection_stub)
        self.assertNotIn("has_leading_vertex", selection_stub)
        self.assertIn("def source_path(self) -> str: ...", resource_stub)
        self.assertNotIn("def source_texture_index(self) -> int: ...", resource_stub)
        self.assertNotIn("def source_sound_index(self) -> int: ...", resource_stub)
        self.assertNotIn("def source_sound_handle(self) -> int: ...", resource_stub)
        self.assertIn(
            "def from_native_bytes(fts: object, llf: object | None = None, dlf: object | None = None, *, "
            "include_texture_sources: bool = True",
            resource_stub,
        )
        self.assertNotIn("with_sources", resource_stub)
        self.assertNotIn("with_animations", resource_stub)
        self.assertIn(
            "def from_ftl_bytes(data: object, *, include_texture_sources: bool = True",
            resource_stub,
        )
        self.assertIn(
            "def from_tea_bytes(data: object, *, include_sound_sources: bool = True",
            resource_stub,
        )
        self.assertIn(
            "def from_amb_bytes(data: object, *, include_sound_sources: bool = True",
            resource_stub,
        )
        self.assertIn(
            "def from_cin_bytes(data: object, *, include_illustration_sources: bool = True, "
            "include_sound_sources: bool = True",
            resource_stub,
        )
        self.assertIn("def source_index(self) -> int | None: ...", resource_stub)
        self.assertIn("def line(self) -> int | None: ...", resource_stub)
        self.assertIn(
            "def group_transforms(self) -> AnimationGroupTransformCollection: ...",
            resource_stub,
        )
        self.assertIn("def perimeter_xz(self) -> tuple[Vector2, ...]: ...", resource_stub)
        self.assertIn("def perimeter_xz(self, value: Sequence[Vector2], /) -> None: ...", resource_stub)
        self.assertIn(
            "def vertices(self) -> tuple[Vector3, Vector3, Vector3, Vector3]: ...",
            resource_stub,
        )
        self.assertIn("def vertices(self) -> tuple[LevelVertex, LevelVertex, LevelVertex]: ...", resource_stub)
        self.assertIn("def ambiance(self) -> LevelZoneAmbiance | None: ...", resource_stub)
        self.assertIn("def __iter__(self) -> Iterator[float]: ...", math_stub)
        self.assertIn("class ModelType(StrEnum):", paths_stub)
        self.assertIn("class AnimationType(StrEnum):", paths_stub)
        self.assertIn("class ModelSelector:", paths_stub)
        self.assertIn("def __init__(self, type: ModelType, name: str, tweak: str = \"\") -> None: ...", paths_stub)
        self.assertIn("def to_path(self) -> str: ...", paths_stub)
        self.assertIn("def from_path(path: str | PathLike[str]) -> ModelSelector: ...", paths_stub)
        self.assertIn("def animation_directory(type: ModelType) -> str: ...", paths_stub)
        self.assertIn("def animation_directory(type: AnimationType) -> str: ...", paths_stub)
        self.assertIn("def model_search_location(type: ModelType) -> SearchLocation: ...", paths_stub)
        self.assertIn("def selector_from_string(selector: str) -> ResourceSelector:", paths_stub)
        self.assertIn("def selector_from_path(path: str | PathLike[str]) -> ResourceSelector:", paths_stub)
        self.assertIn("class Catalog(Sequence[Entry[ResourceSelector]]):", resource_io_catalog_stub)
        self.assertIn("def models(self) -> View[ModelSelector]: ...", resource_io_catalog_stub)
        self.assertIn("class WritePlan(Sequence[WriteEntry]):", resource_io_output_stub)
        self.assertIn("import builtins", resource_stub)
        self.assertIn("@builtins.property", resource_stub)
        self.assertIn("from .._core.resource_io import Operation as Operation", resource_io_stub)
        self.assertNotIn("class Operation(Enum):", resource_io_stub)
        self.assertIn("class DirectoryEntryKind(Enum):", resource_io_mounts_stub)
        self.assertNotIn("class DirectoryEntryKind(Enum):", resource_io_stub)
        self.assertRegex(
            resource_io_stub,
            r"def load_model\(\s*self,\s*source: str \| PathLike\[str\] \| ModelSelector,",
        )
        self.assertRegex(
            resource_io_stub,
            r"def load_model_file\(\s*self,\s*source: str \| PathLike\[str\],",
        )
        self.assertNotIn("std::", resource_io_stub)
        self.assertIn(
            "from collections.abc import Collection, Iterable, Iterator, Mapping, MutableMapping, MutableSet, Sequence",
            resource_stub,
        )
        self.assertIn("def __iter__(self) -> Iterator[ModelVertexRef]: ...", resource_stub)
        self.assertIn("def __iter__(self) -> Iterator[TextureFile]: ...", resource_stub)
        self.assertIn("def __reversed__(self) -> Iterator[ModelVertexRef]: ...", resource_stub)
        self.assertIn("def __reversed__(self) -> Iterator[TextureFile]: ...", resource_stub)
        self.assertNotRegex(resource_stub, r"def __iter__\(self\) -> _\w+Iterator")
        self.assertIn("def nodes(self) -> LevelPathNodeCollection: ...", resource_stub)
        self.assertIn("def animations(self) -> tuple[Animation, ...]: ...", resource_stub)
        self.assertIn("def texture_files(self) -> TextureFileSequence: ...", resource_stub)
        self.assertIn("def sound_files(self) -> SoundFileSequence: ...", resource_stub)
        self.assertIn("def sound_sources(self) -> tuple[SoundSourceReference, ...]: ...", resource_stub)
        self.assertIn("def illustration_source_paths(self) -> tuple[str, ...]: ...", resource_stub)
        self.assertIn("def selections(self) -> ModelSelectionSet: ...", resource_stub)
        self.assertIn("class ModelSelectionSet(MutableSet[ModelSelectionRef]):", resource_stub)
        self.assertIn("class ModelSelectionValueSet(MutableSet[ModelSelection]):", resource_stub)
        self.assertNotIn("def selection_vertices(", resource_stub)
        self.assertNotIn("def selection_bones(", resource_stub)
        self.assertNotIn("def selection_action_points(", resource_stub)
        self.assertNotIn("def selection_includes_origin(", resource_stub)
        self.assertNotIn("def clear_selection_vertices(", resource_stub)
        self.assertNotIn("def clear_selection_bones(", resource_stub)
        self.assertNotIn("def clear_selection_action_points(", resource_stub)
        self.assertNotIn("def set_selection_includes_origin(", resource_stub)
        self.assertIn("def groups(self) -> AnimationGroupCollection: ...", resource_stub)
        self.assertIn("class AnimationGroupCollection(Sequence[AnimationGroupRef]):", resource_stub)
        self.assertIn(
            "def replace(self, frames: Sequence[AnimationFrame], *, frame_length: int) -> None: ...",
            resource_stub,
        )
        self.assertNotIn("def replace_keyframes(", resource_stub)
        self.assertIn("class ModelVertexCollection(Sequence[ModelVertexRef]):", resource_stub)
        self.assertIn(
            "def from_obj(obj: str, mtl: str = '', *, include_texture_sources: bool = True) -> ModelImport: ...",
            resource_stub,
        )
        self.assertIn("def obj_material_library_paths(obj: str) -> list[str]: ...", resource_stub)
        self.assertIn("def inventory_icon(self) -> ModelInventoryIconRef: ...", resource_stub)
        inventory_icon_ref_stub = resource_stub.split("class ModelInventoryIconRef:", 1)[1].split("\nclass ", 1)[0]
        self.assertIn("def copy(self) -> InventoryIcon | None: ...", inventory_icon_ref_stub)
        self.assertIn("def set(self, icon: InventoryIcon) -> None: ...", inventory_icon_ref_stub)
        self.assertIn("def clear(self) -> None: ...", inventory_icon_ref_stub)
        self.assertIn("def render(", inventory_icon_ref_stub)
        self.assertIn("def minimap(self) -> LevelMinimapRef: ...", resource_stub)
        self.assertIn("def player_spawn(self) -> LevelPlayerSpawnRef | None: ...", resource_stub)
        self.assertIn("def loading_screen(self) -> LevelLoadingScreenRef: ...", resource_stub)
        minimap_ref_stub = resource_stub.split("class LevelMinimapRef:", 1)[1].split("\nclass ", 1)[0]
        self.assertIn("def render(", minimap_ref_stub)
        self.assertIn("def generate(", minimap_ref_stub)
        loading_ref_stub = resource_stub.split("class LevelLoadingScreenRef:", 1)[1].split("\nclass ", 1)[0]
        self.assertIn("def render(", loading_ref_stub)
        self.assertIn("def light(self) -> CinematicLightRef | None: ...", resource_stub)
        self.assertIn("def light(self, value: CinematicLight | None, /) -> None: ...", resource_stub)
        self.assertNotIn("def light_active(", resource_stub)
        self.assertNotIn("AnchorFlag as AnchorFlag", root_stub)
        self.assertNotIn("LightFlag as LightFlag", root_stub)
        self.assertNotIn("SoundKind as SoundKind", root_stub)
        self.assertIn("ModelInventoryIconRef as InventoryIconRef", model_stub)
        self.assertNotIn("def set_origin(", resource_stub)
        self.assertNotIn("def set_frame_length(", resource_stub)
        self.assertNotIn("def set_player_spawn(", resource_stub)
        self.assertNotIn("def clear_player_spawn(", resource_stub)
        self.assertNotIn("def set_loading_screen(", resource_stub)
        self.assertNotIn("def clear_loading_screen(", resource_stub)
        self.assertNotIn("def set_corner_color(", resource_stub)
        self.assertNotIn("def set_face_room(", resource_stub)
        self.assertNotIn("def room_distance(", resource_stub)
        self.assertNotIn("def set_texture_image(", resource_stub)
        self.assertNotIn("def clear_texture_image(", resource_stub)
        self.assertIn("support_ignore_flags: FaceFlag", resource_stub)
        self.assertIn("def master_track(self) -> AmbianceTrackRef | None: ...", resource_stub)
        self.assertIn("def master_track(self, value: AmbianceTrackRef, /) -> None: ...", resource_stub)
        self.assertNotIn("def set_master_track(", resource_stub)
        self.assertIn("def keys(self) -> AmbianceKeyCollection: ...", resource_stub)
        self.assertIn(
            "def copy(self) -> AmbiancePannedTrack | AmbiancePositionedTrack: ...",
            resource_stub,
        )
        self.assertIn(
            "def append(self, value: AmbiancePannedTrack | AmbiancePositionedTrack) -> None: ...",
            resource_stub,
        )
        self.assertNotIn("def panned_keys(", resource_stub)
        self.assertNotIn("def positioned_keys(", resource_stub)
        self.assertIn("def __getitem__(self, index: int) -> CinematicSoundEffectRef: ...", resource_stub)
        self.assertIn("def __getitem__(self, index: int) -> CinematicSpeechRef: ...", resource_stub)
        self.assertIn("def __getitem__(self, key: str) -> ModelSelectionRef: ...", resource_stub)
        self.assertIn("def __getitem__(self, key: str) -> CinematicLanguageRef: ...", resource_stub)
        self.assertIn("class ModelSelectionCollection(Collection[ModelSelectionRef]):", resource_stub)
        self.assertIn("class CinematicSoundEffectCollection(Sequence[CinematicSoundEffectRef]):", resource_stub)
        self.assertIn("class CinematicSpeechCollection(Sequence[CinematicSpeechRef]):", resource_stub)
        self.assertIn("class CinematicLanguageCollection(Collection[CinematicLanguageRef]):", resource_stub)
        self.assertIn("def by_path(self, path: str) -> AnimationSoundRef: ...", resource_stub)
        self.assertIn("def by_path(self, path: str) -> AmbianceSoundRef: ...", resource_stub)
        self.assertIn("def by_path(self, path: str) -> CinematicSoundEffectRef: ...", resource_stub)
        self.assertIn("def by_path(self, path: str) -> CinematicSpeechRef: ...", resource_stub)
        for collection in (
            "ModelSelectionCollection",
            "CinematicLanguageCollection",
        ):
            collection_stub = resource_stub.split(f"class {collection}(", 1)[1].split("\nclass ", 1)[0]
            self.assertNotIn("def by_name(", collection_stub)
        self.assertNotIn("def set_sound_path(", resource_stub)
        self.assertNotIn("def set_sound_data(self, sound_handle: int, language_id: int", resource_stub)
        self.assertNotIn("def clear_sound_data(self, sound_handle: int, language_id: int", resource_stub)
        self.assertIn("class CinematicSpeechEncodingMap(MutableMapping[str, bytes]):", resource_stub)
        self.assertIn("class CinematicSpeech:", resource_stub)
        self.assertIn(
            "def __init__(self, *, path: str = '', encodings: Mapping[str, object] | None = None) -> None: ...",
            resource_stub,
        )
        self.assertNotIn("CinematicSoundEncoding", resource_stub)
        self.assertIn("def room_1(self) -> RoomRef | None: ...", resource_stub)
        self.assertNotIn("SOUND_EFFECTS_LANGUAGE_ID", root_stub)
        self.assertIn(
            "def from_tea(tea: native.tea.Data, *, include_sound_sources: bool = True, "
            "text_mode: NativeTextMode",
            resource_stub,
        )
        self.assertIn("def animation_report(self) -> AnimationConversionReport | None: ...", resource_stub)
        self.assertIn("def model_preview_report(self) -> LevelModelPreviewReport: ...", resource_stub)
        self.assertIn("def vertices(self) -> tuple[Vertex, Vertex, Vertex, Vertex]: ...", native_stub)
        self.assertIn(
            "from collections.abc import Iterable, Mapping, MutableMapping, MutableSequence, Sequence",
            native_stub,
        )
        ftl_stub = (private_stubs / "native" / "ftl.pyi").read_text(encoding="utf-8")
        self.assertIn(
            "def vertices(self, arg: Iterable[pistoris._core.native.ftl.Vertex], /) -> None: ...",
            ftl_stub,
        )
        self.assertIn(
            "def vertices(self) -> MutableSequence[Vertex]: ...",
            ftl_stub,
        )
        self.assertIn(
            "def textures(self, arg: Mapping[int, pistoris._core.native.fts.Texture], /) -> None: ...",
            native_stub,
        )
        self.assertIn(
            "def textures(self) -> MutableMapping[int, Texture]: ...",
            native_stub,
        )
        self.assertNotIn("pistoris._core.native.FtlVertexList", ftl_stub)
        self.assertNotIn("pistoris._core.native.FtsTextureMap", native_stub)
        report_stub = resource_stub.split("class AnimationConversionReport:", 1)[1].split("\nclass ", 1)[0]
        self.assertNotIn("def __init__", report_stub)
        self.assertIn("__hash__: None = None", report_stub)
        error_location_stub = resource_stub.split("class ErrorLocation:", 1)[1].split("\nclass ", 1)[0]
        self.assertIn("__hash__: None = None", error_location_stub)
        self.assertIn("radius: float = 0.0001", resource_stub)
        self.assertNotIn("9.999999747378752e-05", resource_stub)
        self.assertIn("support_min_up_cos: float = 0.5881717", resource_stub)
        self.assertIn(
            "support_ignore_flags: FaceFlag = FaceFlag.TRANS | FaceFlag.WATER | FaceFlag.NOCOL | FaceFlag.LAVA",
            resource_stub,
        )
        self.assertIn("min_component_area_ratio: float = 0.05", resource_stub)
        self.assertIn("radius_scale: float = 0.9", resource_stub)
        self.assertIn("global_factor: float = 0.85", resource_stub)
        self.assertNotIn("0.05000000074505806", resource_stub)
        self.assertNotIn("0.8999999761581421", resource_stub)
        self.assertNotIn("0.8500000238418579", resource_stub)
        self.assertIn("from . import model as model", root_stub)
        self.assertIn("ModelVertex as Vertex", model_stub)
        self.assertIn(
            "LevelZoneAmbiance as ZoneAmbiance",
            (public_stubs / "level.pyi").read_text(encoding="utf-8"),
        )


if __name__ == "__main__":
    unittest.main()
