// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/runtime/types.h"

#include "level/anchor_bounds.h"
#include "level/data.h"
#include "level/native/internal.h"
#include "level/validation.h"
#include "modules/geometry.h"
#include "modules/navigation.h"
#include "modules/rooms.h"
#include "modules/textures.h"
#include "utils/log.h"
#include "utils/math/bounds.h"
#include "utils/math/finite.h"
#include "utils/native_text.h"
#include "utils/resource_path.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris::level_native {
namespace {

constexpr float kNormalRepairEpsilon = 1.0e-4f;
constexpr float kRoomDistanceEndpointEpsilon = 1.0f;

struct PortalMatch {
  PortalIndex portal = kInvalidPortalIndex;
  double distance_squared = std::numeric_limits<double>::infinity();
};

struct PositiveRoomDistance {
  float distance = -1.0f;
  PortalIndex low_room_portal = kInvalidPortalIndex;
  PortalIndex high_room_portal = kInvalidPortalIndex;
};

PortalMatch closestRoomPortal(const RoomsData& rooms, RoomIndex room, const ArxVector3& point) {
  PortalMatch match;
  if (!math::finite(point)) return match;
  for (PortalIndex portal = 0; portal < rooms.portals.size(); ++portal) {
    const Portal& candidate = rooms.portals[portal];
    if (candidate.room_1 != room && candidate.room_2 != room) continue;
    const double distance_squared = rooms::pointPortalDistanceSquared(point, candidate);
    if (distance_squared < match.distance_squared) match = {portal, distance_squared};
  }
  return match;
}

std::optional<PositiveRoomDistance> positiveRoomDistance(const RoomsData& rooms, RoomIndex low_room,
                                                         RoomIndex high_room, const fts::RoomDistData& source,
                                                         const ArxVector3& low_point, const ArxVector3& high_point,
                                                         NativeBuildWarnings& warnings) {
  if (source.distance <= 0.0f) return std::nullopt;
  const PortalMatch low_match = closestRoomPortal(rooms, low_room, low_point);
  const PortalMatch high_match = closestRoomPortal(rooms, high_room, high_point);
  if (low_match.portal == kInvalidPortalIndex || high_match.portal == kInvalidPortalIndex) {
    ++warnings.room_distance_discarded_positive_directions;
    return std::nullopt;
  }
  constexpr double kEndpointEpsilonSquared =
      static_cast<double>(kRoomDistanceEndpointEpsilon) * static_cast<double>(kRoomDistanceEndpointEpsilon);
  if (low_match.distance_squared > kEndpointEpsilonSquared) ++warnings.room_distance_reassigned_endpoints;
  if (high_match.distance_squared > kEndpointEpsilonSquared) ++warnings.room_distance_reassigned_endpoints;
  return PositiveRoomDistance{source.distance, low_match.portal, high_match.portal};
}

RoomDistance directRoomDistance(const RoomsData& rooms, RoomIndex low_room, RoomIndex high_room,
                                const fts::RoomDistData& forward, const fts::RoomDistData& backward,
                                NativeBuildWarnings& warnings) {
  if (forward.distance > 0.0f && backward.distance > 0.0f) {
    ++warnings.room_distance_missing_connections;
    return {};
  }
  std::optional<PortalIndex> first_direct = rooms::firstDirectPortal(rooms, low_room, high_room);
  if (!first_direct) {
    ++warnings.room_distance_missing_connections;
    return {};
  }

  PortalIndex best_portal = *first_direct;
  double best_score = std::numeric_limits<double>::infinity();
  bool best_represented = false;
  constexpr double kEndpointEpsilonSquared =
      static_cast<double>(kRoomDistanceEndpointEpsilon) * static_cast<double>(kRoomDistanceEndpointEpsilon);
  auto consider = [&](const fts::RoomDistData& source, const ArxVector3& low_point, const ArxVector3& high_point) {
    if (source.distance > 0.0f || !math::finite(low_point) || !math::finite(high_point)) return;
    for (PortalIndex portal = 0; portal < rooms.portals.size(); ++portal) {
      const Portal& candidate = rooms.portals[portal];
      if (!rooms::connectsRooms(candidate, low_room, high_room)) continue;
      const double low_room_endpoint_distance_squared = rooms::pointPortalDistanceSquared(low_point, candidate);
      const double high_room_endpoint_distance_squared = rooms::pointPortalDistanceSquared(high_point, candidate);
      const double score = low_room_endpoint_distance_squared + high_room_endpoint_distance_squared;
      const bool represented = low_room_endpoint_distance_squared <= kEndpointEpsilonSquared &&
                               high_room_endpoint_distance_squared <= kEndpointEpsilonSquared;
      if ((represented && !best_represented) || (represented == best_represented && score < best_score)) {
        best_score = score;
        best_portal = portal;
        best_represented = represented;
      }
    }
  };
  consider(forward, forward.endpos, forward.startpos);
  consider(backward, backward.startpos, backward.endpos);
  if (!best_represented) ++warnings.room_distance_direct_portal_fallbacks;

  float distance = forward.distance <= 0.0f ? forward.distance : backward.distance;
  if (distance > 0.0f) distance = -1.0f;
  return {.distance = distance, .low_room_portal = best_portal, .high_room_portal = best_portal};
}

using TexturePathMap =
    std::unordered_map<std::string, TextureIndex, ResourcePathIdentityHash, ResourcePathIdentityEqual>;

TextureIndex addTexture(LevelModules& out, std::map<std::int32_t, TextureIndex>& by_id, TexturePathMap& by_path,
                        std::int32_t texture_id, const std::string& texture,
                        std::vector<std::string>* texture_source_paths) {
  if (texture_id <= 0) return kNoTexture;
  if (auto existing = by_id.find(texture_id); existing != by_id.end()) return existing->second;
  if (texture.empty()) {
    by_id.emplace(texture_id, kNoTexture);
    return kNoTexture;
  }
  Texture parsed(texture);
  if (parsed.path.empty()) {
    by_id.emplace(texture_id, kNoTexture);
    return kNoTexture;
  }
  if (auto existing = by_path.find(std::string_view(parsed.path)); existing != by_path.end()) {
    by_id.emplace(texture_id, existing->second);
    return existing->second;
  }
  TextureIndex index = static_cast<TextureIndex>(out.textures.textures.size());
  out.textures.textures.push_back(std::move(parsed));
  by_id.emplace(texture_id, index);
  by_path.emplace(out.textures.textures.back().path, index);
  if (texture_source_paths) texture_source_paths->push_back(texture);
  return index;
}

geometry::Error addTriangle(LevelModules& out, const fts::Poly& poly, int a, int b, int c,
                            std::array<VertexIndex, 4>& polygon_vertices, TextureIndex texture, FaceType flags,
                            std::uint32_t room, const ArxColor3* source_colors, NativeBuildWarnings& warnings) {
  std::array<ArxVector3, 3> positions = {
      ArxVector3{poly.v[a].ssx, poly.v[a].sy, poly.v[a].ssz},
      ArxVector3{poly.v[b].ssx, poly.v[b].sy, poly.v[b].ssz},
      ArxVector3{poly.v[c].ssx, poly.v[c].sy, poly.v[c].ssz},
  };
  ArxVector3 raw_normal = math::cross(positions[1] - positions[0], positions[2] - positions[0]);
  float face_length = static_cast<float>(math::length(raw_normal));
  if (!std::isfinite(face_length) || geometry::degenerateTriangle(positions[0], positions[1], positions[2])) {
    ++warnings.discarded_faces;
    return geometry::Error::kNone;
  }
  ArxVector3 face_normal = math::normalizeFiniteOr(raw_normal, {});
  std::array source_indices = {a, b, c};
  std::array<VertexIndex, 3> vertex_indices{};
  for (std::size_t i = 0; i < source_indices.size(); ++i) {
    VertexIndex& index = polygon_vertices[static_cast<std::size_t>(source_indices[i])];
    if (index == kInvalidVertexIndex) {
      if (out.geometry.vertices.size() >= static_cast<std::size_t>(kInvalidVertexIndex))
        return geometry::Error::kTooManyVertices;
      const Vertex vertex{positions[i]};
      const geometry::Error error = geometry::validateVertex(vertex);
      if (error != geometry::Error::kNone) return error;
      index = geometry::addVertex(out.geometry, vertex);
    }
    vertex_indices[i] = index;
  }

  Face face{};
  for (std::size_t i = 0; i < face.corners.size(); ++i) {
    int source_index = source_indices[i];
    ArxVector3 normal = poly.nrml[source_index];
    float normal_length = static_cast<float>(math::length(normal));
    if (!std::isfinite(normal_length) || normal_length <= kNormalRepairEpsilon) {
      normal = face_normal;
      ++warnings.regenerated_normals;
    } else {
      if (std::abs(normal_length - 1.0f) > kNormalRepairEpsilon) ++warnings.normalized_normals;
      normal = math::normalizeFiniteOr(normal, {});
    }
    face.corners[i] = {
        vertex_indices[i],
        normal,
        poly.v[source_index].stu,
        poly.v[source_index].stv,
    };
  }
  face.texture = texture;
  face.flags = flags;
  face.transval = poly.transval;
  face.normal = face_normal;
  out.geometry.faces.push_back(face);
  out.rooms.face_rooms.push_back(room);
  if (source_colors) {
    for (int source_index : source_indices) out.lighting.corner_colors.push_back(source_colors[source_index]);
  }
  return geometry::Error::kNone;
}

void diagnoseNativeRoomDistanceMatrix(const fts::Data& fts, NativeBuildWarnings& warnings) {
  const std::size_t native_room_count = static_cast<std::size_t>(fts.scene.num_rooms) + 1U;
  if (fts.room_distances.size() != native_room_count * native_room_count) return;

  for (std::size_t room = 1; room < native_room_count; ++room) {
    if (fts.room_distances[room].distance > 0.0f) ++warnings.room_distance_room0_positive;
    if (fts.room_distances[room * native_room_count].distance > 0.0f) ++warnings.room_distance_room0_positive;
  }

  if (fts.scene.num_rooms < 4) return;
  bool has_positive_real_distance = false;
  for (std::size_t first = 1; first < native_room_count; ++first) {
    for (std::size_t second = 1; second < native_room_count; ++second) {
      if (first == second) continue;
      if (fts.room_distances[first * native_room_count + second].distance > 0.0f) has_positive_real_distance = true;
    }
  }
  warnings.room_distance_no_positive_real_pairs = !has_positive_real_distance;
}

void importRoomDistances(const fts::Data& fts, LevelModules& out, NativeBuildWarnings& warnings) {
  diagnoseNativeRoomDistanceMatrix(fts, warnings);
  rooms::resetRoomDistances(out.rooms.distances, out.rooms.definitions.size());
  const std::size_t native_room_count = static_cast<std::size_t>(fts.scene.num_rooms) + 1U;
  for (RoomIndex low_room = 0; low_room < out.rooms.definitions.size(); ++low_room) {
    for (RoomIndex high_room = low_room + 1U; high_room < out.rooms.definitions.size(); ++high_room) {
      const std::size_t compact_index = rooms::roomDistancePairIndex(low_room, high_room);
      RoomDistance& distance = out.rooms.distances[compact_index];
      const std::size_t native_low = static_cast<std::size_t>(low_room) + 1U;
      const std::size_t native_high = static_cast<std::size_t>(high_room) + 1U;
      const fts::RoomDistData& forward = fts.room_distances[native_low * native_room_count + native_high];
      const fts::RoomDistData& backward = fts.room_distances[native_high * native_room_count + native_low];

      const std::optional<PositiveRoomDistance> forward_distance =
          positiveRoomDistance(out.rooms, low_room, high_room, forward, forward.endpos, forward.startpos, warnings);
      const std::optional<PositiveRoomDistance> backward_distance =
          positiveRoomDistance(out.rooms, low_room, high_room, backward, backward.startpos, backward.endpos, warnings);
      if (!forward_distance && !backward_distance) {
        distance = directRoomDistance(out.rooms, low_room, high_room, forward, backward, warnings);
        continue;
      }
      if (forward_distance.has_value() != backward_distance.has_value()) ++warnings.room_distance_one_sided;
      if (forward_distance && backward_distance && forward_distance->distance != backward_distance->distance)
        ++warnings.room_distance_asymmetric;

      if (forward_distance && !backward_distance) {
        distance = {forward_distance->distance, forward_distance->low_room_portal, forward_distance->high_room_portal};
        continue;
      }
      if (!forward_distance && backward_distance) {
        distance = {
            backward_distance->distance, backward_distance->low_room_portal, backward_distance->high_room_portal};
        continue;
      }

      const bool portals_match = forward_distance->low_room_portal == backward_distance->low_room_portal &&
                                 forward_distance->high_room_portal == backward_distance->high_room_portal;
      if (portals_match) {
        const float average = static_cast<float>(
            (static_cast<double>(forward_distance->distance) + static_cast<double>(backward_distance->distance)) * 0.5);
        distance = {average, forward_distance->low_room_portal, forward_distance->high_room_portal};
      } else {
        ++warnings.room_distance_mismatched_portals;
        const PositiveRoomDistance& selected =
            forward_distance->distance <= backward_distance->distance ? *forward_distance : *backward_distance;
        distance = {selected.distance, selected.low_room_portal, selected.high_room_portal};
      }
    }
  }
}

}  // namespace

std::size_t expectedFtsColorCount(const fts::Data& fts) {
  std::size_t count = 0;
  for (const fts::Cell& cell : fts.cells) {
    for (const fts::Poly& poly : cell.polygons) count += (poly.type & kFaceBitQuad) != 0 ? 4U : 3U;
  }
  return count;
}

ArxReturnCode buildFtsModules(const fts::Data& fts, std::span<const ArxColor3> colors, LevelModules& out,
                              NativeBuildWarnings& warnings, std::vector<std::string>* texture_source_paths,
                              NativeTextMode text_mode) {
  std::map<std::int32_t, TextureIndex> textures;
  TexturePathMap textures_by_path;
  std::size_t color_index = 0;

  std::vector<std::int32_t> native_rooms_to_level(static_cast<std::size_t>(fts.scene.num_rooms) + 1,
                                                  std::numeric_limits<std::int32_t>::min());
  out.rooms.definitions.reserve(static_cast<std::size_t>(fts.scene.num_rooms));
  for (std::int32_t room = 1; room <= fts.scene.num_rooms; ++room) {
    native_rooms_to_level[static_cast<std::size_t>(room)] = static_cast<std::int32_t>(out.rooms.definitions.size());
    out.rooms.definitions.push_back({std::format("room_{}", room)});
  }

  std::vector<std::int32_t> texture_ids;
  texture_ids.reserve(fts.textures.size());
  for (const auto& [id, unused] : fts.textures) {
    (void)unused;
    texture_ids.push_back(id);
  }
  std::sort(texture_ids.begin(), texture_ids.end());
  textures_by_path.reserve(texture_ids.size());
  std::unordered_map<std::string, std::string, ResourcePathIdentityHash, ResourcePathIdentityEqual> decoded_sources;
  if (texture_source_paths) texture_source_paths->reserve(texture_ids.size());
  for (std::int32_t id : texture_ids) {
    const std::string& raw_path = fts.textures.at(id).fic;
    std::string decoded_path;
    if (!native_text::decode(raw_path, text_mode, decoded_path)) return ARX_FTS_BAD_TEXTURE_PATH;
    const auto [source, new_source] = decoded_sources.try_emplace(decoded_path, raw_path);
    if (!new_source && !ResourcePathIdentityEqual{}(source->second, raw_path)) {
      log(ARX_LOG_ERROR, "FTS -> Level: distinct native texture paths decode to '{}'", decoded_path);
      return ARX_FTS_BAD_TEXTURE_PATH;
    }
    addTexture(out, textures, textures_by_path, id, decoded_path, texture_source_paths);
  }
  textures::PathRepairInfo texture_repairs;
  const textures::Error texture_error = textures::repairPaths(out.textures.textures, &texture_repairs);
  if (texture_error != textures::Error::kNone) return level_validation::textureError(texture_error);
  for (const textures::PathRepairInfo::Repair& repair : texture_repairs.repairs)
    log(ARX_LOG_WARN, "FTS -> Level: texture path '{}' normalized to '{}'", repair.original, repair.repaired);
  out.rooms.portals.reserve(fts.portals.size());
  std::uint32_t portal_ordinal = 1;
  for (const fts::Portal& portal : fts.portals) {
    if (portal.room_1 <= 0 || portal.room_2 <= 0 ||
        static_cast<std::size_t>(portal.room_1) >= native_rooms_to_level.size() ||
        static_cast<std::size_t>(portal.room_2) >= native_rooms_to_level.size()) {
      ++warnings.skipped_room0_portals;
      continue;
    }
    Portal converted;
    converted.name = std::format("portal_{}", portal_ordinal++);
    converted.room_1 = static_cast<std::uint32_t>(native_rooms_to_level[static_cast<std::size_t>(portal.room_1)]);
    converted.room_2 = static_cast<std::uint32_t>(native_rooms_to_level[static_cast<std::size_t>(portal.room_2)]);
    converted.shape = (portal.poly.type & kFaceBitQuad) != 0 ? PortalShape::kQuad : PortalShape::kTriangle;
    if (converted.shape == PortalShape::kQuad) {
      converted.vertices = {portal.poly.v[0].pos, portal.poly.v[1].pos, portal.poly.v[3].pos, portal.poly.v[2].pos};
    } else {
      for (std::size_t i = 0; i < 3; ++i) converted.vertices[i] = portal.poly.v[i].pos;
    }
    out.rooms.portals.push_back(std::move(converted));
  }

  for (std::int32_t z = 0; z < fts.scene.sizez; ++z) {
    for (std::int32_t x = 0; x < fts.scene.sizex; ++x) {
      std::uint32_t cell_index = static_cast<std::uint32_t>(z * fts.scene.sizex + x);
      const auto& cell = fts.cells[cell_index];
      for (const fts::Poly& poly : cell.polygons) {
        std::uint32_t corners = (poly.type & kFaceBitQuad) != 0 ? 4U : 3U;
        if (poly.room <= 0 || static_cast<std::size_t>(poly.room) >= native_rooms_to_level.size()) {
          ++warnings.skipped_room0_faces;
          color_index += corners;
          continue;
        }
        TextureIndex texture = kNoTexture;
        if (poly.tex > 0) texture = textures.at(poly.tex);
        FaceType flags = static_cast<FaceType>(poly.type & ~kFaceBitQuad);
        const ArxColor3* source = colors.empty() ? nullptr : colors.data() + color_index;
        std::uint32_t room = static_cast<std::uint32_t>(native_rooms_to_level[static_cast<std::size_t>(poly.room)]);
        std::array<VertexIndex, 4> polygon_vertices = {
            kInvalidVertexIndex, kInvalidVertexIndex, kInvalidVertexIndex, kInvalidVertexIndex};
        geometry::Error error =
            addTriangle(out, poly, 0, 1, 2, polygon_vertices, texture, flags, room, source, warnings);
        if (error != geometry::Error::kNone) return level_validation::geometryError(error);
        if ((poly.type & kFaceBitQuad) != 0) {
          error = addTriangle(out, poly, 3, 2, 1, polygon_vertices, texture, flags, room, source, warnings);
          if (error != geometry::Error::kNone) return level_validation::geometryError(error);
        }
        color_index += corners;
      }
    }
  }
  importRoomDistances(fts, out, warnings);
  if (out.geometry.faces.empty()) return ARX_OK;

  ArxAabb bounds;
  bool first = true;
  for (const Face& face : out.geometry.faces) {
    for (const Corner& corner : face.corners) {
      const ArxVector3& position = out.geometry.vertices[corner.vertex].position;
      if (first) {
        bounds.min = position;
        bounds.max = position;
        first = false;
      } else {
        math::expand(bounds, position);
      }
    }
  }

  std::vector<std::uint32_t> remap(fts.anchors.size(), std::numeric_limits<std::uint32_t>::max());
  out.navigation.anchors.reserve(fts.anchors.size());
  for (std::size_t anchor_index = 0; anchor_index < fts.anchors.size(); ++anchor_index) {
    const fts::Anchor& source_anchor = fts.anchors[anchor_index];
    const ArxVector3& position = source_anchor.data.pos;
    if (!level_anchor_bounds::insideNativeMap(position)) {
      ++warnings.discarded_out_of_bounds_anchors;
      continue;
    }
    if (level_anchor_bounds::materiallyOutsideGeometry(bounds, position)) ++warnings.outside_geometry_anchors;
    remap[anchor_index] = static_cast<std::uint32_t>(out.navigation.anchors.size());
    out.navigation.anchors.push_back(
        {source_anchor.data.pos, source_anchor.data.radius, source_anchor.data.height, source_anchor.data.flags, {}});
  }

  for (std::size_t anchor_index = 0; anchor_index < fts.anchors.size(); ++anchor_index) {
    std::uint32_t first_anchor = remap[anchor_index];
    if (first_anchor == std::numeric_limits<std::uint32_t>::max()) continue;
    for (std::int32_t linked : fts.anchors[anchor_index].linked) {
      if (linked < 0 || static_cast<std::size_t>(linked) >= remap.size()) continue;
      std::uint32_t second_anchor = remap[static_cast<std::size_t>(linked)];
      if (second_anchor == std::numeric_limits<std::uint32_t>::max() || first_anchor == second_anchor) continue;
      std::uint32_t a = std::min(first_anchor, second_anchor);
      std::uint32_t b = std::max(first_anchor, second_anchor);
      out.navigation.connections.push_back({a, b});
    }
  }
  std::sort(out.navigation.connections.begin(),
            out.navigation.connections.end(),
            [](const AnchorConnection& a, const AnchorConnection& b) {
              return a.first < b.first || (a.first == b.first && a.second < b.second);
            });
  out.navigation.connections.erase(std::unique(out.navigation.connections.begin(),
                                               out.navigation.connections.end(),
                                               [](const AnchorConnection& a, const AnchorConnection& b) {
                                                 return a.first == b.first && a.second == b.second;
                                               }),
                                   out.navigation.connections.end());
  return ARX_OK;
}

}  // namespace pistoris::level_native
