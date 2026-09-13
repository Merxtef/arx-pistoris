// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/runtime/types.h"

#include "level/data.h"
#include "level/native/fts/quad_packing.h"
#include "level/native/fts/texture_shards.h"
#include "level/native/internal.h"
#include "modules/geometry.h"
#include "modules/lights.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "modules/scene.h"
#include "native/fts.h"
#include "utils/log.h"
#include "utils/math/finite.h"
#include "utils/math/geometry_algorithms.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace pistoris::level_native {
namespace {

constexpr std::int32_t kNativeGridSize = 160;
constexpr float kNativeCellSize = 100.0f;
constexpr float kNativeMaxCoord = 16000.0f;

// Bit intersections retain source-edge provenance through clipping
constexpr std::array<std::uint8_t, 3> kSourceCornerEdgeMasks = {0b101U, 0b011U, 0b110U};

using BakedCorner = fts_bake::Corner;
using BakedPolygon = fts_bake::Polygon;
using BakedTriangle = fts_bake::Triangle;

// Convex triangle / rectangle intersection: 7 corners max
constexpr std::size_t kMaxClippedCorners = 7;

struct ClippedPolygon {
  std::array<BakedCorner, kMaxClippedCorners> corners = {};
  std::size_t count = 0;

  bool push(const BakedCorner& corner) {
    if (count == corners.size()) return false;
    corners[count++] = corner;
    return true;
  }

  std::span<const BakedCorner> view() const { return std::span(corners).first(count); }
};

struct ClipScratch {
  std::array<ClippedPolygon, 2> polygons;
};

using math::finite;

float axisValue(const ArxVector3& position, bool x_axis) { return x_axis ? position.x : position.z; }

BakedCorner interpolate(const BakedCorner& a, const BakedCorner& b, float t) {
  BakedCorner out;
  out.position = {
      a.position.x + (b.position.x - a.position.x) * t,
      a.position.y + (b.position.y - a.position.y) * t,
      a.position.z + (b.position.z - a.position.z) * t,
  };
  out.normal = {
      a.normal.x + (b.normal.x - a.normal.x) * t,
      a.normal.y + (b.normal.y - a.normal.y) * t,
      a.normal.z + (b.normal.z - a.normal.z) * t,
  };
  out.u = a.u + (b.u - a.u) * t;
  out.v = a.v + (b.v - a.v) * t;
  out.color = {
      a.color.r + (b.color.r - a.color.r) * t,
      a.color.g + (b.color.g - a.color.g) * t,
      a.color.b + (b.color.b - a.color.b) * t,
  };
  out.source_edge_mask = a.source_edge_mask & b.source_edge_mask;
  return out;
}

bool clipHalfPlane(std::span<const BakedCorner> input, bool x_axis, float boundary, bool keep_greater,
                   ClippedPolygon& output) {
  output.count = 0;
  if (input.empty()) return true;

  auto inside = [&](const BakedCorner& corner) {
    const float value = axisValue(corner.position, x_axis);
    return keep_greater ? value >= boundary : value <= boundary;
  };

  BakedCorner previous = input.back();
  bool previous_inside = inside(previous);
  for (const BakedCorner& current : input) {
    const bool current_inside = inside(current);
    if (current_inside != previous_inside) {
      const float a = axisValue(previous.position, x_axis);
      const float b = axisValue(current.position, x_axis);
      const float denom = b - a;
      const float t = std::abs(denom) <= std::numeric_limits<float>::epsilon() ? 0.0f : (boundary - a) / denom;
      BakedCorner intersection = interpolate(previous, current, std::clamp(t, 0.0f, 1.0f));
      if (x_axis)
        intersection.position.x = boundary;
      else
        intersection.position.z = boundary;
      if (!output.push(intersection)) return false;
    }
    if (current_inside && !output.push(current)) return false;
    previous = current;
    previous_inside = current_inside;
  }
  return true;
}

void removeDuplicateCorners(ClippedPolygon& polygon) {
  std::size_t retained = 0;
  for (std::size_t index = 0; index < polygon.count; ++index) {
    if (retained == 0 || polygon.corners[index].position != polygon.corners[retained - 1U].position)
      polygon.corners[retained++] = polygon.corners[index];
  }
  polygon.count = retained;
  if (polygon.count > 1U && polygon.corners.front().position == polygon.corners[polygon.count - 1U].position)
    --polygon.count;
}

bool clipToCell(std::span<const BakedCorner> triangle, std::int32_t cell_x, std::int32_t cell_z, ClipScratch& scratch,
                std::span<const BakedCorner>& clipped) {
  const float min_x = static_cast<float>(cell_x) * kNativeCellSize;
  const float max_x = min_x + kNativeCellSize;
  const float min_z = static_cast<float>(cell_z) * kNativeCellSize;
  const float max_z = min_z + kNativeCellSize;

  ClippedPolygon* input = &scratch.polygons[0];
  ClippedPolygon* output = &scratch.polygons[1];
  input->count = 0;
  for (const BakedCorner& corner : triangle)
    if (!input->push(corner)) return false;

  const std::array planes = {
      std::tuple{true, min_x, true},
      std::tuple{true, max_x, false},
      std::tuple{false, min_z, true},
      std::tuple{false, max_z, false},
  };
  for (const auto& [x_axis, boundary, keep_greater] : planes) {
    if (!clipHalfPlane(input->view(), x_axis, boundary, keep_greater, *output)) return false;
    std::swap(input, output);
  }
  removeDuplicateCorners(*input);
  clipped = input->view();
  return true;
}

std::int32_t cellCoord(float value) {
  if (value >= kNativeMaxCoord) return kNativeGridSize - 1;
  return std::clamp(static_cast<std::int32_t>(std::floor(value / kNativeCellSize)), 0, kNativeGridSize - 1);
}

fts::Poly makeFtsPoly(const BakedPolygon& source) {
  fts::Poly poly;
  poly.tex = source.texture == kNoTexture ? 0 : static_cast<std::int32_t>(source.texture + 1U);
  poly.type = source.flags & ~kFaceBitQuad;
  const bool quad = source.kind != fts_bake::PolygonKind::kTriangle;
  if (quad) poly.type |= kFaceBitQuad;
  poly.room = source.room;
  poly.transval = source.transval;
  poly.norm = source.norm;
  poly.norm2 = source.norm2;
  poly.area = math::triangleArea(source.corners[0].position, source.corners[1].position, source.corners[2].position);
  if (quad)
    poly.area += math::triangleArea(source.corners[3].position, source.corners[2].position, source.corners[1].position);
  const std::size_t count = quad ? 4U : 3U;
  for (std::size_t i = 0; i < count; ++i) {
    poly.v[i].ssx = source.corners[i].position.x;
    poly.v[i].sy = source.corners[i].position.y;
    poly.v[i].ssz = source.corners[i].position.z;
    poly.v[i].stu = source.corners[i].u;
    poly.v[i].stv = source.corners[i].v;
    const ArxVector3 fallback = i == 3U ? source.norm2 : source.norm;
    poly.nrml[i] = math::normalizeFiniteOr(source.corners[i].normal, fallback, std::numeric_limits<float>::epsilon());
  }
  return poly;
}

void setSavedVertex(fts::SavedTextureVertex& out, const ArxVector3& position) { out.pos = position; }

fts::SavePoly makePortalPoly(const Portal& portal) {
  fts::SavePoly poly;
  const std::size_t count = portal.shape == PortalShape::kQuad ? 4U : 3U;
  std::array<ArxVector3, 4> vertices{};
  if (portal.shape == PortalShape::kQuad) {
    vertices = {portal.vertices[0], portal.vertices[1], portal.vertices[3], portal.vertices[2]};
    poly.type = kFaceBitQuad;
  } else {
    vertices = portal.vertices;
  }

  poly.min = vertices[0];
  poly.max = vertices[0];
  poly.center = {};
  for (std::size_t i = 0; i < count; ++i) {
    setSavedVertex(poly.v[i], vertices[i]);
    setSavedVertex(poly.tv[i], vertices[i]);
    poly.min.x = std::min(poly.min.x, vertices[i].x);
    poly.min.y = std::min(poly.min.y, vertices[i].y);
    poly.min.z = std::min(poly.min.z, vertices[i].z);
    poly.max.x = std::max(poly.max.x, vertices[i].x);
    poly.max.y = std::max(poly.max.y, vertices[i].y);
    poly.max.z = std::max(poly.max.z, vertices[i].z);
  }
  if (portal.shape == PortalShape::kQuad) {
    for (std::size_t i = 0; i < count; ++i) {
      poly.center.x += 0.25f * vertices[i].x;
      poly.center.y += 0.25f * vertices[i].y;
      poly.center.z += 0.25f * vertices[i].z;
    }
  } else {
    poly.center.x = 0.25f * vertices[0].x + 0.375f * vertices[1].x + 0.375f * vertices[2].x;
    poly.center.y = 0.25f * vertices[0].y + 0.375f * vertices[1].y + 0.375f * vertices[2].y;
    poly.center.z = 0.25f * vertices[0].z + 0.375f * vertices[1].z + 0.375f * vertices[2].z;
  }
  float radius = 0.0f;
  for (std::size_t i = 0; i < count; ++i)
    radius = std::max(radius, static_cast<float>(math::length(vertices[i] - poly.center)));
  poly.v[0].rhw = radius;
  poly.norm = math::normalizeFiniteOr(math::cross(vertices[1] - vertices[0], vertices[2] - vertices[0]),
                                      {0.0f, -1.0f, 0.0f},
                                      std::numeric_limits<float>::epsilon());
  poly.norm2 = poly.norm;
  for (std::size_t i = 0; i < count; ++i) poly.nrml[i] = poly.norm;
  poly.area = math::triangleArea(vertices[0], vertices[1], vertices[2]);
  if (portal.shape == PortalShape::kQuad) poly.area += math::triangleArea(vertices[2], vertices[3], vertices[1]);
  return poly;
}

ArxReturnCode copyTexturePath(const std::string& path, fts::Texture& out) {
  if (path.empty() || path.size() + 2U > sizeof(out.fic)) return ARX_FTS_BAD_TEXTURE_PATH;
  std::memcpy(out.fic, path.data(), path.size());
  out.fic[path.size()] = '.';
  out.fic[path.size() + 1U] = '\0';
  return ARX_OK;
}

ArxReturnCode assignNativeTexture(const BakedPolygon& polygon, NativeTextureResources& textures,
                                  fts_bake::TextureShardAllocator& allocator, fts::Data& fts,
                                  TextureIndex& out_texture) {
  if (polygon.texture == kNoTexture) {
    out_texture = kNoTexture;
    return ARX_OK;
  }
  if (static_cast<std::size_t>(polygon.texture) >= textures.families.size()) return ARX_FTS_BAD_TEXTURE_ID;

  std::size_t shard = 0;
  if ((polygon.flags & (kFaceBitIgnore | kFaceBitHide)) == 0) {
    const std::uint32_t corners = polygon.kind == fts_bake::PolygonKind::kTriangle ? 3U : 4U;
    shard = allocator.assign(static_cast<std::size_t>(polygon.room), polygon.texture, corners);
  }

  NativeTextureFamily& family = textures.families[static_cast<std::size_t>(polygon.texture)];
  if (shard >= family.shards.size()) {
    std::size_t added_shard = 0;
    ArxReturnCode rc = addNativeTextureShard(textures, polygon.texture, added_shard);
    if (rc != ARX_OK) return rc;
    if (added_shard != shard) return ARX_FTS_BAD_TEXTURE_COUNT;

    const NativeTextureShard& added = family.shards[added_shard];
    fts::Texture texture;
    rc = copyTexturePath(added.resource_path, texture);
    if (rc != ARX_OK) return rc;
    fts.textures.emplace(added.fts_id, texture);
  }

  out_texture = static_cast<TextureIndex>(family.shards[shard].fts_id - 1);
  return ARX_OK;
}

bool hasPositiveRoomDistance(const RoomDistances& distances) {
  for (const RoomDistance& distance : distances)
    if (distance.distance > 0.0f) return true;
  return false;
}

void bakeRoomDistances(const LevelModules& level, fts::Data& fts) {
  if (!rooms::hasCompleteRoomDistances(level.rooms.distances, level.rooms.definitions.size())) return;
  const std::size_t native_room_count = level.rooms.definitions.size() + 1U;
  for (std::size_t first = 0; first < level.rooms.definitions.size(); ++first) {
    for (std::size_t second = first + 1U; second < level.rooms.definitions.size(); ++second) {
      const std::size_t source_index = rooms::roomDistancePairIndex(first, second);
      const RoomDistance& source = level.rooms.distances[source_index];
      const std::size_t native_first = first + 1U;
      const std::size_t native_second = second + 1U;
      ArxVector3 first_position{};
      ArxVector3 second_position{};
      float baked_distance = -1.0f;
      if (source.low_room_portal != kInvalidPortalIndex && source.high_room_portal != kInvalidPortalIndex) {
        first_position = rooms::portalCentroid(level.rooms.portals[source.low_room_portal]);
        second_position = rooms::portalCentroid(level.rooms.portals[source.high_room_portal]);
        baked_distance = source.distance;
      }
      fts::RoomDistData& forward = fts.room_distances[native_first * native_room_count + native_second];
      forward.distance = baked_distance;
      forward.startpos = second_position;
      forward.endpos = first_position;

      fts::RoomDistData& backward = fts.room_distances[native_second * native_room_count + native_first];
      backward.distance = baked_distance;
      backward.startpos = first_position;
      backward.endpos = second_position;
    }
  }
}

}  // namespace

void logRoomDistanceBakeWarnings(const LevelModules& level) {
  const std::size_t pair_count = rooms::roomDistancePairCount(level.rooms.definitions.size());
  if (!rooms::hasCompleteRoomDistances(level.rooms.distances, level.rooms.definitions.size())) {
    if (pair_count != 0) {
      log(ARX_LOG_WARN,
          "Level native bake: room-distance data missing or incomplete; writing default -1 "
          "distances for {} room pair(s)",
          pair_count);
    }
    return;
  }

  std::size_t missing_connections = 0;
  for (const RoomDistance& distance : level.rooms.distances) {
    if (distance.distance <= 0.0f && distance.low_room_portal == kInvalidPortalIndex &&
        distance.high_room_portal == kInvalidPortalIndex)
      ++missing_connections;
  }
  if (missing_connections != 0) {
    log(ARX_LOG_WARN,
        "Level native bake: {} room-distance connection(s) missing; writing -1 with zero endpoints",
        missing_connections);
  }

  if (level.rooms.definitions.size() >= 4U && !hasPositiveRoomDistance(level.rooms.distances)) {
    log(ARX_LOG_WARN,
        "Level native bake: room-distance data contains no positive real-room distances; writing non-positive "
        "fallbacks");
  }
}

ArxReturnCode bakeFts(const LevelModules& level, NativeTextureResources& textures, bool reconstruct_quads,
                      fts::Data& out, std::vector<ArxColor3>& baked_colors, NativeBakeWarnings& warnings,
                      NativeBakeStatistics& statistics) {
  NativeBakeStatistics baked_statistics;
  if (level.rooms.definitions.size() > static_cast<std::size_t>(std::numeric_limits<std::int16_t>::max()))
    return ARX_FTS_BAD_ROOM_COUNT;
  if (level.rooms.portals.size() > kFtsMaxPortals) return ARX_FTS_BAD_PORTAL_COUNT;
  if (level.textures.textures.size() > kFtsMaxTextures) return ARX_FTS_BAD_TEXTURE_COUNT;
  if (textures.families.size() != level.textures.textures.size()) return ARX_FTS_BAD_TEXTURE_COUNT;
  const std::size_t native_room_count = level.rooms.definitions.size() + 1U;

  fts::Data fts;
  fts.header.version = kFtsVersion;
  fts.scene.version = kFtsVersion;
  fts.scene.sizex = kNativeGridSize;
  fts.scene.sizez = kNativeGridSize;
  fts.scene.Mscenepos = {};
  fts.scene.playerpos = level.scene.player_spawn.value_or(PlayerSpawn{}).position;
  fts.scene.num_rooms = static_cast<std::int32_t>(level.rooms.definitions.size());
  fts.cells.resize(static_cast<std::size_t>(kNativeGridSize) * static_cast<std::size_t>(kNativeGridSize));
  std::vector<std::vector<BakedTriangle>> cell_triangles(fts.cells.size());

  for (const NativeTextureFamily& family : textures.families) {
    if (family.shards.size() != 1U) return ARX_FTS_BAD_TEXTURE_COUNT;
    const NativeTextureShard& base = family.shards.front();
    fts::Texture texture;
    ArxReturnCode rc = copyTexturePath(base.resource_path, texture);
    if (rc != ARX_OK) return rc;
    fts.textures.emplace(base.fts_id, texture);
  }
  fts_bake::TextureShardAllocator texture_shards(native_room_count, textures.families.size());

  fts.rooms.resize(native_room_count);
  fts.room_distances.resize(native_room_count * native_room_count);
  for (fts::RoomDistData& distance : fts.room_distances) distance.distance = -1.0f;
  bakeRoomDistances(level, fts);

  if (level.navigation.anchors.size() > kFtsMaxAnchors) return ARX_FTS_BAD_ANCHOR_COUNT;
  fts.anchors.reserve(level.navigation.anchors.size());
  for (std::size_t anchor_index = 0; anchor_index < level.navigation.anchors.size(); ++anchor_index) {
    const Anchor& source = level.navigation.anchors[anchor_index];
    if (source.position.x < 0.0f || source.position.x > kNativeMaxCoord || source.position.z < 0.0f ||
        source.position.z > kNativeMaxCoord)
      return ARX_FTS_BAD_ANCHOR_POSITION;

    fts::Anchor anchor;
    anchor.data.pos = source.position;
    anchor.data.radius = source.radius;
    anchor.data.height = source.height;
    anchor.data.flags = source.flags;
    fts.anchors.push_back(anchor);

    const std::size_t cell_idx =
        static_cast<std::size_t>(cellCoord(source.position.z)) * static_cast<std::size_t>(kNativeGridSize) +
        static_cast<std::size_t>(cellCoord(source.position.x));
    fts.cells[cell_idx].anchor_ids.push_back(static_cast<std::int32_t>(anchor_index));
  }

  for (const AnchorConnection& connection : level.navigation.connections) {
    if (connection.first >= level.navigation.anchors.size() || connection.second >= level.navigation.anchors.size())
      return ARX_FTS_BAD_ANCHOR_INDEX;
    fts.anchors[connection.first].linked.push_back(static_cast<std::int32_t>(connection.second));
    fts.anchors[connection.second].linked.push_back(static_cast<std::int32_t>(connection.first));
  }

  for (std::size_t portal_index = 0; portal_index < level.rooms.portals.size(); ++portal_index) {
    const Portal& source = level.rooms.portals[portal_index];
    fts::Portal portal;
    portal.poly = makePortalPoly(source);
    portal.room_1 = static_cast<std::int32_t>(source.room_1) + 1;
    portal.room_2 = static_cast<std::int32_t>(source.room_2) + 1;
    portal.useportal = 1;
    if (portal.room_1 <= 0 || portal.room_2 <= 0 || static_cast<std::size_t>(portal.room_1) >= native_room_count ||
        static_cast<std::size_t>(portal.room_2) >= native_room_count)
      return ARX_FTS_BAD_PORTAL_ROOM_INDEX;
    fts.portals.push_back(portal);
    fts.rooms[static_cast<std::size_t>(portal.room_1)].portal_ids.push_back(static_cast<std::int32_t>(portal_index));
    fts.rooms[static_cast<std::size_t>(portal.room_2)].portal_ids.push_back(static_cast<std::int32_t>(portal_index));
  }

  ClipScratch clip_scratch;
  for (std::size_t face_index = 0; face_index < level.geometry.faces.size(); ++face_index) {
    const Face& face = level.geometry.faces[face_index];
    std::array<BakedCorner, 3> triangle = {};
    float min_x = std::numeric_limits<float>::max();
    float max_x = std::numeric_limits<float>::lowest();
    float min_z = std::numeric_limits<float>::max();
    float max_z = std::numeric_limits<float>::lowest();
    for (std::size_t corner_index = 0; corner_index < face.corners.size(); ++corner_index) {
      const pistoris::Corner& source_corner = face.corners[corner_index];
      const ArxVector3 position = level.geometry.vertices[source_corner.vertex].position;
      const ArxColor3 color = lights::cornerColorOr(
          level.lighting, static_cast<FaceIndex>(face_index), corner_index, lights::kDefaultCornerColor);
      if (position.x < 0.0f || position.x > kNativeMaxCoord || position.z < 0.0f || position.z > kNativeMaxCoord)
        return ARX_FTS_BAD_POLYGON_POSITION;
      min_x = std::min(min_x, position.x);
      max_x = std::max(max_x, position.x);
      min_z = std::min(min_z, position.z);
      max_z = std::max(max_z, position.z);
      triangle[corner_index] = {position,
                                source_corner.normal,
                                source_corner.u,
                                source_corner.v,
                                color,
                                kSourceCornerEdgeMasks[corner_index]};
    }

    const ArxVector3 face_normal = math::normalizeFiniteOr(
        math::cross(triangle[1].position - triangle[0].position, triangle[2].position - triangle[0].position),
        {0.0f, -1.0f, 0.0f},
        std::numeric_limits<float>::epsilon());
    const std::int16_t native_room = static_cast<std::int16_t>(level.rooms.face_rooms[face_index] + 1U);
    const std::int32_t min_cell_x = cellCoord(min_x);
    const std::int32_t max_cell_x = cellCoord(max_x);
    const std::int32_t min_cell_z = cellCoord(min_z);
    const std::int32_t max_cell_z = cellCoord(max_z);
    bool emitted_face = false;
    for (std::int32_t cell_z = min_cell_z; cell_z <= max_cell_z; ++cell_z) {
      for (std::int32_t cell_x = min_cell_x; cell_x <= max_cell_x; ++cell_x) {
        std::span<const BakedCorner> clipped;
        if (!clipToCell(triangle, cell_x, cell_z, clip_scratch, clipped)) return ARX_INTERNAL_ERROR;
        if (clipped.size() < 3) continue;
        const std::size_t cell_idx = static_cast<std::size_t>(cell_z) * static_cast<std::size_t>(kNativeGridSize) +
                                     static_cast<std::size_t>(cell_x);
        for (std::size_t i = 1; i + 1 < clipped.size(); ++i) {
          std::array<BakedCorner, 3> corners = {clipped[0], clipped[i], clipped[i + 1]};
          const float area = math::triangleArea(corners[0].position, corners[1].position, corners[2].position);
          if (!finite(area) ||
              geometry::degenerateTriangle(corners[0].position, corners[1].position, corners[2].position)) {
            ++warnings.discarded_fragments;
            continue;
          }
          BakedTriangle baked;
          baked.corners = corners;
          baked.source_vertices = {face.corners[0].vertex, face.corners[1].vertex, face.corners[2].vertex};
          baked.face_normal = face_normal;
          baked.texture = face.texture;
          baked.flags = face.flags;
          baked.transval = face.transval;
          baked.room = native_room;
          baked.source_face = face_index;
          cell_triangles[cell_idx].push_back(baked);
          emitted_face = true;
        }
      }
    }
    if (!emitted_face) ++warnings.fully_discarded_faces;
  }

  std::size_t fragment_count = 0;
  std::size_t max_cell_triangles = 0;
  for (const std::vector<BakedTriangle>& triangles : cell_triangles) {
    fragment_count += triangles.size();
    max_cell_triangles = std::max(max_cell_triangles, triangles.size());
  }
  fts_bake::CellPackingScratch packing_scratch;
  fts_bake::reserveCellPackingScratch(packing_scratch, max_cell_triangles);
  std::vector<ArxColor3> colors;
  colors.reserve(std::min(fragment_count, kFtsMaxPolygons) * 4U);

  std::size_t polygon_count = 0;
  for (std::size_t cell_idx = 0; cell_idx < fts.cells.size(); ++cell_idx) {
    const std::span<const BakedPolygon> polygons =
        fts_bake::packCellPolygons(cell_triangles[cell_idx], reconstruct_quads, packing_scratch);
    fts.cells[cell_idx].polygons.reserve(std::min(polygons.size(), kFtsMaxCellPolygons));
    const std::int16_t cell_x = static_cast<std::int16_t>(cell_idx % static_cast<std::size_t>(kNativeGridSize));
    const std::int16_t cell_z = static_cast<std::int16_t>(cell_idx / static_cast<std::size_t>(kNativeGridSize));
    for (const BakedPolygon& source : polygons) {
      if (fts.cells[cell_idx].polygons.size() >= kFtsMaxCellPolygons) return ARX_FTS_BAD_CELL_POLYGON_COUNT;
      if (polygon_count >= kFtsMaxPolygons) return ARX_FTS_BAD_POLYGON_COUNT;
      fts::Room& room = fts.rooms[static_cast<std::size_t>(source.room)];
      if (room.polygons.size() >= kFtsMaxPolygons) return ARX_FTS_BAD_ROOM_POLYGON_COUNT;

      const std::int16_t local_idx = static_cast<std::int16_t>(fts.cells[cell_idx].polygons.size());
      BakedPolygon emitted = source;
      ArxReturnCode rc = assignNativeTexture(source, textures, texture_shards, fts, emitted.texture);
      if (rc != ARX_OK) return rc;
      fts.cells[cell_idx].polygons.push_back(makeFtsPoly(emitted));
      const std::size_t corner_count = source.kind == fts_bake::PolygonKind::kTriangle ? 3U : 4U;
      for (std::size_t corner = 0; corner < corner_count; ++corner) colors.push_back(source.corners[corner].color);
      room.polygons.push_back({cell_x, cell_z, local_idx, 0});
      switch (source.kind) {
        case fts_bake::PolygonKind::kTriangle:
          ++baked_statistics.triangles;
          break;
        case fts_bake::PolygonKind::kClippedFragmentQuad:
          ++baked_statistics.clipped_fragment_quads;
          break;
        case fts_bake::PolygonKind::kCrossFaceQuad:
          ++baked_statistics.cross_face_quads;
          break;
      }
      ++polygon_count;
    }
  }

  fts.scene.num_textures = static_cast<std::int32_t>(fts.textures.size());
  fts.scene.num_polys = static_cast<std::int32_t>(polygon_count);
  fts.scene.num_anchors = static_cast<std::int32_t>(fts.anchors.size());
  fts.scene.num_portals = static_cast<std::int32_t>(fts.portals.size());
  for (fts::Room& room : fts.rooms) {
    room.data.num_portals = static_cast<std::int32_t>(room.portal_ids.size());
    room.data.num_polys = static_cast<std::int32_t>(room.polygons.size());
  }
  ArxReturnCode rc = validateFts(&fts);
  if (rc != ARX_OK) return rc;
  baked_colors = std::move(colors);
  out = std::move(fts);
  statistics = baked_statistics;
  return ARX_OK;
}

}  // namespace pistoris::level_native
