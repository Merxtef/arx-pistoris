"""Mount configuration and lookup result types."""

from .._core.resource_io.mounts import (
    MAXIMUM_READ_MOUNTS,
    DirectoryEntry,
    DirectoryEntryKind,
    Mount,
    ReadResult,
    ResolvedResource,
    ResourceFile,
)
from .._facade import install_immutable_values as _install_immutable_values
from .._facade import install_record_semantics as _install_record_semantics
from .._facade import publish_types as _publish_types

__all__ = [name for name in globals() if not name.startswith("_")]

_publish_types(globals(), __name__, __all__)
_install_immutable_values(
    globals(),
    (
        "DirectoryEntry",
        "Mount",
        "ReadResult",
        "ResolvedResource",
        "ResourceFile",
    ),
)
_install_record_semantics(
    globals(),
    {
        "DirectoryEntry": ("name", "kind", "provider_mask"),
        "Mount": ("id", "path"),
        "ReadResult": ("data", "native_path", "mount_id"),
        "ResolvedResource": ("native_path", "mount_id"),
        "ResourceFile": ("logical_path", "provider_mask"),
    },
)
