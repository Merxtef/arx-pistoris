// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "api/strerror.h"

#include "arx_pistoris/pistoris_types.h"

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
    case ARX_INVALID_XFORM:
      return "affine transform is non-positive determinant (reflection or singular)";
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
    case ARX_FTL_ORPHAN_BONE:
      return "FTL: non-root bone has no parent (origin vertex not claimed by any earlier group)";
    case ARX_FTL_MULTIPLE_ROOTS:
      return "FTL: skeleton has more than one root joint";

    // TEA
    case ARX_TEA_BAD_VERSION:
      return "TEA: unsupported version";
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
    case ARX_TEA_NON_MONOTONIC_FRAMES:
      return "TEA: keyframe num_frame values are not strictly increasing";

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

    // Level
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
    case ARX_LEVEL_BAD_FACE_NORMAL:
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
    case ARX_LEVEL_BAD_ANCHOR_CONNECTION_ORDER:
      return "Level: anchor connections are not sorted and unique";
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

    // OBJ
    case ARX_OBJ_BAD_FORMAT:
      return "OBJ: malformed syntax";
    case ARX_OBJ_BAD_VERTEX_IDX:
      return "OBJ: vertex index out of range";
    case ARX_OBJ_TOO_MANY_VERTICES:
      return "OBJ: too many vertices";
    case ARX_OBJ_TOO_MANY_NORMALS:
      return "OBJ: too many normals";
    case ARX_OBJ_TOO_MANY_TEXCOORDS:
      return "OBJ: too many texture coordinates";
    case ARX_OBJ_TOO_MANY_TEXTURES:
      return "OBJ: too many textures";
    case ARX_OBJ_TOO_MANY_MATERIALS:
      return "OBJ: too many materials";
    case ARX_OBJ_TOO_MANY_FACES:
      return "OBJ: too many faces";
    case ARX_OBJ_NO_GEOMETRY:
      return "OBJ: no geometry found";

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
    case ARX_GLB_BAD_LEVEL_MATERIAL:
      return "GLB: invalid Level material convention";
    case ARX_GLB_BAD_LEVEL_MATERIAL_RESERVED_STEM:
      return "GLB: real texture uses a reserved Level material stem";
    case ARX_GLB_BAD_LEVEL_MATERIAL_STEM_COLLISION:
      return "GLB: Level material stem resolves to conflicting texture paths";
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
    case ARX_GLB_MODEL_TOO_MANY_VERTICES:
      return "GLB: expanded model primitive vertex count exceeds uint16 max";
    case ARX_GLB_MODEL_NON_UNIFORM_SCALE:
      return "GLB: non-uniform scale or shear in mesh node chain or inverse bind matrix";
    case ARX_GLB_MODEL_MULTIPLE_SKINS:
      return "GLB: skins do not merge into a single connected armature";
    case ARX_GLB_ANIMATION_GROUP_MISMATCH:
      return "GLB: animation group count does not match model group count";
    case ARX_GLB_ANIMATION_NO_MODEL_GROUPS:
      return "GLB: animations supplied but the model has no bone groups";

    // JSON
    case ARX_JSON_BAD_FORMAT:
      return "JSON: malformed JSON";
    case ARX_JSON_BAD_SCHEMA:
      return "JSON: missing or wrong-type field";
    case ARX_JSON_LIMIT_EXCEEDED:
      return "JSON: target format limit exceeded";
    default:
      return "unknown error code";
  }
}

// NOLINTEND(readability-identifier-naming)
