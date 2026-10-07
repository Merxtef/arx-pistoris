"""Editable Arx Fatalis resources and format conversion."""

from . import ambiance as ambiance
from . import animation as animation
from . import cinematic as cinematic
from . import level as level
from . import math as math
from . import model as model
from . import native as native
from . import paths as paths
from . import resource_io as resource_io
from ._core import (
    Ambiance,
    Animation,
    Cinematic,
    DegenerateFacePolicy,
    ErrorLocation,
    FaceFlag,
    ImageFormat,
    Level,
    Model,
    NativeTextMode,
    PistorisError,
    PositionWeldMetric,
    SoundSourceReference,
    Sound,
    SoundFile,
    SoundFileSequence,
    TextEncoding,
    Texture,
    TextureFile,
    TextureFileSequence,
    __version__,
    build_time,
    classify_text_encoding,
    latin1_to_utf8,
    utf8_to_latin1,
)
from ._facade import install_immutable_values as _install_immutable_values
from ._facade import install_record_semantics as _install_record_semantics
from ._facade import publish_types as _publish_types

__all__ = [
    "Ambiance",
    "Animation",
    "Cinematic",
    "DegenerateFacePolicy",
    "ErrorLocation",
    "FaceFlag",
    "ImageFormat",
    "Level",
    "Model",
    "NativeTextMode",
    "PistorisError",
    "PositionWeldMetric",
    "SoundSourceReference",
    "Sound",
    "SoundFile",
    "SoundFileSequence",
    "TextEncoding",
    "Texture",
    "TextureFile",
    "TextureFileSequence",
    "__version__",
    "ambiance",
    "animation",
    "build_time",
    "cinematic",
    "classify_text_encoding",
    "latin1_to_utf8",
    "level",
    "math",
    "model",
    "native",
    "paths",
    "resource_io",
    "utf8_to_latin1",
]

_publish_types(globals(), __name__, __all__)
_install_immutable_values(
    globals(),
    (
        "ErrorLocation",
        "SoundFile",
        "SoundSourceReference",
        "TextureFile",
    ),
)
_install_record_semantics(
    globals(),
    {
        "ErrorLocation": (
            "domain",
            "element",
            "element_name",
            "input_index",
            "index",
            "subindex",
            "byte_offset",
            "requested_bytes",
            "source_index",
            "line",
            "sound_handle",
            "language_id",
            "label",
            "property",
            "field",
            "resource_path",
            "native_path",
            "source_path",
            "json_pointer",
            "binary_region",
            "operation",
            "mount_mask",
        ),
        "Texture": ("path", "encoded_image", "external_image_extension"),
        "TextureFile": ("source_path", "path", "encoded_image"),
        "Sound": ("path", "encoded_audio"),
        "SoundFile": ("source_path", "path", "encoded_audio"),
        "SoundSourceReference": ("sound_path", "source_path"),
    },
    {
        "Texture": ("encoded_image",),
        "TextureFile": ("encoded_image",),
        "Sound": ("encoded_audio",),
        "SoundFile": ("encoded_audio",),
    },
)
