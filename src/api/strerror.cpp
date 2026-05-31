// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "api/strerror.h"

// NOLINTBEGIN(readability-identifier-naming)

ARX_STRERROR_API const char* arx_pistoris_strerror(ArxReturnCode rc) {
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

    // internal
    case ARX_BAD_ALLOC:
      return "memory allocation failed";
    case ARX_UNEXPECTED_EOF:
      return "unexpected end of data";
    case ARX_UNKNOWN_ERROR:
      return "unknown internal error";

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
    case ARX_GLB_TEA_GROUP_MISMATCH:
      return "GLB: TEA group count does not match FTL group count";
    case ARX_GLB_TOO_MANY_VERTICES:
      return "GLB: expanded primitive vertex count exceeds uint16 max";
    case ARX_GLB_NO_GROUPS_FOR_TEA:
      return "GLB: TEA animations supplied but FTL has no bone groups";
    case ARX_GLB_BAD_FORMAT:
      return "GLB: malformed GLB container or GLTF JSON";
    case ARX_GLB_UNSUPPORTED_FEATURE:
      return "GLB: unsupported feature (sparse accessor, external buffer, etc)";
    case ARX_GLB_NON_UNIFORM_SCALE:
      return "GLB: non-uniform scale or shear in mesh node chain or inverse bind matrix";
    case ARX_GLB_MULTIPLE_SKINS:
      return "GLB: skins do not merge into a single connected armature";

    // JSON
    case ARX_JSON_BAD_FORMAT:
      return "JSON: malformed JSON";
    case ARX_JSON_BAD_SCHEMA:
      return "JSON: missing or wrong-type field";
    case ARX_JSON_LIMIT_EXCEEDED:
      return "JSON: target format limit exceeded";

    case ARX_RETURN_CODE_MAX:
      break;
  }
  return "unknown error code";
}

// NOLINTEND(readability-identifier-naming)
