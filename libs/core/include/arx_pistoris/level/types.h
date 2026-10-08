// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_LEVEL_TYPES_H
#define ARX_PISTORIS_LEVEL_TYPES_H

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/string_view.h"

#include <stddef.h>
#include <stdint.h>

// Public C-compatible Level value types
// NOLINTBEGIN(readability-identifier-naming, performance-enum-size)

typedef struct ArxTextureView ArxTextureView;

#ifdef __cplusplus
#define ARX_PISTORIS_DETAIL_CXX_DEFAULT(value) = value
#else
#define ARX_PISTORIS_DETAIL_CXX_DEFAULT(value)
#endif

typedef uint32_t ArxPortalShape;
enum { ARX_PORTAL_TRIANGLE = 3, ARX_PORTAL_QUAD = 4 };

typedef uint32_t ArxZoneHeightMode;
enum { ARX_ZONE_HEIGHT_FINITE = 0, ARX_ZONE_HEIGHT_INFINITE = 1 };

typedef uint32_t ArxPathNodeType;
enum { ARX_PATH_NODE_STANDARD = 0, ARX_PATH_NODE_BEZIER = 1 };

enum { ARX_ANCHOR_FLAG_BLOCKED = 1U << 3, ARX_LEVEL_FACE_BITS_ALL = ARX_FACE_BITS_ALL & ~ARX_FACE_BIT_QUAD };

typedef struct ArxLevelVertex {
  ArxVector3 position;
} ArxLevelVertex;

typedef struct ArxLevelCorner {
  ArxVertexIndex vertex;
  ArxVector3 normal;
  float u;
  float v;
  ArxColor3 color ARX_PISTORIS_DETAIL_CXX_DEFAULT((ArxColor3{0.5f, 0.5f, 0.5f}));
} ArxLevelCorner;

typedef struct ArxLevelFace {
  ArxLevelCorner corners[3];
  ArxTextureIndex texture ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_NO_TEXTURE);
  ArxRoomIndex room ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_NO_ROOM);
  ArxFaceType flags;
  float transval;
  ArxVector3 normal;
} ArxLevelFace;

typedef struct ArxLevelRoomDistance {
  ArxRoomIndex room_a;
  ArxRoomIndex room_b;
  float distance;
  ArxPortalIndex portal_a ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_INVALID_INDEX);
  ArxPortalIndex portal_b ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_INVALID_INDEX);
} ArxLevelRoomDistance;

typedef struct ArxLevelAnchorConnection {
  ArxAnchorIndex first;
  ArxAnchorIndex second;
} ArxLevelAnchorConnection;

typedef struct ArxLevelNavSurfaceTriangle {
  ArxNavSurfaceVertexIndex vertices[3];
} ArxLevelNavSurfaceTriangle;

typedef struct ArxLevelPlayerSpawn {
  ArxVector3 position;
  ArxQuat rotation;
  uint8_t is_usable;
} ArxLevelPlayerSpawn;

typedef struct ArxLevelModelPreviewReport {
  size_t mapped_models ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  size_t previewed_entities ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  size_t skipped_anonymous_models ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  size_t skipped_unmappable_models ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  size_t skipped_duplicate_models ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
  size_t skipped_invalid_models ARX_PISTORIS_DETAIL_CXX_DEFAULT(0);
} ArxLevelModelPreviewReport;

typedef struct ArxLevelGlbImportInfo {
  ArxVector3 applied_arx_offset ARX_PISTORIS_DETAIL_CXX_DEFAULT({});
} ArxLevelGlbImportInfo;

typedef struct ArxLevelRoom {
  ArxStringView name;
} ArxLevelRoom;

typedef struct ArxLevelPortal {
  ArxStringView name;
  ArxRoomIndex room_front;
  ArxRoomIndex room_back;
  ArxPortalShape shape ARX_PISTORIS_DETAIL_CXX_DEFAULT(ARX_PORTAL_QUAD);
  /* The fourth vertex is ignored when shape is ARX_PORTAL_TRIANGLE */
  ArxVector3 vertices[4];
} ArxLevelPortal;

typedef struct ArxLevelAnchor {
  ArxVector3 position;
  float radius ARX_PISTORIS_DETAIL_CXX_DEFAULT(50.0f);
  float height ARX_PISTORIS_DETAIL_CXX_DEFAULT(-165.0f);
  int16_t flags;
  ArxStringView name;
} ArxLevelAnchor;

typedef struct ArxLevelNavSurfaceInfo {
  uint8_t has_surface;
  size_t vertex_count;
  size_t triangle_count;
} ArxLevelNavSurfaceInfo;

typedef struct ArxLevelNavSurfaceInput {
  /* Scalar counts: positions is xyz triples; triangle_indices is triples of vertex indices. */
  const float* positions;
  size_t position_count;
  const uint32_t* triangle_indices;
  size_t triangle_index_count;
} ArxLevelNavSurfaceInput;

typedef struct ArxLevelLight {
  ArxStringView name;
  ArxVector3 position;
  ArxColor3 color;
  float fallstart;
  float fallend ARX_PISTORIS_DETAIL_CXX_DEFAULT(1.0f);
  float intensity;
  ArxColor3 flicker;
  float effect_radius;
  float effect_frequency;
  float effect_size;
  float effect_speed;
  float flare_size;
  uint32_t flags;
} ArxLevelLight;

typedef struct ArxLevelEntity {
  ArxStringView class_path;
  int32_t ident ARX_PISTORIS_DETAIL_CXX_DEFAULT(-1);
  ArxVector3 position;
  ArxQuat rotation;
  ArxStringView name;
} ArxLevelEntity;

typedef struct ArxLevelFog {
  ArxVector3 position;
  ArxColor3 color;
  float size;
  uint8_t directional;
  float scale;
  ArxQuat rotation;
  float speed;
  float rotate_speed;
  int32_t lifetime_ms;
  float frequency;
  ArxStringView name;
} ArxLevelFog;

typedef struct ArxLevelZoneAmbiance {
  ArxStringView name;
  float volume ARX_PISTORIS_DETAIL_CXX_DEFAULT(100.0f);
} ArxLevelZoneAmbiance;

typedef struct ArxLevelZone {
  ArxStringView name;
  size_t perimeter_count;
  float reference_y;
  ArxZoneHeightMode height_mode;
  float height;
  uint8_t has_color;
  ArxColor3 color;
  uint8_t has_farclip;
  float farclip;
  uint8_t has_ambiance;
  ArxLevelZoneAmbiance ambiance;
} ArxLevelZone;

typedef struct ArxLevelZoneInput {
  ArxLevelZone value;
  const ArxVector2* perimeter_xz;
} ArxLevelZoneInput;

typedef struct ArxLevelPathNode {
  ArxVector3 relative_position;
  ArxPathNodeType type;
  uint32_t time_ms;
} ArxLevelPathNode;

typedef struct ArxLevelPath {
  ArxStringView name;
  ArxVector3 position;
  size_t node_count;
} ArxLevelPath;

typedef struct ArxLevelPathInput {
  ArxStringView name;
  ArxVector3 position;
  const ArxLevelPathNode* nodes;
  size_t node_count;
} ArxLevelPathInput;

typedef struct ArxLevelFacesInput {
  /* Scalar layouts: indices 3F, UVs 6F, corner_normals 9F, textures F, transvals F,
     corner_colors 9F or one RGB
   * triple, face_normals 3F or omitted, flags F or omitted. */
  const uint32_t* vertex_indices;
  size_t vertex_index_count;
  const float* uvs;
  size_t uv_count;
  const float* corner_normals;
  size_t corner_normal_count;
  const uint32_t* textures;
  size_t texture_count;
  const float* transvals;
  size_t transval_count;
  const float* corner_colors;
  size_t corner_color_count;
  const float* face_normals;
  size_t face_normal_count;
  const ArxFaceType* flags;
  size_t flag_count;
} ArxLevelFacesInput;

typedef struct ArxLevelAnchorsInput {
  /* Scalar counts: positions is xyz triples; radii, heights, and flags each contain A values. */
  const float* positions;
  size_t position_count;
  const float* radii;
  size_t radius_count;
  const float* heights;
  size_t height_count;
  const uint32_t* flags;
  size_t flag_count;
} ArxLevelAnchorsInput;

typedef struct ArxLevelFacesOutput {
  /* Null with zero count omits a field; nonnull with zero count requests an empty field. */
  uint32_t* vertex_indices;
  size_t vertex_index_count;
  float* uvs;
  size_t uv_count;
  float* corner_normals;
  size_t corner_normal_count;
  ArxTextureIndex* textures;
  size_t texture_count;
  float* transvals;
  size_t transval_count;
  float* corner_colors;
  size_t corner_color_count;
  float* face_normals;
  size_t face_normal_count;
  ArxFaceType* flags;
  size_t flag_count;
} ArxLevelFacesOutput;

typedef struct ArxLevelAnchorsOutput {
  /* Null with zero count omits a field; nonnull with zero count requests an empty field. */
  float* positions;
  size_t position_count;
  float* radii;
  size_t radius_count;
  float* heights;
  size_t height_count;
  uint32_t* flags;
  size_t flag_count;
} ArxLevelAnchorsOutput;

typedef struct ArxLevelNavSurfaceOutput {
  /* Null with zero count omits a field; nonnull with zero count requests an empty field. */
  float* positions;
  size_t position_count;
  ArxNavSurfaceVertexIndex* triangle_indices;
  size_t triangle_index_count;
} ArxLevelNavSurfaceOutput;

typedef struct ArxLevelRoomDistancesOutput {
  /* Null with zero count omits a field; nonnull with zero count requests an empty field. */
  float* distances;
  size_t distance_count;
  ArxPortalIndex* endpoint_portals;
  size_t endpoint_portal_count;
} ArxLevelRoomDistancesOutput;

#undef ARX_PISTORIS_DETAIL_CXX_DEFAULT

// NOLINTEND(readability-identifier-naming, performance-enum-size)

#endif /* ARX_PISTORIS_LEVEL_TYPES_H */
