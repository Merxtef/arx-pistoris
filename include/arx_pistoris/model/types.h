// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_MODEL_TYPES_H
#define ARX_PISTORIS_MODEL_TYPES_H

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/string_view.h"

#include <stddef.h>
#include <stdint.h>

// Public C-compatible Model value types
// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

typedef struct ArxTextureView ArxTextureView;

#ifdef __cplusplus
#define ARX_PISTORIS_DETAIL_CXX_DEFAULT(value) = value
#else
#define ARX_PISTORIS_DETAIL_CXX_DEFAULT(value)
#endif

enum { ARX_MODEL_FACE_BITS_ALL = ARX_FACE_BITS_ALL & ~ARX_FACE_BIT_QUAD };

typedef struct ArxModelVertex {
  ArxVector3 position;
  ArxBoneIndex bone ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_INVALID_INDEX);
} ArxModelVertex;

typedef struct ArxModelCorner {
  ArxVertexIndex vertex;
  ArxVector3 normal;
  float u;
  float v;
} ArxModelCorner;

typedef struct ArxModelFace {
  ArxModelCorner corners[3];
  ArxVector3 normal;
  ArxTextureIndex texture ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_NO_TEXTURE);
  ArxFaceType flags;
  float transval;
} ArxModelFace;

typedef struct ArxModelBone {
  ArxStringView name;
  ArxVector3 position;
  ArxBoneIndex parent ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_INVALID_INDEX);
  float blob_shadow_size;
} ArxModelBone;

typedef struct ArxModelActionPoint {
  ArxStringView name;
  ArxVector3 position;
  ArxBoneIndex bone ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_INVALID_INDEX);
} ArxModelActionPoint;

typedef struct ArxModelOrigin {
  ArxBoneIndex bone ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_INVALID_INDEX);
} ArxModelOrigin;

typedef struct ArxModelSelection {
  ArxStringView name;
  uint8_t has_leading_vertex;
  ArxVector3 leading_position;
  ArxBoneIndex leading_bone ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_INVALID_INDEX);
} ArxModelSelection;

typedef struct ArxModelSelectionMembersInput {
  const ArxVertexIndex* vertices ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t vertex_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  const ArxBoneIndex* bones ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t bone_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  const ArxActionPointIndex* action_points ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t action_point_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
} ArxModelSelectionMembersInput;

typedef struct ArxModelMeshInput {
  const ArxModelVertex* vertices;
  size_t vertex_count;
  const ArxModelFace* faces;
  size_t face_count;
  const ArxTextureView* textures;
  size_t texture_count;
} ArxModelMeshInput;

typedef struct ArxModelSkeletonInput {
  const ArxModelBone* bones;
  size_t bone_count;
  ArxModelOrigin origin;
} ArxModelSkeletonInput;

typedef struct ArxModelActionPointsInput {
  const ArxModelActionPoint* action_points;
  size_t action_point_count;
} ArxModelActionPointsInput;

#undef ARX_PISTORIS_DETAIL_CXX_DEFAULT

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_MODEL_TYPES_H */
