from enum import Enum
from pathlib import Path

MAXIMUM_READ_MOUNTS: int

class DirectoryEntryKind(Enum):
    DIRECTORY: DirectoryEntryKind
    UNKNOWN_FILE: DirectoryEntryKind
    FTL: DirectoryEntryKind
    TEA: DirectoryEntryKind
    FTS: DirectoryEntryKind
    DLF: DirectoryEntryKind
    LLF: DirectoryEntryKind
    AMB: DirectoryEntryKind
    CIN: DirectoryEntryKind
    GLB: DirectoryEntryKind
    OBJ: DirectoryEntryKind
    MTL: DirectoryEntryKind
    JSON: DirectoryEntryKind
    PNG: DirectoryEntryKind
    JPEG: DirectoryEntryKind
    BMP: DirectoryEntryKind
    TGA: DirectoryEntryKind
    WAV: DirectoryEntryKind
    MP3: DirectoryEntryKind
    OGG: DirectoryEntryKind

class Mount:
    @property
    def id(self) -> int: ...
    @property
    def path(self) -> Path: ...

class ReadResult:
    @property
    def data(self) -> bytes: ...
    @property
    def native_path(self) -> Path: ...
    @property
    def mount_id(self) -> int: ...

class ResolvedResource:
    @property
    def native_path(self) -> Path: ...
    @property
    def mount_id(self) -> int: ...

class ResourceFile:
    @property
    def logical_path(self) -> str: ...
    @property
    def provider_mask(self) -> int: ...

class DirectoryEntry:
    @property
    def name(self) -> str: ...
    @property
    def kind(self) -> DirectoryEntryKind: ...
    @property
    def provider_mask(self) -> int: ...
