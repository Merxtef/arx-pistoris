"""Animation editing values, views, and conversion outputs."""

from ._core import (
    Animation,
    AnimationBytesOutput as BytesOutput,
    AnimationConversionReport as ConversionReport,
    AnimationFrame as Frame,
    AnimationGroupCollection as GroupCollection,
    AnimationGroupRef as GroupRef,
    AnimationGroupTransform as GroupTransform,
    AnimationGroupTransformCollection as GroupTransformCollection,
    AnimationGroupTransformRef as GroupTransformRef,
    AnimationImport as Import,
    AnimationKeyframe as Keyframe,
    AnimationKeyframeCollection as KeyframeCollection,
    AnimationKeyframeRef as KeyframeRef,
    AnimationNativeOutput as NativeOutput,
    AnimationSoundCollection as SoundCollection,
    AnimationSoundFile as SoundFile,
    AnimationSoundFileSequence as SoundFileSequence,
    AnimationSoundRef as SoundRef,
    AnimationSoundSource as SoundSource,
)
from ._facade import install_immutable_values as _install_immutable_values
from ._facade import install_record_semantics as _install_record_semantics
from ._facade import install_record_repr as _install_record_repr
from ._facade import publish_types as _publish_types

__all__ = [
    "Animation",
    "BytesOutput",
    "ConversionReport",
    "Frame",
    "GroupCollection",
    "GroupRef",
    "GroupTransform",
    "GroupTransformCollection",
    "GroupTransformRef",
    "Import",
    "Keyframe",
    "KeyframeCollection",
    "KeyframeRef",
    "NativeOutput",
    "SoundCollection",
    "SoundFile",
    "SoundFileSequence",
    "SoundRef",
    "SoundSource",
]

_publish_types(globals(), __name__, __all__)
_install_immutable_values(
    globals(),
    (
        "BytesOutput",
        "ConversionReport",
        "Import",
        "NativeOutput",
        "SoundFile",
        "SoundSource",
    ),
)
_install_record_semantics(
    globals(),
    {
        "GroupTransform": ("rotation", "translation", "scale"),
        "Keyframe": ("frame", "root_translation", "root_rotation", "footstep", "sound"),
        "Frame": ("keyframe", "group_transforms"),
        "ConversionReport": ("converted", "skipped"),
        "SoundSource": ("animation", "source"),
        "SoundFile": ("animation", "file"),
    },
)
_install_record_repr(
    globals(),
    {
        "NativeOutput": ("sound_files",),
        "BytesOutput": ("sound_files",),
        "Import": ("sound_sources",),
    },
)
