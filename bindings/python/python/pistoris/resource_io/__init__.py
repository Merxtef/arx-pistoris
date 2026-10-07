"""Mounted Arx resource discovery and loading."""

from reprlib import repr as _bounded_repr

from . import catalog as catalog
from . import mounts as mounts
from . import output as output
from .._core.resource_io import (
    ALL_MOUNTS,
    ExistingFilePolicy,
    Operation,
    OutputPart,
    ResourceMounts,
    Resources,
    libertatis_resource_root,
)
from .._facade import publish_types as _publish_types
from .._facade import install_read_only_properties as _install_read_only_properties

__all__ = [
    "ALL_MOUNTS",
    "ExistingFilePolicy",
    "MountMask",
    "Operation",
    "OutputPart",
    "ResourceMounts",
    "Resources",
    "libertatis_resource_root",
    "catalog",
    "mounts",
    "output",
]

_publish_types(globals(), __name__, __all__)
_install_read_only_properties(Resources, ("mounts",))

# Runtime alias only: publishing it as a class would try to rewrite builtins.int.
MountMask = int


def _resource_mounts_repr(value: ResourceMounts) -> str:
    return (
        f"ResourceMounts(read_mounts={len(value.read_mounts)}, "
        f"write_mount={_bounded_repr(value.write_mount)})"
    )


def _resources_repr(value: Resources) -> str:
    return f"Resources(mounts={value.mounts!r})"


ResourceMounts.__repr__ = _resource_mounts_repr
Resources.__repr__ = _resources_repr
