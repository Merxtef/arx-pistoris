# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

from collections.abc import MutableMapping, MutableSequence, MutableSet, Sequence
from pathlib import Path

import pistoris


def exercise_error_types(error: pistoris.PistorisError) -> None:
    if error.location is not None:
        operation: pistoris.resource_io.Operation | None = error.location.operation
        assert operation is None or operation is pistoris.resource_io.Operation.READ
        native_path: Path | None = error.location.native_path
        assert native_path is None or native_path.is_absolute()


def exercise_bulk_buffer_keywords(
    model: pistoris.Model,
    level: pistoris.Level,
    floats: object,
    indices: object,
    masks: object,
) -> None:
    model.vertices.replace(positions=floats)
    model.vertices.replace_bones(bones=indices)
    model.vertices.replace_selection_masks(masks=masks)
    model.faces.replace(
        vertex_indices=indices,
        uvs=floats,
        corner_normals=floats,
        textures=indices,
        transvals=floats,
        face_normals=floats,
        flags=indices,
    )
    model.faces.replace_textures(textures=indices)
    model.skeleton.bones.replace_selection_masks(masks=masks)
    model.action_points.replace_bones(bones=indices)
    model.action_points.replace_selection_masks(masks=masks)
    model.replace_vertices(positions=floats)

    level.vertices.replace(positions=floats)
    level.faces.replace(
        vertex_indices=indices,
        uvs=floats,
        corner_normals=floats,
        textures=indices,
        transvals=floats,
        corner_colors=floats,
        face_normals=floats,
        flags=indices,
    )
    level.faces.replace_textures(textures=indices)
    level.faces.replace_rooms(rooms=indices)
    level.anchors.replace(positions=floats, radii=floats, heights=floats, flags=indices)
    level.anchor_connections.replace(endpoints=indices)
    level.room_distances.replace(distances=floats, endpoint_portals=indices)
    level.nav_surface.replace(positions=floats, triangle_indices=indices)

    model.vertices.copy(positions=floats)
    model.vertices.copy_bones(bones=indices)
    model.vertices.copy_selection_masks(masks=masks)
    model.faces.copy(
        vertex_indices=indices, uvs=floats, corner_normals=floats, textures=indices,
        transvals=floats, face_normals=floats, flags=indices,
    )
    model.faces.copy_textures(textures=indices)
    model.skeleton.bones.copy_selection_masks(masks=masks)
    model.action_points.copy_bones(bones=indices)
    model.action_points.copy_selection_masks(masks=masks)

    level.vertices.copy(positions=floats)
    level.faces.copy(
        vertex_indices=indices, uvs=floats, corner_normals=floats, textures=indices,
        transvals=floats, corner_colors=floats, face_normals=floats, flags=indices,
    )
    level.faces.copy_textures(textures=indices)
    level.faces.copy_rooms(rooms=indices)
    level.anchors.copy(positions=floats, radii=floats, heights=floats, flags=indices)
    level.anchor_connections.copy(endpoints=indices)
    level.room_distances.copy(distances=floats, endpoint_portals=indices)
    level.nav_surface.copy(positions=floats, triangle_indices=indices)


def exercise_public_types(
    model: pistoris.Model,
    animation: pistoris.Animation,
    ambiance: pistoris.Ambiance,
    cinematic: pistoris.Cinematic,
    level: pistoris.Level,
) -> None:
    model_vertex: pistoris.model.Vertex = pistoris.model.Vertex()
    animation_keyframe: pistoris.animation.Keyframe = pistoris.animation.Keyframe()
    ambiance_track: pistoris.ambiance.PannedTrack = pistoris.ambiance.PannedTrack()
    cinematic_keyframe: pistoris.cinematic.Keyframe = pistoris.cinematic.Keyframe()
    level_zone: pistoris.level.Zone = pistoris.level.Zone()
    model_selector: pistoris.paths.ModelSelector = pistoris.paths.ModelSelector(
        pistoris.paths.ModelType.NPC, "human_base"
    )
    animation_selector: pistoris.paths.AnimationSelector = pistoris.paths.AnimationSelector(
        pistoris.paths.AnimationType.NPC, "walk"
    )
    model_type: pistoris.paths.ModelType = model_selector.type
    animation_type: pistoris.paths.AnimationType = animation_selector.type
    model_ftl: str = model_selector.to_path()
    animation_tea: str = animation_selector.to_path()
    model_animation_directory: str = pistoris.paths.animation_directory(model_type)
    animation_directory: str = pistoris.paths.animation_directory(animation_type)
    resource_kind: pistoris.paths.ResourceKind = pistoris.paths.selector_from_string("model:npc:human_base").kind
    search_location: pistoris.paths.SearchLocation = pistoris.paths.model_search_location(model_type)
    selector: pistoris.paths.ResourceSelector = pistoris.paths.selector_from_string("model:npc:human_base")
    mounts: pistoris.resource_io.ResourceMounts = pistoris.resource_io.ResourceMounts([])
    resources: pistoris.resource_io.Resources = pistoris.resource_io.Resources([])
    supplying_mount: pistoris.resource_io.mounts.Mount | None = mounts.highest_priority_mount(
        pistoris.resource_io.ALL_MOUNTS
    )
    files: list[pistoris.resource_io.mounts.ResourceFile] = mounts.list_files(max_depth=2)
    catalog: Sequence[pistoris.resource_io.catalog.Entry[pistoris.paths.ResourceSelector]] = resources.scan_catalog()
    available_mount_mask: int = mounts.available_mount_mask
    provider_mask: int = files[0].provider_mask if files else available_mount_mask
    mounted_model: pistoris.Model = resources.load_model(
        model_selector,
        text_mode=pistoris.NativeTextMode.AUTO,
        arx_units_per_glb_unit=10.0,
    ).model
    loose_model: pistoris.model.Import = resources.load_model_file(Path("model.glb"))
    mounted_report: pistoris.resource_io.output.WriteReport = resources.write_model(
        mounted_model, "editing/model.glb"
    )
    loose_plan: pistoris.resource_io.output.WritePlan = resources.prepare_model_file_write(
        loose_model, Path("model-copy.glb")
    )
    loose_report: pistoris.resource_io.output.WriteReport = loose_plan.execute()
    assert supplying_mount is None or supplying_mount.id != 0
    assert isinstance(files, list)
    assert provider_mask >= 0
    assert len(mounted_report) >= 0
    assert len(loose_report) >= 0

    origin: pistoris.model.OriginRef = model.origin
    origin.bone = origin.bone
    animation.frame_length = animation.frame_length + 1
    group: pistoris.animation.GroupRef = animation.groups[0]
    group.claimed = not group.claimed
    if not group.is_void:
        group.make_void()

    selection = model.selections["selection"]
    vertices: tuple[pistoris.model.VertexRef, ...] = selection.vertices
    bones: tuple[pistoris.model.BoneRef, ...] = selection.bones
    action_points: tuple[pistoris.model.ActionPointRef, ...] = selection.action_points
    if selection.includes_origin:
        origin.selections.discard(selection)
    else:
        origin.selections.add(selection)
    selection.leading_vertex = pistoris.model.SelectionLeadingVertex(bone=model.skeleton.bones[0].name)
    leading_vertex: pistoris.model.SelectionLeadingVertexRef | None = selection.leading_vertex
    if leading_vertex is not None:
        leading_vertex.position = pistoris.math.Vector3()
    selection.leading_vertex = None

    cinematic.sfx.append(pistoris.Sound(path="sound"))
    sound: pistoris.cinematic.SoundEffectRef = cinematic.sfx[-1]
    sound.path = "renamed"
    sound = cinematic.sfx.by_path("renamed")
    sound.encoded_audio = b"encoded audio"
    encoded_effect: bytes | None = sound.encoded_audio
    language: pistoris.cinematic.LanguageRef = cinematic.languages.add("english")
    language.name = "default"
    cinematic.speech.append(
        pistoris.cinematic.Speech(
            path="guard/greeting", encodings={language.name: b"encoded speech"}
        )
    )
    speech: pistoris.cinematic.SpeechRef = cinematic.speech[-1]
    speech = cinematic.speech.by_path("guard/greeting")
    encodings: MutableMapping[str, bytes] = speech.encodings
    encodings[language.name] = b"encoded speech"
    del encodings[language.name]
    cinematic_light: pistoris.cinematic.LightRef | None = cinematic.keyframes[0].light
    if cinematic_light is not None:
        cinematic.keyframes[0].light = cinematic_light.copy()
    cinematic.keyframes[0].light = None

    detached_selections: MutableSet[pistoris.model.Selection] = model_vertex.selections
    member_selections: MutableSet[pistoris.model.SelectionRef] = origin.selections

    zone = level.zones[0]
    perimeter: tuple[pistoris.math.Vector2, ...] = zone.perimeter_xz
    zone.perimeter_xz = perimeter

    spawn: pistoris.level.PlayerSpawnRef | None = level.player_spawn
    if spawn is not None:
        level.player_spawn = spawn.copy()
    level.player_spawn = None

    icon_ref: pistoris.model.InventoryIconRef = model.inventory_icon
    icon: pistoris.model.InventoryIcon | None = icon_ref.copy()
    minimap: pistoris.level.MinimapRef = level.minimap
    minimap_snapshot: pistoris.level.Minimap | None = minimap.copy()
    loading_screen: pistoris.level.LoadingScreenRef = level.loading_screen
    loading_screen_image: bytes | None = loading_screen.encoded_image
    loading_screen.encoded_image = loading_screen_image

    texture_media: bytes | None = model.textures[0].encoded_image
    model.textures[0].encoded_image = texture_media
    model.textures[0].encoded_image = None
    animation_media: bytes | None = animation.sounds[0].encoded_audio
    animation.sounds[0].encoded_audio = animation_media
    animation.sounds[0].encoded_audio = None

    native_ftl = pistoris.native.ftl.Data()
    native_ftl.vertices = [pistoris.native.ftl.Vertex()]
    native_ftl.vertices = (pistoris.native.ftl.Vertex() for _ in range(1))
    native_vertices: MutableSequence[pistoris.native.ftl.Vertex] = native_ftl.vertices
    native_fts = pistoris.native.fts.Data()
    native_fts.textures = {0: pistoris.native.fts.Texture()}
    native_textures: MutableMapping[int, pistoris.native.fts.Texture] = native_fts.textures

    group_transform = animation.keyframes[0].group_transforms[0].copy()
    animation.keyframes[0].group_transforms[0] = group_transform
    track_keys: pistoris.ambiance.KeyCollection = ambiance.tracks[0].keys
    track_snapshot: (
        pistoris.ambiance.PannedTrack | pistoris.ambiance.PositionedTrack
    ) = ambiance.tracks[0].copy()
    key_ref: pistoris.ambiance.KeyRef = track_keys[0]
    automation_ref: pistoris.ambiance.AutomationRef = key_ref.volume
    spatial_automation: pistoris.ambiance.AutomationRef | None = key_ref.pan
    master_track: pistoris.ambiance.TrackRef | None = ambiance.master_track
    if master_track is not None:
        ambiance.master_track = master_track

    cinematic.end_frame = cinematic.end_frame
    cinematic.fps = cinematic.fps

    if icon is not None:
        icon_ref.set(icon)
    icon_ref.clear()

    room_distance: pistoris.level.RoomDistanceRef = level.room_distances[level.rooms[0], level.rooms[1]]
    room_distance_value: pistoris.level.RoomDistance = room_distance.copy()
    level.room_distances[level.rooms[1], level.rooms[0]] = room_distance_value
    room_distance.reset()
    level.room_distances.reset()

    _ = (
        model_vertex,
        animation_keyframe,
        ambiance_track,
        cinematic_keyframe,
        sound,
        speech,
        language,
        encoded_effect,
        cinematic_light,
        detached_selections,
        member_selections,
        vertices,
        bones,
        action_points,
        level_zone,
        track_keys,
        track_snapshot,
        key_ref,
        automation_ref,
        spatial_automation,
        master_track,
        native_vertices,
        native_textures,
        icon,
        icon_ref,
        minimap,
        minimap_snapshot,
        loading_screen,
        catalog,
        mounted_model,
    )
