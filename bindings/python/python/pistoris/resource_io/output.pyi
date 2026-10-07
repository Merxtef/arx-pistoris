from collections.abc import Sequence
from enum import Enum
from pathlib import Path

from ..paths import ResourceKind
from .._core.resource_io import ExistingFilePolicy

class OutputKind(Enum):
    DATA: OutputKind
    IMAGE: OutputKind
    AUDIO: OutputKind

class WriteStatus(Enum):
    PENDING: WriteStatus
    NEEDS_CANDIDATE: WriteStatus
    NEEDS_EXISTING_FILE_POLICY: WriteStatus
    READY: WriteStatus
    WRITTEN: WriteStatus
    ALREADY_CURRENT: WriteStatus
    PRESERVED: WriteStatus
    FAILED: WriteStatus

class WriteCandidate:
    """One encoded output competing for a destination."""
    @property
    def kind(self) -> OutputKind: ...
    @property
    def primary(self) -> bool: ...
    @property
    def owner_kind(self) -> ResourceKind | None: ...
    @property
    def owner_identity(self) -> str: ...
    @property
    def resource_path(self) -> str | None: ...
    @property
    def native_path(self) -> Path | None: ...
    @property
    def data(self) -> bytes: ...
    @property
    def size(self) -> int: ...
    @property
    def written(self) -> bool: ...

class WriteEntry:
    """One resolved destination and its mutable pending decisions."""
    @property
    def path(self) -> Path: ...
    @property
    def candidates(self) -> tuple[WriteCandidate, ...]: ...
    @property
    def selected_candidate(self) -> int | None: ...
    @selected_candidate.setter
    def selected_candidate(self, value: int, /) -> None: ...
    @property
    def if_exists(self) -> ExistingFilePolicy | None: ...
    @if_exists.setter
    def if_exists(self, value: ExistingFilePolicy | None, /) -> None: ...
    @property
    def status(self) -> WriteStatus: ...

class WriteReportEntry:
    """The final status of one output destination."""
    @property
    def path(self) -> Path: ...
    @property
    def status(self) -> WriteStatus: ...

class WriteReport(Sequence[WriteReportEntry]):
    """An immutable snapshot of write outcomes."""
    ...

class WritePlan(Sequence[WriteEntry]):
    """A mutable set of pending output decisions."""
    @property
    def default_if_exists(self) -> ExistingFilePolicy: ...
    @default_if_exists.setter
    def default_if_exists(self, value: ExistingFilePolicy, /) -> None: ...
    def preflight(self) -> WriteReport: ...
    def execute(self) -> WriteReport: ...
