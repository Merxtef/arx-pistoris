# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

from collections.abc import MutableMapping, MutableSequence, MutableSet

import pistoris


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
    model_path: pistoris.paths.ModelPath = pistoris.paths.ModelPath(
        pistoris.paths.ModelType.NPC, "human_base"
    )
    animation_path: pistoris.paths.AnimationPath = pistoris.paths.AnimationPath(
        pistoris.paths.AnimationType.NPC, "walk"
    )
    model_type: pistoris.paths.ModelType = model_path.type
    animation_type: pistoris.paths.AnimationType = animation_path.type
    model_ftl: str = pistoris.paths.model_ftl(model_path)
    animation_tea: str = pistoris.paths.animation_tea(animation_path)
    model_animation_directory: str = pistoris.paths.animation_directory(model_type)
    animation_directory: str = pistoris.paths.animation_directory(animation_type)
    resource_kind: pistoris.paths.ResourceKind = pistoris.paths.resource_selector_kind("model:npc:human_base")
    search_location: pistoris.paths.SearchLocation = pistoris.paths.model_search_location(model_type)

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

    sound: pistoris.cinematic.SoundEffectRef = cinematic.sfx.add("sound")
    sound.path = "renamed"
    sound = cinematic.sfx["renamed"]
    sound.encoded_audio = b"encoded audio"
    encoded_effect: bytes | None = sound.encoded_audio
    language: pistoris.cinematic.LanguageRef = cinematic.languages.add("english")
    language.name = "default"
    speech: pistoris.cinematic.SpeechRef = cinematic.speech.add("guard/greeting")
    speech = cinematic.speech["guard/greeting"]
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

    texture_media: bytes | None = model.mesh.textures[0].encoded_image
    model.mesh.textures[0].encoded_image = texture_media
    model.mesh.textures[0].encoded_image = None
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
    )
