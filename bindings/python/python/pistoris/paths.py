"""Logical Arx resource paths and selectors."""

from ._core.paths import (
    AmbianceSelector,
    AnimationSelector,
    AnimationType,
    CinematicSelector,
    EntityClassKind,
    LevelSelector,
    ModelSelector,
    ModelType,
    ResourceKind,
    SearchLocation,
    ambiance_search_location,
    ambiance_sound_directory,
    amb_from_zone_ambiance,
    animation_directory,
    animation_search_location,
    base_entity_class_from_model,
    cinematic_illustration_directory,
    cinematic_search_location,
    dlf_scene_from_level_name,
    entity_class_from_ftl,
    entity_class_from_model,
    entity_class_kind,
    ftl_from_entity_class,
    fts_from_dlf_scene,
    is_portable_filename,
    is_portable_resource_path_component,
    item_icon_from_entity_class,
    level_loading_screen,
    level_minimap,
    level_search_location,
    minimap_offsets_file,
    minimap_resource_level,
    model_from_entity_class,
    model_search_location,
    normalize_zone_ambiance,
    sanitize_portable_filename,
    selector_from_path,
    selector_from_string,
    sound_directory,
    texture_directory,
)
from ._facade import install_immutable_values as _install_immutable_values
from ._facade import install_record_semantics as _install_record_semantics
from ._facade import publish_types as _publish_types

ResourceSelector = ModelSelector | AnimationSelector | LevelSelector | CinematicSelector | AmbianceSelector

__all__ = [name for name in globals() if not name.startswith("_")]

_publish_types(globals(), __name__, __all__)
_install_immutable_values(
    globals(),
    (
        "ModelSelector",
        "AnimationSelector",
        "LevelSelector",
        "CinematicSelector",
        "AmbianceSelector",
        "SearchLocation",
    ),
)
_install_record_semantics(
    globals(),
    {
        "ModelSelector": ("type", "name", "tweak"),
        "AnimationSelector": ("type", "name"),
        "LevelSelector": ("level",),
        "CinematicSelector": ("name",),
        "AmbianceSelector": ("name",),
        "SearchLocation": ("base_path", "max_discovery_depth"),
    },
    hashable=(
        "ModelSelector",
        "AnimationSelector",
        "LevelSelector",
        "CinematicSelector",
        "AmbianceSelector",
    ),
)
