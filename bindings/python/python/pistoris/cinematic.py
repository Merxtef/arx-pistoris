"""Cinematic editing values, views, and conversion outputs."""

from ._core import (
    Cinematic,
    CinematicBaseEffect as BaseEffect,
    CinematicBytesOutput as BytesOutput,
    CinematicGlbOutput as GlbOutput,
    CinematicIllustration as Illustration,
    CinematicIllustrationCollection as IllustrationCollection,
    CinematicIllustrationFormat as IllustrationFormat,
    CinematicIllustrationRef as IllustrationRef,
    CinematicImport as Import,
    CinematicInterpolation as Interpolation,
    CinematicKeyframe as Keyframe,
    CinematicKeyframeCollection as KeyframeCollection,
    CinematicKeyframeRef as KeyframeRef,
    CinematicLanguage as Language,
    CinematicLanguageCollection as LanguageCollection,
    CinematicLanguageRef as LanguageRef,
    CinematicLight as Light,
    CinematicLightRef as LightRef,
    CinematicNativeOutput as NativeOutput,
    CinematicPostEffect as PostEffect,
    CinematicSound as Sound,
    CinematicSoundEffectCollection as SoundEffectCollection,
    CinematicSoundEffectRef as SoundEffectRef,
    CinematicSoundFile as SoundFile,
    CinematicSoundFileSequence as SoundFileSequence,
    CinematicSoundSourceReference as SoundSourceReference,
    CinematicSpeechCollection as SpeechCollection,
    CinematicSpeechEncodingMap as SpeechEncodingMap,
    CinematicSpeechRef as SpeechRef,
    SoundKind,
)
from ._facade import install_immutable_values as _install_immutable_values
from ._facade import install_record_semantics as _install_record_semantics
from ._facade import install_record_repr as _install_record_repr
from ._facade import publish_types as _publish_types

__all__ = [
    "BaseEffect",
    "BytesOutput",
    "Cinematic",
    "GlbOutput",
    "Illustration",
    "IllustrationCollection",
    "IllustrationFormat",
    "IllustrationRef",
    "Import",
    "Interpolation",
    "Keyframe",
    "KeyframeCollection",
    "KeyframeRef",
    "Language",
    "LanguageCollection",
    "LanguageRef",
    "Light",
    "LightRef",
    "NativeOutput",
    "PostEffect",
    "Sound",
    "SoundEffectCollection",
    "SoundEffectRef",
    "SoundFile",
    "SoundFileSequence",
    "SoundSourceReference",
    "SpeechCollection",
    "SpeechEncodingMap",
    "SpeechRef",
    "SoundKind",
]

_publish_types(globals(), __name__, __all__)
_install_immutable_values(
    globals(),
    (
        "BytesOutput",
        "GlbOutput",
        "Import",
        "NativeOutput",
        "SoundFile",
        "SoundSourceReference",
    ),
)
_install_record_semantics(
    globals(),
    {
        "Light": ("position", "fall_in", "fall_out", "color", "intensity", "random_intensity"),
        "Illustration": ("path", "encoded_image", "external_image_extension", "subdivision_scale"),
        "Keyframe": (
            "frame", "camera_position", "camera_roll", "color", "secondary_color",
            "flash_color", "flash_decay", "light", "outgoing_speed", "sound_path", "sound_kind", "interpolation",
            "base_effect", "post_effect", "crossfade", "dream",
        ),
        "Sound": ("path",),
        "SoundSourceReference": ("kind", "sound_path", "source_path"),
        "Language": ("name",),
        "SoundFile": ("kind", "source_path", "language", "path", "encoded_audio"),
    },
    {
        "SoundFile": ("encoded_audio",),
    },
)
_install_record_repr(
    globals(),
    {
        "NativeOutput": ("illustration_files", "sound_files"),
        "BytesOutput": ("illustration_files", "sound_files"),
        "GlbOutput": ("sound_files",),
        "Import": ("illustration_source_paths", "sound_sources"),
    },
)
