"""Ambiance editing values, views, and conversion outputs."""

from ._core import (
    Ambiance,
    AmbianceAutomation as Automation,
    AmbianceAutomationMode as AutomationMode,
    AmbianceAutomationRef as AutomationRef,
    AmbianceBytesOutput as BytesOutput,
    AmbianceGlbOutput as GlbOutput,
    AmbianceImport as Import,
    AmbianceKeyCollection as KeyCollection,
    AmbianceKeyRef as KeyRef,
    AmbianceNativeOutput as NativeOutput,
    AmbiancePannedKey as PannedKey,
    AmbiancePannedTrack as PannedTrack,
    AmbiancePositionedKey as PositionedKey,
    AmbiancePositionedTrack as PositionedTrack,
    AmbianceSoundCollection as SoundCollection,
    AmbianceSoundRef as SoundRef,
    AmbianceTrackCollection as TrackCollection,
    AmbianceTrackKind as TrackKind,
    AmbianceTrackRef as TrackRef,
)
from ._facade import install_immutable_values as _install_immutable_values
from ._facade import install_record_semantics as _install_record_semantics
from ._facade import install_record_repr as _install_record_repr
from ._facade import publish_types as _publish_types

__all__ = [
    "Ambiance",
    "Automation",
    "AutomationMode",
    "AutomationRef",
    "BytesOutput",
    "GlbOutput",
    "Import",
    "KeyCollection",
    "KeyRef",
    "NativeOutput",
    "PannedKey",
    "PannedTrack",
    "PositionedKey",
    "PositionedTrack",
    "SoundCollection",
    "SoundRef",
    "TrackCollection",
    "TrackKind",
    "TrackRef",
]

_publish_types(globals(), __name__, __all__)
_install_immutable_values(
    globals(), ("BytesOutput", "GlbOutput", "Import", "NativeOutput")
)
_install_record_semantics(
    globals(),
    {
        "Automation": ("first", "second", "interval_ms", "mode"),
        "PannedKey": ("start_delay_ms", "play_count", "delay_min_ms", "delay_max_ms", "volume", "pitch", "pan"),
        "PositionedKey": ("start_delay_ms", "play_count", "delay_min_ms", "delay_max_ms", "volume", "pitch", "x", "y", "z"),
        "PannedTrack": ("sound_path", "keys"),
        "PositionedTrack": ("sound_path", "keys"),
    },
)
_install_record_repr(
    globals(),
    {
        "NativeOutput": ("sound_files",),
        "BytesOutput": ("sound_files",),
        "GlbOutput": ("sound_files",),
        "Import": ("sound_sources",),
    },
)
