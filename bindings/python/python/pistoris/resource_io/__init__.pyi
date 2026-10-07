from collections.abc import Buffer, Sequence
from os import PathLike
from pathlib import Path
from enum import Enum, Flag
from typing import TypeAlias

from .. import Ambiance, Animation, Cinematic, Level, Model, NativeTextMode
from ..cinematic import IllustrationFormat
from ..math import Vector3
from ..model import Import as ModelImport
from ..paths import AmbianceSelector, AnimationSelector, CinematicSelector, LevelSelector, ModelSelector
from .._core.resource_io import Operation as Operation
from . import catalog as catalog
from . import mounts as mounts
from . import output as output

ALL_MOUNTS: int
MountMask: TypeAlias = int

def libertatis_resource_root() -> Path: ...

class OutputPart(Flag):
    """Categories of files to include in a resource write."""
    NONE: OutputPart
    PRIMARY: OutputPart
    COMPANIONS: OutputPart
    TEXTURES: OutputPart
    AUDIO: OutputPart
    IMAGES: OutputPart
    ALL: OutputPart

class ExistingFilePolicy(Enum):
    """How a write handles a differing destination that already exists."""
    ERROR: ExistingFilePolicy
    OVERWRITE: ExistingFilePolicy
    PRESERVE: ExistingFilePolicy

class ResourceMounts:
    """Ordered live read roots and an independent write root."""
    def __init__(
        self,
        read_mounts: Sequence[str | PathLike[str]] = (),
        *,
        write_mount: str | PathLike[str] | None = None,
    ) -> None: ...
    @property
    def read_mounts(self) -> tuple[mounts.Mount, ...]: ...
    @property
    def write_mount(self) -> Path | None: ...
    @write_mount.setter
    def write_mount(self, value: str | PathLike[str] | None, /) -> None: ...
    @property
    def available_mount_mask(self) -> int: ...
    def add_read_mount(self, path: str | PathLike[str]) -> mounts.Mount | None: ...
    def add_libertatis_mounts(self) -> None: ...
    def set_libertatis_write_mount(self) -> None: ...
    def highest_priority_mount(self, mount_mask: MountMask) -> mounts.Mount | None: ...
    def read(
        self, path: str | PathLike[str], *, mount_mask: MountMask = ALL_MOUNTS, recover_case_collisions: bool = False
    ) -> mounts.ReadResult: ...
    def resolve(
        self, path: str | PathLike[str], *, mount_mask: MountMask = ALL_MOUNTS, recover_case_collisions: bool = False
    ) -> mounts.ResolvedResource: ...
    def resolve_write_path(
        self, path: str | PathLike[str], *, recover_case_collisions: bool = False
    ) -> Path: ...
    def write(
        self, path: str | PathLike[str], data: Buffer, *, recover_case_collisions: bool = False
    ) -> None: ...
    def list_files(
        self,
        directory: str | PathLike[str] = "",
        *,
        max_depth: int,
        mount_mask: MountMask = ALL_MOUNTS,
        recover_case_collisions: bool = False,
    ) -> list[mounts.ResourceFile]: ...
    def list_directory(
        self,
        directory: str | PathLike[str] = "",
        *,
        mount_mask: MountMask = ALL_MOUNTS,
        recover_case_collisions: bool = False,
    ) -> list[mounts.DirectoryEntry]: ...

class Resources:
    """Complete mounted or loose resource loading and writing."""
    def __init__(
        self,
        read_mounts: Sequence[str | PathLike[str]] = (),
        *,
        write_mount: str | PathLike[str] | None = None,
    ) -> None: ...
    @property
    def mounts(self) -> ResourceMounts: ...
    def scan_catalog(
        self, *, mount_mask: MountMask = ALL_MOUNTS, recover_case_collisions: bool = False
    ) -> catalog.Catalog: ...
    def load_model(
        self,
        source: str | PathLike[str] | ModelSelector,
        *,
        text_mode: NativeTextMode = NativeTextMode.AUTO,
        arx_units_per_glb_unit: float = 10.0,
        mount_mask: MountMask = ALL_MOUNTS,
        recover_case_collisions: bool = False,
    ) -> ModelImport: ...
    def load_model_file(
        self,
        source: str | PathLike[str],
        *,
        text_mode: NativeTextMode = NativeTextMode.AUTO,
        arx_units_per_glb_unit: float = 10.0,
    ) -> ModelImport: ...
    def load_animation(
        self,
        source: str | PathLike[str] | AnimationSelector,
        *,
        text_mode: NativeTextMode = NativeTextMode.AUTO,
        mount_mask: MountMask = ALL_MOUNTS,
        recover_case_collisions: bool = False,
    ) -> Animation: ...
    def load_animation_file(
        self, source: str | PathLike[str], *, text_mode: NativeTextMode = NativeTextMode.AUTO
    ) -> Animation: ...
    def load_level(
        self,
        source: int | str | PathLike[str] | LevelSelector,
        *,
        text_mode: NativeTextMode = NativeTextMode.AUTO,
        arx_units_per_glb_unit: float = 100.0,
        arx_offset: Vector3 | None = None,
        mount_mask: MountMask = ALL_MOUNTS,
        recover_case_collisions: bool = False,
    ) -> Level: ...
    def load_level_file(
        self,
        source: str | PathLike[str],
        *,
        llf: str | PathLike[str] | None = None,
        dlf: str | PathLike[str] | None = None,
        text_mode: NativeTextMode = NativeTextMode.AUTO,
        arx_units_per_glb_unit: float = 100.0,
        arx_offset: Vector3 | None = None,
    ) -> Level: ...
    def load_ambiance(
        self,
        source: str | PathLike[str] | AmbianceSelector,
        *,
        text_mode: NativeTextMode = NativeTextMode.AUTO,
        arx_units_per_glb_unit: float = 10.0,
        mount_mask: MountMask = ALL_MOUNTS,
        recover_case_collisions: bool = False,
    ) -> Ambiance: ...
    def load_ambiance_file(
        self,
        source: str | PathLike[str],
        *,
        text_mode: NativeTextMode = NativeTextMode.AUTO,
        arx_units_per_glb_unit: float = 10.0,
    ) -> Ambiance: ...
    def load_cinematic(
        self,
        source: str | PathLike[str] | CinematicSelector,
        *,
        text_mode: NativeTextMode = NativeTextMode.AUTO,
        mount_mask: MountMask = ALL_MOUNTS,
        recover_case_collisions: bool = False,
    ) -> Cinematic: ...
    def load_cinematic_file(
        self, source: str | PathLike[str], *, text_mode: NativeTextMode = NativeTextMode.AUTO
    ) -> Cinematic: ...
    def prepare_model_write(
        self,
        model: Model | ModelImport,
        target: str | PathLike[str] | ModelSelector,
        *,
        text_mode: NativeTextMode | None = None,
        arx_units_per_glb_unit: float | None = None,
        compress: bool | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
        recover_case_collisions: bool = False,
    ) -> output.WritePlan: ...
    def prepare_model_file_write(
        self,
        model: Model | ModelImport,
        target: str | PathLike[str],
        *,
        text_mode: NativeTextMode | None = None,
        arx_units_per_glb_unit: float | None = None,
        compress: bool | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
    ) -> output.WritePlan: ...
    def prepare_animation_write(
        self,
        animation: Animation,
        target: str | PathLike[str] | AnimationSelector,
        *,
        text_mode: NativeTextMode = NativeTextMode.AUTO,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
        recover_case_collisions: bool = False,
    ) -> output.WritePlan: ...
    def prepare_animation_file_write(
        self,
        animation: Animation,
        target: str | PathLike[str],
        *,
        text_mode: NativeTextMode = NativeTextMode.AUTO,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
    ) -> output.WritePlan: ...
    def prepare_level_write(
        self,
        level: Level,
        target: str | PathLike[str] | LevelSelector,
        *,
        llf: str | PathLike[str] | None = None,
        dlf: str | PathLike[str] | None = None,
        level_index: int | None = None,
        text_mode: NativeTextMode | None = None,
        arx_units_per_glb_unit: float | None = None,
        arx_offset: Vector3 | None = None,
        reconstruct_quads: bool | None = None,
        compress: bool | None = None,
        embed_lighting: bool | None = None,
        signer: str | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
        recover_case_collisions: bool = False,
    ) -> output.WritePlan: ...
    def prepare_level_file_write(
        self,
        level: Level,
        target: str | PathLike[str],
        *,
        llf: str | PathLike[str] | None = None,
        dlf: str | PathLike[str] | None = None,
        level_index: int | None = None,
        text_mode: NativeTextMode | None = None,
        arx_units_per_glb_unit: float | None = None,
        arx_offset: Vector3 | None = None,
        reconstruct_quads: bool | None = None,
        compress: bool | None = None,
        embed_lighting: bool | None = None,
        signer: str | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
    ) -> output.WritePlan: ...
    def prepare_ambiance_write(
        self,
        ambiance: Ambiance,
        target: str | PathLike[str] | AmbianceSelector,
        *,
        text_mode: NativeTextMode | None = None,
        arx_units_per_glb_unit: float | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
        recover_case_collisions: bool = False,
    ) -> output.WritePlan: ...
    def prepare_ambiance_file_write(
        self,
        ambiance: Ambiance,
        target: str | PathLike[str],
        *,
        text_mode: NativeTextMode | None = None,
        arx_units_per_glb_unit: float | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
    ) -> output.WritePlan: ...
    def prepare_cinematic_write(
        self,
        cinematic: Cinematic,
        target: str | PathLike[str] | CinematicSelector,
        *,
        text_mode: NativeTextMode | None = None,
        illustration_format: IllustrationFormat | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
        recover_case_collisions: bool = False,
    ) -> output.WritePlan: ...
    def prepare_cinematic_file_write(
        self,
        cinematic: Cinematic,
        target: str | PathLike[str],
        *,
        text_mode: NativeTextMode | None = None,
        illustration_format: IllustrationFormat | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
    ) -> output.WritePlan: ...
    def write_model(
        self,
        model: Model | ModelImport,
        target: str | PathLike[str] | ModelSelector,
        *,
        text_mode: NativeTextMode | None = None,
        arx_units_per_glb_unit: float | None = None,
        compress: bool | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
        recover_case_collisions: bool = False,
    ) -> output.WriteReport: ...
    def write_model_file(
        self,
        model: Model | ModelImport,
        target: str | PathLike[str],
        *,
        text_mode: NativeTextMode | None = None,
        arx_units_per_glb_unit: float | None = None,
        compress: bool | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
    ) -> output.WriteReport: ...
    def write_animation(
        self,
        animation: Animation,
        target: str | PathLike[str] | AnimationSelector,
        *,
        text_mode: NativeTextMode = NativeTextMode.AUTO,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
        recover_case_collisions: bool = False,
    ) -> output.WriteReport: ...
    def write_animation_file(
        self,
        animation: Animation,
        target: str | PathLike[str],
        *,
        text_mode: NativeTextMode = NativeTextMode.AUTO,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
    ) -> output.WriteReport: ...
    def write_level(
        self,
        level: Level,
        target: str | PathLike[str] | LevelSelector,
        *,
        llf: str | PathLike[str] | None = None,
        dlf: str | PathLike[str] | None = None,
        level_index: int | None = None,
        text_mode: NativeTextMode | None = None,
        arx_units_per_glb_unit: float | None = None,
        arx_offset: Vector3 | None = None,
        reconstruct_quads: bool | None = None,
        compress: bool | None = None,
        embed_lighting: bool | None = None,
        signer: str | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
        recover_case_collisions: bool = False,
    ) -> output.WriteReport: ...
    def write_level_file(
        self,
        level: Level,
        target: str | PathLike[str],
        *,
        llf: str | PathLike[str] | None = None,
        dlf: str | PathLike[str] | None = None,
        level_index: int | None = None,
        text_mode: NativeTextMode | None = None,
        arx_units_per_glb_unit: float | None = None,
        arx_offset: Vector3 | None = None,
        reconstruct_quads: bool | None = None,
        compress: bool | None = None,
        embed_lighting: bool | None = None,
        signer: str | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
    ) -> output.WriteReport: ...
    def write_ambiance(
        self,
        ambiance: Ambiance,
        target: str | PathLike[str] | AmbianceSelector,
        *,
        text_mode: NativeTextMode | None = None,
        arx_units_per_glb_unit: float | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
        recover_case_collisions: bool = False,
    ) -> output.WriteReport: ...
    def write_ambiance_file(
        self,
        ambiance: Ambiance,
        target: str | PathLike[str],
        *,
        text_mode: NativeTextMode | None = None,
        arx_units_per_glb_unit: float | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
    ) -> output.WriteReport: ...
    def write_cinematic(
        self,
        cinematic: Cinematic,
        target: str | PathLike[str] | CinematicSelector,
        *,
        text_mode: NativeTextMode | None = None,
        illustration_format: IllustrationFormat | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
        recover_case_collisions: bool = False,
    ) -> output.WriteReport: ...
    def write_cinematic_file(
        self,
        cinematic: Cinematic,
        target: str | PathLike[str],
        *,
        text_mode: NativeTextMode | None = None,
        illustration_format: IllustrationFormat | None = None,
        outputs: OutputPart = OutputPart.ALL,
        if_exists: ExistingFilePolicy = ExistingFilePolicy.ERROR,
    ) -> output.WriteReport: ...
