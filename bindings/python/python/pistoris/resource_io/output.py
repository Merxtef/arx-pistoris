"""Prepared resource writes, conflict decisions, and outcome reports."""

from .._core.resource_io.output import (
    OutputKind,
    WriteCandidate,
    WriteEntry,
    WritePlan,
    WriteReport,
    WriteReportEntry,
    WriteStatus,
)
from .._facade import install_read_only_properties as _install_read_only_properties
from .._facade import publish_types as _publish_types

__all__ = [name for name in globals() if not name.startswith("_")]

_publish_types(globals(), __name__, __all__)
_install_read_only_properties(
    WriteCandidate,
    ("kind", "primary", "owner_kind", "owner_identity", "resource_path", "native_path", "data", "size", "written"),
)
_install_read_only_properties(WriteEntry, ("path", "candidates", "status"))
_install_read_only_properties(WriteReportEntry, ("path", "status"))


def _candidate_repr(value: WriteCandidate) -> str:
    location = value.resource_path if value.resource_path is not None else value.native_path
    return (
        f"WriteCandidate(kind={value.kind!r}, path={location!r}, "
        f"size={value.size}, written={value.written})"
    )


def _entry_repr(value: WriteEntry) -> str:
    return f"WriteEntry(path={value.path!r}, status={value.status!r}, candidates={len(value.candidates)})"


def _report_entry_repr(value: WriteReportEntry) -> str:
    return f"WriteReportEntry(path={value.path!r}, status={value.status!r})"


def _plan_repr(value: WritePlan) -> str:
    return f"WritePlan(len={len(value)}, default_if_exists={value.default_if_exists!r})"


def _report_repr(value: WriteReport) -> str:
    return f"WriteReport(len={len(value)})"


WriteCandidate.__repr__ = _candidate_repr
WriteEntry.__repr__ = _entry_repr
WriteReportEntry.__repr__ = _report_entry_repr
WritePlan.__repr__ = _plan_repr
WriteReport.__repr__ = _report_repr
