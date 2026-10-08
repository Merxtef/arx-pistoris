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

// Counts are scalar element counts. Empty face_normals derive normals; empty flags mean zero.
typedef struct ArxModelFacesInput {
  const uint32_t* vertex_indices ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t vertex_index_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  const float* uvs ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t uv_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  const float* corner_normals ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t corner_normal_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  const ArxTextureIndex* textures ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t texture_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  const float* transvals ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t transval_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  const float* face_normals ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t face_normal_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  const ArxFaceType* flags ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t flag_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
} ArxModelFacesInput;

// Null pointer and zero count omit a destination; non-null pointer and zero count request an empty destination.
typedef struct ArxModelFacesOutput {
  uint32_t* vertex_indices ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t vertex_index_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  float* uvs ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t uv_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  float* corner_normals ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t corner_normal_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  ArxTextureIndex* textures ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t texture_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  float* transvals ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t transval_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  float* face_normals ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t face_normal_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  ArxFaceType* flags ARX_PISTORIS_DETAIL_CXX_DEFAULT(nullptr);
  size_t flag_count ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
} ArxModelFacesOutput;

#undef ARX_PISTORIS_DETAIL_CXX_DEFAULT

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_MODEL_TYPES_H */
