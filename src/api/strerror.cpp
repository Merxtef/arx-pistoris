// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "api/strerror.h"

#include "arx_pistoris/base/status.h"

// NOLINTBEGIN(readability-identifier-naming)

ARX_STRERROR_API const char* arx_pistoris_strerror(ArxReturnCode rc) noexcept {
  switch (rc) {
    // general
    case ARX_OK:
      return "ok";
    case ARX_INVALID_IDENTIFIER:
      return "invalid format identifier";
    case ARX_INVALID_DATA_POINTER:
      return "null data pointer";
    case ARX_INVALID_HANDLE:
      return "null or invalid handle";
    case ARX_INVALID_OPTIONS:
      return "invalid operation options";
    case ARX_INDEX_OUT_OF_RANGE:
      return "index out of range";
    case ARX_BUFFER_TOO_SMALL:
      return "output buffer is too small";

    // runtime and storage
    case ARX_BAD_ALLOC:
      return "memory allocation failed";
    case ARX_UNEXPECTED_EOF:
      return "unexpected end of data";
    case ARX_INTERNAL_ERROR:
      return "internal error";
    case ARX_COMPRESSION_FAILED:
      return "native compression failed";
    case ARX_DECOMPRESSION_FAILED:
      return "native compressed data is invalid";
    case ARX_COMPRESSION_INPUT_TOO_LARGE:
      return "native compression input exceeds the format limit";
    case ARX_DECOMPRESSION_LIMIT_EXCEEDED:
      return "native decompressed data exceeds the safety limit";
    case ARX_AUDIO_BAD_DATA:
      return "encoded audio data is invalid";
    case ARX_AUDIO_UNSUPPORTED_CHANNELS:
      return "audio channel count is unsupported";
    case ARX_AUDIO_TOO_LARGE:
      return "audio data exceeds the safety limit";
    case ARX_IMAGE_BAD_DATA:
      return "encoded image data is invalid";

    // FTL
    case ARX_FTL_BAD_VERSION:
      return "FTL: unsupported version";
    case ARX_FTL_BAD_OFFSET:
      return "FTL: bad 3D data offset";
    case ARX_FTL_BAD_VERT_N:
      return "FTL: invalid vertex count";
    case ARX_FTL_BAD_ORIGIN:
      return "FTL: origin index out of range";
    case ARX_FTL_BAD_FACE_N:
      return "FTL: invalid face count";
    case ARX_FTL_BAD_FACE_TYPE:
      return "FTL: face has unknown type bits";
    case ARX_FTL_BAD_FACE_VERT_IDX:
      return "FTL: face vertex index out of range";
    case ARX_FTL_BAD_FACE_TEX:
      return "FTL: face texture id out of range";
    case ARX_FTL_BAD_TEX_N:
      return "FTL: invalid texture count";
    case ARX_FTL_BAD_GROUP_N:
      return "FTL: invalid group count";
    case ARX_FTL_BAD_GROUP_IDX_N:
      return "FTL: invalid group index count";
    case ARX_FTL_BAD_GROUP_IDX:
      return "FTL: group vertex index out of range";
    case ARX_FTL_BAD_GROUP_ORIGIN:
      return "FTL: group origin index out of range";
    case ARX_FTL_BAD_ACTION_N:
      return "FTL: invalid action count";
    case ARX_FTL_BAD_ACTION_VERT_IDX:
      return "FTL: action vertex index out of range";
    case ARX_FTL_BAD_SEL_N:
      return "FTL: invalid selection count";
    case ARX_FTL_BAD_SEL_IDX_N:
      return "FTL: invalid selection index count";
    case ARX_FTL_BAD_SEL_IDX:
      return "FTL: selection vertex index out of range";
    case ARX_FTL_BAD_SOURCE_PATH:
      return "FTL: source path is not null-terminated";
    case ARX_FTL_BAD_TEXTURE_PATH:
      return "FTL: texture path is not null-terminated";
    case ARX_FTL_BAD_GROUP_NAME:
      return "FTL: group name is not null-terminated";
    case ARX_FTL_BAD_ACTION_NAME:
      return "FTL: action name is not null-terminated";
    case ARX_FTL_BAD_SELECTION_NAME:
      return "FTL: selection name is not null-terminated";
    // TEA
    case ARX_TEA_BAD_VERSION:
      return "TEA: unsupported version";
    case ARX_TEA_BAD_NAME:
      return "TEA: animation name is not null-terminated";
    case ARX_TEA_BAD_FRAMES_N:
      return "TEA: invalid frame count";
    case ARX_TEA_BAD_GROUPS_N:
      return "TEA: invalid group count";
    case ARX_TEA_BAD_KEYFRAMES_N:
      return "TEA: invalid keyframe count";
    case ARX_TEA_BAD_FLAG_FRAME:
      return "TEA: unknown frame event flag";
    case ARX_TEA_BAD_SAMPLE_SIZE:
      return "TEA: negative sample size";
    case ARX_TEA_BAD_SAMPLE_PATH:
      return "TEA: sample path is not null-terminated";
    case ARX_TEA_NON_MONOTONIC_FRAMES:
      return "TEA: keyframe num_frame values are not strictly increasing";
    case ARX_TEA_BAD_ROOT_TRANSFORM:
      return "TEA: root transform is not finite";
    case ARX_TEA_BAD_GROUP_TRANSFORM:
      return "TEA: group transform is not finite";

    // FTS
    case ARX_FTS_BAD_VERSION:
      return "FTS: unsupported version";
    case ARX_FTS_BAD_METADATA_COUNT:
      return "FTS: invalid metadata header count";
    case ARX_FTS_BAD_GRID_SIZE:
      return "FTS: invalid grid size";
    case ARX_FTS_BAD_TEXTURE_COUNT:
      return "FTS: invalid texture count";
    case ARX_FTS_BAD_TEXTURE_ID:
      return "FTS: texture id is invalid";
    case ARX_FTS_DUPLICATE_TEXTURE_ID:
      return "FTS: duplicate texture id";
    case ARX_FTS_BAD_TEXTURE_PATH:
      return "FTS: texture path is invalid";
    case ARX_FTS_BAD_POLYGON_COUNT:
      return "FTS: invalid polygon count";
    case ARX_FTS_BAD_CELL_POLYGON_COUNT:
      return "FTS: invalid cell polygon count";
    case ARX_FTS_BAD_CELL_ANCHOR_COUNT:
      return "FTS: invalid cell anchor count";
    case ARX_FTS_BAD_POLYGON_TYPE:
      return "FTS: polygon has unknown type bits";
    case ARX_FTS_BAD_POLYGON_TEXTURE_ID:
      return "FTS: polygon texture id is invalid";
    case ARX_FTS_BAD_POLYGON_POSITION:
      return "FTS: polygon position is invalid";
    case ARX_FTS_BAD_POLYGON_UV:
      return "FTS: polygon texture coordinate is not finite";
    case ARX_FTS_BAD_POLYGON_TRANSVAL:
      return "FTS: polygon transparency value is not finite";
    case ARX_FTS_BAD_ANCHOR_COUNT:
      return "FTS: invalid anchor count";
    case ARX_FTS_BAD_ANCHOR_POSITION:
      return "FTS: anchor position is invalid";
    case ARX_FTS_BAD_ANCHOR_LINK_COUNT:
      return "FTS: invalid linked anchor count";
    case ARX_FTS_BAD_ANCHOR_INDEX:
      return "FTS: anchor index out of range";
    case ARX_FTS_BAD_PORTAL_COUNT:
      return "FTS: invalid portal count";
    case ARX_FTS_BAD_ROOM_PORTAL_INDEX:
      return "FTS: room portal index out of range";
    case ARX_FTS_BAD_PORTAL_ROOM_INDEX:
      return "FTS: portal room index out of range";
    case ARX_FTS_BAD_PORTAL_TYPE:
      return "FTS: portal polygon has invalid type";
    case ARX_FTS_BAD_PORTAL_POSITION:
      return "FTS: portal position is invalid";
    case ARX_FTS_BAD_PORTAL_GEOMETRY:
      return "FTS: portal geometry is invalid";
    case ARX_FTS_BAD_ROOM_COUNT:
      return "FTS: invalid room count";
    case ARX_FTS_BAD_ROOM_PORTAL_COUNT:
      return "FTS: invalid room portal count";
    case ARX_FTS_BAD_ROOM_POLYGON_COUNT:
      return "FTS: invalid room polygon count";
    case ARX_FTS_BAD_ROOM_TEXTURE_VERTEX_COUNT:
      return "FTS: too many render vertices for one room texture resource";
    case ARX_FTS_BAD_ROOM_POLYGON_INDEX:
      return "FTS: room polygon reference out of range";
    case ARX_FTS_BAD_ROOM_DISTANCE_COUNT:
      return "FTS: invalid room distance count";
    case ARX_FTS_BAD_ROOM_DISTANCE:
      return "FTS: invalid room distance";
    case ARX_FTS_BAD_SCENE_OFFSET:
      return "FTS: scene offset is not finite";

    // LLF
    case ARX_LLF_BAD_LIGHT_COUNT:
      return "LLF: invalid light count";
    case ARX_LLF_BAD_BAKED_COLOR_COUNT:
      return "LLF: invalid baked color count";
    case ARX_LLF_BAD_LIGHT_POSITION:
      return "LLF: light position is not finite";
    case ARX_LLF_BAD_LIGHT_COLOR:
      return "LLF: light color is outside [0,1]";
    case ARX_LLF_BAD_LIGHT_FALLOFF:
      return "LLF: light falloff range is invalid";
    case ARX_LLF_BAD_LIGHT_INTENSITY:
      return "LLF: light intensity is invalid";
    case ARX_LLF_BAD_LIGHT_EFFECT:
      return "LLF: light effect data is not finite";
    case ARX_LLF_BAD_LIGHT_FLAGS:
      return "LLF: light has unknown flag bits";
    case ARX_LLF_BAD_BAKED_COLOR:
      return "LLF: baked color is outside [0,1]";

    // DLF
    case ARX_DLF_BAD_VERSION:
      return "DLF: unsupported version";
    case ARX_DLF_BAD_SCENE_COUNT:
      return "DLF: invalid scene count";
    case ARX_DLF_BAD_SCENE_PATH:
      return "DLF: scene path is invalid";
    case ARX_DLF_BAD_PLAYER_SPAWN:
      return "DLF: player spawn is invalid";
    case ARX_DLF_BAD_ENTITY_COUNT:
      return "DLF: invalid entity count";
    case ARX_DLF_BAD_ENTITY_CLASS_PATH:
      return "DLF: entity class path is invalid";
    case ARX_DLF_BAD_ENTITY_POSITION:
      return "DLF: entity position is not finite";
    case ARX_DLF_BAD_ENTITY_ANGLE:
      return "DLF: entity angle is not finite";
    case ARX_DLF_BAD_AI_NODE_COUNT:
      return "DLF: invalid AI node count or extent";
    case ARX_DLF_BAD_AI_NODE_LINK_COUNT:
      return "DLF: invalid AI node-link count";
    case ARX_DLF_BAD_EMBEDDED_LIGHT_COUNT:
      return "DLF: invalid embedded light count";
    case ARX_DLF_BAD_EMBEDDED_COLOR_COUNT:
      return "DLF: invalid embedded lighting color count";
    case ARX_DLF_BAD_FOG_COUNT:
      return "DLF: invalid fog count";
    case ARX_DLF_BAD_FOG_POSITION:
      return "DLF: fog position is not finite";
    case ARX_DLF_BAD_FOG_COLOR:
      return "DLF: fog color is not finite";
    case ARX_DLF_BAD_FOG_ANGLE:
      return "DLF: fog angle is not finite";
    case ARX_DLF_BAD_FOG_EFFECT:
      return "DLF: fog effect data is not finite";
    case ARX_DLF_BAD_PATH_RECORD_COUNT:
      return "DLF: invalid combined zone and path count";
    case ARX_DLF_BAD_PATH_NODE_COUNT:
      return "DLF: invalid path node count";
    case ARX_DLF_BAD_ZONE_NAME:
      return "DLF: zone name is invalid";
    case ARX_DLF_BAD_ZONE_POSITION:
      return "DLF: zone position is not finite";
    case ARX_DLF_BAD_ZONE_POINT_COUNT:
      return "DLF: invalid zone point count";
    case ARX_DLF_BAD_ZONE_POINT:
      return "DLF: zone point is not finite";
    case ARX_DLF_BAD_ZONE_HEIGHT:
      return "DLF: zone height is invalid";
    case ARX_DLF_BAD_ZONE_COLOR:
      return "DLF: zone color is not finite";
    case ARX_DLF_BAD_ZONE_FARCLIP:
      return "DLF: zone farclip is not finite";
    case ARX_DLF_BAD_ZONE_AMBIANCE:
      return "DLF: zone ambiance is invalid";
    case ARX_DLF_BAD_PATH_NAME:
      return "DLF: path name is invalid";
    case ARX_DLF_BAD_PATH_POSITION:
      return "DLF: path position is not finite";
    case ARX_DLF_BAD_PATH_NODE_POSITION:
      return "DLF: path node position is not finite";
    case ARX_DLF_BAD_PATH_NODE_TYPE:
      return "DLF: path node type is invalid";
    case ARX_DLF_BAD_PATH_FIRST_NODE:
      return "DLF: first path node is not the required origin node";

    // AMB
    case ARX_AMB_BAD_VERSION:
      return "AMB: unsupported version";
    case ARX_AMB_BAD_TRACK_COUNT:
      return "AMB: invalid track count";
    case ARX_AMB_BAD_MASTER_COUNT:
      return "AMB: expected exactly one master track";
    case ARX_AMB_BAD_SAMPLE_PATH:
      return "AMB: sample path is invalid";
    case ARX_AMB_BAD_KEY_COUNT:
      return "AMB: invalid track key count";
    case ARX_AMB_BAD_KEY_TIMING:
      return "AMB: key delay range is invalid";
    case ARX_AMB_BAD_SETTING:
      return "AMB: key setting is invalid";
    case ARX_AMB_UNUSED_SETTING_DATA:
      return "AMB: unused key setting data must be zero";

    // Level
    case ARX_LEVEL_BAD_RESOURCE_PATH:
      return "Level: resource path is invalid";
    case ARX_LEVEL_BAD_MINIMAP_IMAGE:
      return "Level: minimap image is invalid";
    case ARX_LEVEL_BAD_MINIMAP_BOUNDS:
      return "Level: minimap world bounds are invalid";
    case ARX_LEVEL_BAD_LOADING_SCREEN_IMAGE:
      return "Level: loading screen image is invalid";
    case ARX_LEVEL_NO_GEOMETRY:
      return "Level: no geometry";
    case ARX_LEVEL_TOO_MANY_VERTICES:
      return "Level: too many vertices";
    case ARX_LEVEL_BAD_VERTEX_POSITION:
      return "Level: vertex position is not finite";
    case ARX_LEVEL_VERTEX_OUT_OF_BOUNDS:
      return "Level: vertex is outside the game world bounds";
    case ARX_LEVEL_TOO_MANY_TEXTURES:
      return "Level: too many textures";
    case ARX_LEVEL_BAD_TEXTURE_PATH:
      return "Level: texture path is invalid";
    case ARX_LEVEL_BAD_TEXTURE_IMAGE:
      return "Level: texture image is invalid or unsupported";
    case ARX_LEVEL_TOO_MANY_FACES:
      return "Level: too many faces";
    case ARX_LEVEL_BAD_FACE_VERTEX:
      return "Level: face vertex index is out of range or duplicated";
    case ARX_LEVEL_BAD_FACE_TEXTURE:
      return "Level: face texture index out of range";
    case ARX_LEVEL_BAD_FACE_TYPE:
      return "Level: face has unsupported or unknown type bits";
    case ARX_LEVEL_BAD_CORNER_NORMAL:
      return "Level: face corner normal is invalid";
    case ARX_LEVEL_BAD_FACE_UV:
      return "Level: face corner texture coordinate is not finite";
    case ARX_LEVEL_BAD_FACE_TRANSVAL:
      return "Level: face transparency value is not finite";
    case ARX_LEVEL_DEGENERATE_FACE:
      return "Level: face is degenerate";
    case ARX_LEVEL_BAD_VERTEX_WELD_SEGMENT:
      return "Level: vertex weld segment is invalid";
    case ARX_LEVEL_OVERLAPPING_VERTEX_WELD_SEGMENTS:
      return "Level: unprotected vertex belongs to multiple weld segments";
    case ARX_LEVEL_NO_ROOMS:
      return "Level: no rooms";
    case ARX_LEVEL_TOO_MANY_ROOMS:
      return "Level: too many rooms";
    case ARX_LEVEL_BAD_ROOM_NAME:
      return "Level: room name is invalid";
    case ARX_LEVEL_DUPLICATE_ROOM_NAME:
      return "Level: room names are not unique";
    case ARX_LEVEL_BAD_FACE_ROOM_COUNT:
      return "Level: face room count does not match face count";
    case ARX_LEVEL_BAD_FACE_ROOM_INDEX:
      return "Level: face room index out of range";
    case ARX_LEVEL_BAD_ROOM_DISTANCE_COUNT:
      return "Level: room distance count does not match room count";
    case ARX_LEVEL_BAD_ROOM_DISTANCE:
      return "Level: room distance is invalid";
    case ARX_LEVEL_TOO_MANY_PORTALS:
      return "Level: too many portals";
    case ARX_LEVEL_BAD_PORTAL_NAME:
      return "Level: portal name is invalid";
    case ARX_LEVEL_DUPLICATE_PORTAL_NAME:
      return "Level: portal names are not unique";
    case ARX_LEVEL_BAD_PORTAL_ROOM:
      return "Level: portal room reference is invalid";
    case ARX_LEVEL_BAD_PORTAL_SHAPE:
      return "Level: portal shape is invalid";
    case ARX_LEVEL_BAD_PORTAL_VERTEX:
      return "Level: portal vertex is invalid";
    case ARX_LEVEL_PORTAL_OUT_OF_BOUNDS:
      return "Level: portal is outside the game world bounds";
    case ARX_LEVEL_DEGENERATE_PORTAL:
      return "Level: portal is degenerate";
    case ARX_LEVEL_NON_PLANAR_PORTAL:
      return "Level: portal is not planar";
    case ARX_LEVEL_INCONSISTENT_PORTAL_ORIENTATION:
      return "Level: portal triangles have inconsistent orientation";
    case ARX_LEVEL_SELF_INTERSECTING_PORTAL:
      return "Level: portal is self-intersecting";
    case ARX_LEVEL_BAD_NAV_SURFACE:
      return "Level: navigation surface is empty";
    case ARX_LEVEL_TOO_MANY_NAV_SURFACE_VERTICES:
      return "Level: too many navigation surface vertices";
    case ARX_LEVEL_BAD_NAV_SURFACE_VERTEX:
      return "Level: navigation surface vertex is invalid";
    case ARX_LEVEL_BAD_NAV_SURFACE_TRIANGLE:
      return "Level: navigation surface triangle index is invalid";
    case ARX_LEVEL_DEGENERATE_NAV_SURFACE_TRIANGLE:
      return "Level: navigation surface triangle is degenerate";
    case ARX_LEVEL_NAV_SURFACE_REQUIRED:
      return "Level: navigation surface is required for this operation";
    case ARX_LEVEL_EMPTY_NAVIGATION_RESULT:
      return "Level: navigation generation produced no usable result";
    case ARX_LEVEL_TOO_MANY_ANCHORS:
      return "Level: too many anchors";
    case ARX_LEVEL_BAD_ANCHOR_NAME:
      return "Level: anchor name is invalid";
    case ARX_LEVEL_DUPLICATE_ANCHOR_NAME:
      return "Level: anchor name is duplicated";
    case ARX_LEVEL_BAD_ANCHOR_POSITION:
      return "Level: anchor position is not finite";
    case ARX_LEVEL_BAD_ANCHOR_RADIUS:
      return "Level: anchor radius is invalid";
    case ARX_LEVEL_BAD_ANCHOR_HEIGHT:
      return "Level: anchor height is invalid";
    case ARX_LEVEL_BAD_ANCHOR_FLAGS:
      return "Level: anchor has unknown flag bits";
    case ARX_LEVEL_ANCHOR_OUT_OF_BOUNDS:
      return "Level: anchor is outside native X/Z bounds";
    case ARX_LEVEL_TOO_MANY_ANCHOR_CONNECTIONS:
      return "Level: too many anchor connections";
    case ARX_LEVEL_BAD_ANCHOR_CONNECTION_INDEX:
      return "Level: anchor connection index is invalid";
    case ARX_LEVEL_DUPLICATE_ANCHOR_CONNECTION:
      return "Level: anchor connection is duplicated";
    case ARX_LEVEL_BAD_ANCHOR_CONNECTION_ORDER:
      return "Level: anchor connections are not sorted";
    case ARX_LEVEL_TOO_MANY_LIGHTS:
      return "Level: too many lights";
    case ARX_LEVEL_BAD_LIGHT_NAME:
      return "Level: light name is invalid";
    case ARX_LEVEL_DUPLICATE_LIGHT_NAME:
      return "Level: light names are not unique";
    case ARX_LEVEL_BAD_LIGHT_POSITION:
      return "Level: light position is not finite";
    case ARX_LEVEL_BAD_LIGHT_COLOR:
      return "Level: light color is outside [0,1]";
    case ARX_LEVEL_BAD_LIGHT_FALLOFF:
      return "Level: light falloff range is invalid";
    case ARX_LEVEL_BAD_LIGHT_INTENSITY:
      return "Level: light intensity is invalid";
    case ARX_LEVEL_BAD_LIGHT_EFFECT:
      return "Level: light effect data is not finite";
    case ARX_LEVEL_BAD_LIGHT_FLAGS:
      return "Level: light has unknown flag bits";
    case ARX_LEVEL_BAD_CORNER_COLOR_COUNT:
      return "Level: baked corner color count does not match face count";
    case ARX_LEVEL_BAD_CORNER_COLOR:
      return "Level: baked corner color is outside [0,1]";
    case ARX_LEVEL_BAD_PLAYER_SPAWN:
      return "Level: player spawn is invalid";
    case ARX_LEVEL_TOO_MANY_ENTITIES:
      return "Level: too many entities";
    case ARX_LEVEL_BAD_ENTITY_NAME:
      return "Level: entity display name is invalid";
    case ARX_LEVEL_DUPLICATE_ENTITY_NAME:
      return "Level: entity display names are not unique";
    case ARX_LEVEL_BAD_ENTITY_CLASS_PATH:
      return "Level: entity class path is invalid";
    case ARX_LEVEL_BAD_ENTITY_POSITION:
      return "Level: entity position is not finite";
    case ARX_LEVEL_BAD_ENTITY_ROTATION:
      return "Level: entity rotation is invalid";
    case ARX_LEVEL_TOO_MANY_FOGS:
      return "Level: too many fogs";
    case ARX_LEVEL_BAD_FOG_NAME:
      return "Level: fog name is invalid";
    case ARX_LEVEL_DUPLICATE_FOG_NAME:
      return "Level: fog names are not unique";
    case ARX_LEVEL_BAD_FOG_POSITION:
      return "Level: fog position is not finite";
    case ARX_LEVEL_BAD_FOG_ROTATION:
      return "Level: fog rotation is invalid";
    case ARX_LEVEL_BAD_FOG_COLOR:
      return "Level: fog color is not finite";
    case ARX_LEVEL_BAD_FOG_EFFECT:
      return "Level: fog effect data is not finite";
    case ARX_LEVEL_TOO_MANY_ZONES:
      return "Level: too many zones";
    case ARX_LEVEL_BAD_ZONE_NAME:
      return "Level: zone name is invalid";
    case ARX_LEVEL_DUPLICATE_ZONE_NAME:
      return "Level: zone names are not unique";
    case ARX_LEVEL_BAD_ZONE_PERIMETER:
      return "Level: zone perimeter is invalid";
    case ARX_LEVEL_BAD_ZONE_HEIGHT_MODE:
      return "Level: zone height mode is invalid";
    case ARX_LEVEL_BAD_ZONE_HEIGHT:
      return "Level: finite zone height is invalid";
    case ARX_LEVEL_BAD_ZONE_COLOR:
      return "Level: zone color is not finite";
    case ARX_LEVEL_BAD_ZONE_FARCLIP:
      return "Level: zone farclip is not finite";
    case ARX_LEVEL_BAD_ZONE_AMBIANCE:
      return "Level: zone ambiance is invalid";
    case ARX_LEVEL_TOO_MANY_PATHS:
      return "Level: too many paths";
    case ARX_LEVEL_BAD_PATH_NAME:
      return "Level: path name is invalid";
    case ARX_LEVEL_DUPLICATE_PATH_NAME:
      return "Level: path names are not unique";
    case ARX_LEVEL_BAD_PATH_POSITION:
      return "Level: path position is not finite";
    case ARX_LEVEL_BAD_PATH_NODE_COUNT:
      return "Level: path has no nodes or too many nodes";
    case ARX_LEVEL_BAD_PATH_NODE_POSITION:
      return "Level: path node position is not finite";
    case ARX_LEVEL_BAD_PATH_NODE_TYPE:
      return "Level: path node type is invalid";
    case ARX_LEVEL_BAD_PATH_FIRST_NODE:
      return "Level: first path node is not the required origin node";

    // Model
    case ARX_MODEL_BAD_RESOURCE_PATH:
      return "Model: resource path is invalid";
    case ARX_MODEL_BAD_INVENTORY_ICON:
      return "Model: inventory icon is malformed";
    case ARX_MODEL_NO_GEOMETRY:
      return "Model: no usable geometry";
    case ARX_MODEL_TOO_MANY_VERTICES:
      return "Model: too many vertices";
    case ARX_MODEL_TOO_MANY_NATIVE_VERTICES:
      return "Model: native vertex expansion exceeds uint16 max";
    case ARX_MODEL_TOO_MANY_FACES:
      return "Model: too many faces";
    case ARX_MODEL_TOO_MANY_TEXTURES:
      return "Model: too many textures";
    case ARX_MODEL_BAD_VERTEX_POSITION:
      return "Model: vertex position is not finite";
    case ARX_MODEL_BAD_TEXTURE_PATH:
      return "Model: texture path is invalid";
    case ARX_MODEL_BAD_TEXTURE_IMAGE:
      return "Model: texture image is malformed";
    case ARX_MODEL_BAD_FACE_TEXTURE:
      return "Model: face texture index is invalid";
    case ARX_MODEL_BAD_FACE_VERTEX:
      return "Model: face vertex index is invalid";
    case ARX_MODEL_BAD_FACE_TYPE:
      return "Model: face flags are invalid";
    case ARX_MODEL_BAD_FACE_TRANSVAL:
      return "Model: face transparency value is not finite";
    case ARX_MODEL_BAD_FACE_NORMAL:
      return "Model: face normal is invalid";
    case ARX_MODEL_BAD_CORNER_NORMAL:
      return "Model: face corner normal is invalid";
    case ARX_MODEL_BAD_FACE_UV:
      return "Model: face texture coordinates are not finite";
    case ARX_MODEL_DEGENERATE_FACE:
      return "Model: face is degenerate";
    case ARX_MODEL_TOO_MANY_BONES:
      return "Model: too many bones";
    case ARX_MODEL_BAD_BONE_NAME:
      return "Model: bone name is invalid";
    case ARX_MODEL_DUPLICATE_BONE_NAME:
      return "Model: bone names are not unique";
    case ARX_MODEL_BAD_BONE_POSITION:
      return "Model: bone position is not finite";
    case ARX_MODEL_BAD_BONE_PARENT:
      return "Model: bone parent is invalid or not ordered before the child";
    case ARX_MODEL_BAD_BONE_BLOB_SHADOW_SIZE:
      return "Model: bone blob shadow size is invalid";
    case ARX_MODEL_BAD_VERTEX_BONE_COUNT:
      return "Model: vertex bone count does not match vertex count";
    case ARX_MODEL_BAD_VERTEX_BONE:
      return "Model: vertex bone index is invalid";
    case ARX_MODEL_BAD_ORIGIN_BONE:
      return "Model: origin bone index is invalid";
    case ARX_MODEL_BONE_IN_USE:
      return "Model: bone is still referenced";
    case ARX_MODEL_REFERENCE_BONE_COUNT_MISMATCH:
      return "Model: reference bone count does not match";
    case ARX_MODEL_REFERENCE_BONE_TOPOLOGY_MISMATCH:
      return "Model: reference bone topology does not match";
    case ARX_MODEL_TOO_MANY_ACTION_POINTS:
      return "Model: too many action points";
    case ARX_MODEL_BAD_ACTION_POINT_NAME:
      return "Model: action point name is invalid";
    case ARX_MODEL_BAD_ACTION_POINT_POSITION:
      return "Model: action point position is not finite";
    case ARX_MODEL_BAD_ACTION_POINT_BONE:
      return "Model: action point bone index is invalid";
    case ARX_MODEL_TOO_MANY_SELECTIONS:
      return "Model: too many selections";
    case ARX_MODEL_BAD_SELECTION_NAME:
      return "Model: selection name is invalid";
    case ARX_MODEL_DUPLICATE_SELECTION_NAME:
      return "Model: selection names are not unique";
    case ARX_MODEL_BAD_SELECTION_LEADING_POSITION:
      return "Model: selection leading vertex position is not finite";
    case ARX_MODEL_BAD_SELECTION_LEADING_BONE:
      return "Model: selection leading vertex bone index is invalid";
    case ARX_MODEL_BAD_SELECTION_VERTEX:
      return "Model: selection vertex index is invalid";
    case ARX_MODEL_BAD_SELECTION_BONE:
      return "Model: selection bone index is invalid";
    case ARX_MODEL_BAD_SELECTION_ACTION_POINT:
      return "Model: selection action point index is invalid";

    // Animation
    case ARX_ANIMATION_BAD_RESOURCE_PATH:
      return "Animation: resource path is invalid";
    case ARX_ANIMATION_BAD_NAME:
      return "Animation: name is invalid";
    case ARX_ANIMATION_TOO_MANY_SOUNDS:
      return "Animation: too many sounds";
    case ARX_ANIMATION_BAD_SOUND_PATH:
      return "Animation: sound path is invalid";
    case ARX_ANIMATION_BAD_SOUND_DATA:
      return "Animation: encoded sound data is invalid";
    case ARX_ANIMATION_UNSUPPORTED_SOUND_CHANNELS:
      return "Animation: sound channel count is unsupported";
    case ARX_ANIMATION_SOUND_TOO_LARGE:
      return "Animation: encoded sound is too large";
    case ARX_ANIMATION_DUPLICATE_SOUND_PATH:
      return "Animation: sound path is duplicated";
    case ARX_ANIMATION_SOUND_IN_USE:
      return "Animation: sound is still referenced by a keyframe";
    case ARX_ANIMATION_NO_KEYFRAMES:
      return "Animation: no keyframes";
    case ARX_ANIMATION_TOO_MANY_KEYFRAMES:
      return "Animation: too many keyframes";
    case ARX_ANIMATION_TOO_MANY_GROUPS:
      return "Animation: too many groups";
    case ARX_ANIMATION_BAD_FRAME_LENGTH:
      return "Animation: frame length is invalid";
    case ARX_ANIMATION_BAD_FRAME:
      return "Animation: keyframe number is invalid";
    case ARX_ANIMATION_BAD_ROOT_TRANSFORM:
      return "Animation: root transform is invalid";
    case ARX_ANIMATION_BAD_GROUP_TRANSFORM:
      return "Animation: group transform is invalid";
    case ARX_ANIMATION_BAD_KEYFRAME_SOUND:
      return "Animation: keyframe sound index is invalid";
    case ARX_ANIMATION_BAD_TRANSFORM_COUNT:
      return "Animation: group transform count is invalid";
    case ARX_ANIMATION_BAD_GROUP_CLAIM:
      return "Animation: group claim is outside the timeline group range";

    // Ambiance
    case ARX_AMBIANCE_NO_TRACKS:
      return "Ambiance: no tracks";
    case ARX_AMBIANCE_TOO_MANY_TRACKS:
      return "Ambiance: too many tracks";
    case ARX_AMBIANCE_TOO_MANY_SOUNDS:
      return "Ambiance: too many sounds";
    case ARX_AMBIANCE_BAD_RESOURCE_PATH:
      return "Ambiance: resource path is invalid";
    case ARX_AMBIANCE_BAD_MASTER_TRACK:
      return "Ambiance: master track is invalid";
    case ARX_AMBIANCE_BAD_SOUND_PATH:
      return "Ambiance: sound path is invalid";
    case ARX_AMBIANCE_BAD_SOUND_DATA:
      return "Ambiance: encoded sound data is invalid";
    case ARX_AMBIANCE_UNSUPPORTED_SOUND_CHANNELS:
      return "Ambiance: sound channel count is unsupported";
    case ARX_AMBIANCE_SOUND_TOO_LARGE:
      return "Ambiance: decoded sound data is too large";
    case ARX_AMBIANCE_DUPLICATE_SOUND_PATH:
      return "Ambiance: sound path is not unique";
    case ARX_AMBIANCE_BAD_TRACK_SOUND:
      return "Ambiance: track sound index is invalid";
    case ARX_AMBIANCE_SOUND_IN_USE:
      return "Ambiance: sound is referenced by a track";
    case ARX_AMBIANCE_BAD_KEY_COUNT:
      return "Ambiance: track key count is invalid";
    case ARX_AMBIANCE_BAD_PLAY_COUNT:
      return "Ambiance: key play count is invalid";
    case ARX_AMBIANCE_BAD_KEY_TIMING:
      return "Ambiance: key delay range is invalid";
    case ARX_AMBIANCE_BAD_AUTOMATION:
      return "Ambiance: key automation is invalid";
    case ARX_AMBIANCE_SOUND_DATA_REQUIRED:
      return "Ambiance: encoded sound data is required";
    case ARX_AMBIANCE_TRACK_CANNOT_FIT_MASTER:
      return "Ambiance: track cannot fit within the master duration";

    // OBJ
    case ARX_OBJ_BAD_FORMAT:
      return "OBJ: malformed syntax";
    case ARX_OBJ_BAD_VERTEX_IDX:
      return "OBJ: vertex index out of range";
    case ARX_OBJ_TOO_MANY_VERTICES:
      return "OBJ: too many vertices";
    case ARX_OBJ_TOO_MANY_TEXTURES:
      return "OBJ: too many textures";
    case ARX_OBJ_TOO_MANY_FACES:
      return "OBJ: too many faces";
    case ARX_OBJ_NO_GEOMETRY:
      return "OBJ: no geometry found";
    case ARX_OBJ_BAD_POSITION_INDEX:
      return "OBJ: position index out of range";
    case ARX_OBJ_BAD_TEXCOORD_INDEX:
      return "OBJ: texture-coordinate index out of range";
    case ARX_OBJ_BAD_NORMAL_INDEX:
      return "OBJ: normal index out of range";
    case ARX_OBJ_BAD_FACE:
      return "OBJ: invalid face";
    case ARX_OBJ_BAD_MTL:
      return "OBJ: invalid material library";
    case ARX_OBJ_BAD_ACTION_POINT:
      return "OBJ: invalid action point";
    case ARX_OBJ_TOO_MANY_ACTION_POINTS:
      return "OBJ: too many action points";
    case ARX_OBJ_BAD_MATERIAL_LIBRARY_NAME:
      return "OBJ: invalid material-library name";
    case ARX_OBJ_BAD_MATERIAL_NAME:
      return "OBJ: invalid material name";

    // GLB
    case ARX_GLB_BAD_FORMAT:
      return "GLB: malformed GLB container or GLTF JSON";
    case ARX_GLB_UNSUPPORTED_FEATURE:
      return "GLB: unsupported feature (sparse accessor, external buffer, etc)";
    case ARX_GLB_AMBIGUOUS_SCENE:
      return "GLB: scene selection is ambiguous";
    case ARX_GLB_NO_LEVEL_GEOMETRY:
      return "GLB: no usable Level geometry";
    case ARX_GLB_BAD_LEVEL_GEOMETRY:
      return "GLB: invalid Level geometry convention";
    case ARX_GLB_BAD_LEVEL_POSITION_ATTRIBUTE:
      return "GLB: invalid Level POSITION attribute";
    case ARX_GLB_BAD_LEVEL_INDEX_ACCESSOR:
      return "GLB: invalid Level primitive index accessor";
    case ARX_GLB_BAD_LEVEL_NORMAL_ATTRIBUTE:
      return "GLB: invalid Level NORMAL attribute";
    case ARX_GLB_BAD_LEVEL_TEXCOORD_ATTRIBUTE:
      return "GLB: missing or invalid Level TEXCOORD attribute";
    case ARX_GLB_BAD_LEVEL_COLOR_ATTRIBUTE:
      return "GLB: invalid Level COLOR attribute";
    case ARX_GLB_BAD_LEVEL_MATERIAL:
      return "GLB: invalid Level material convention";
    case ARX_GLB_BAD_LEVEL_MATERIAL_RESERVED_STEM:
      return "GLB: real texture uses a reserved Level material stem";
    case ARX_GLB_BAD_LEVEL_HIERARCHY:
      return "GLB: invalid Level node hierarchy";
    case ARX_GLB_BAD_LEVEL_ROOM:
      return "GLB: invalid Level room convention";
    case ARX_GLB_BAD_LEVEL_PORTAL:
      return "GLB: invalid Level portal convention";
    case ARX_GLB_BAD_LEVEL_NAV_SURFACE:
      return "GLB: invalid Level navigation surface convention";
    case ARX_GLB_BAD_LEVEL_ANCHOR:
      return "GLB: invalid Level anchor convention";
    case ARX_GLB_BAD_LEVEL_PLAYER_SPAWN:
      return "GLB: invalid Level player spawn convention";
    case ARX_GLB_BAD_LEVEL_ENTITY:
      return "GLB: invalid Level entity convention";
    case ARX_GLB_BAD_LEVEL_FOG:
      return "GLB: invalid Level fog convention";
    case ARX_GLB_BAD_LEVEL_ZONE:
      return "GLB: invalid Level zone convention";
    case ARX_GLB_BAD_LEVEL_PATH:
      return "GLB: invalid Level path convention";
    case ARX_GLB_BAD_LEVEL_LIGHT:
      return "GLB: invalid Level light convention";
    case ARX_GLB_NO_MODEL_GEOMETRY:
      return "GLB: no Model geometry";
    case ARX_GLB_BAD_MODEL_GEOMETRY:
      return "GLB: invalid Model geometry";
    case ARX_GLB_BAD_MODEL_POSITION_ATTRIBUTE:
      return "GLB: invalid Model POSITION attribute";
    case ARX_GLB_BAD_MODEL_INDEX_ACCESSOR:
      return "GLB: invalid Model primitive index accessor";
    case ARX_GLB_BAD_MODEL_NORMAL_ATTRIBUTE:
      return "GLB: invalid Model NORMAL attribute";
    case ARX_GLB_BAD_MODEL_TEXCOORD_ATTRIBUTE:
      return "GLB: missing or invalid Model TEXCOORD attribute";
    case ARX_GLB_BAD_MODEL_MATERIAL:
      return "GLB: invalid Model material";
    case ARX_GLB_BAD_MODEL_HIERARCHY:
      return "GLB: invalid Model node hierarchy";
    case ARX_GLB_BAD_MODEL_SKELETON:
      return "GLB: invalid Model skeleton convention";
    case ARX_GLB_BAD_MODEL_SKINNING:
      return "GLB: invalid Model skinning data";
    case ARX_GLB_BAD_MODEL_BONE_HELPER:
      return "GLB: invalid Model bone helper convention";
    case ARX_GLB_BAD_MODEL_ACTION_POINT:
      return "GLB: invalid Model action point convention";
    case ARX_GLB_BAD_MODEL_SELECTION:
      return "GLB: invalid Model selection convention";
    case ARX_GLB_MODEL_NON_UNIFORM_SCALE:
      return "GLB: unsupported Model reflection, non-uniform scale, or shear";
    case ARX_GLB_BAD_ANIMATION_NAME:
      return "GLB: animation name is missing or invalid";
    case ARX_GLB_BAD_ANIMATION_HELPER:
      return "GLB: animation helper is invalid";
    case ARX_GLB_BAD_ANIMATION_SAMPLER:
      return "GLB: animation sampler is invalid";
    case ARX_GLB_BAD_ANIMATION_CHANNEL:
      return "GLB: animation channel is invalid";
    case ARX_GLB_BAD_ANIMATION_BINDING:
      return "GLB: animation cannot bind to the Model skeleton";
    case ARX_GLB_NO_AMBIANCE:
      return "GLB: no Ambiance root";
    case ARX_GLB_AMBIGUOUS_AMBIANCE:
      return "GLB: multiple Ambiance roots";
    case ARX_GLB_BAD_AMBIANCE_ROOT:
      return "GLB: invalid Ambiance root convention";
    case ARX_GLB_BAD_AMBIANCE_TRACK:
      return "GLB: invalid Ambiance track convention";
    case ARX_GLB_BAD_AMBIANCE_KEY:
      return "GLB: invalid Ambiance key convention";
    case ARX_GLB_BAD_AMBIANCE_AUTOMATION:
      return "GLB: invalid Ambiance automation convention";

    // JSON
    case ARX_JSON_BAD_FORMAT:
      return "JSON: malformed JSON";
    case ARX_JSON_BAD_SCHEMA:
      return "JSON: missing or wrong-type field";
    case ARX_JSON_LIMIT_EXCEEDED:
      return "JSON: target format limit exceeded";
    case ARX_JSON_UNREPRESENTABLE_VALUE:
      return "JSON: value cannot be represented";
    default:
      return "unknown error code";
  }
}

// NOLINTEND(readability-identifier-naming)
