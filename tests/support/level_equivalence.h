// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "doctest/doctest.h"

#include "arx_pistoris/level.hpp"

#include "support/equivalence.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace test_support {

enum class LevelEquivalenceDomain : std::uint8_t {
  kNativeBundle,
  kGlb,
};

struct LevelEquivalenceOptions {
  LevelEquivalenceDomain domain = LevelEquivalenceDomain::kNativeBundle;
  float comparison_epsilon = 1e-5f;
};

namespace level_equivalence_detail {

template <std::ranges::input_range Range>
auto materialize(Range&& range) {
  using Item = std::ranges::range_value_t<Range>;
  std::vector<Item> result;
  if constexpr (std::ranges::sized_range<Range>) result.reserve(std::ranges::size(range));
  for (Item item : range) result.push_back(std::move(item));
  return result;
}

inline std::string_view roomName(ArxRoomIndex room, std::span<const ArxLevelRoom> rooms) {
  if (room == ARX_INVALID_INDEX) return {};
  CHECK(room < rooms.size());
  if (room >= rooms.size()) return {};
  return equivalence::stringView(rooms[room].name);
}

inline std::string_view portalName(ArxPortalIndex portal, std::span<const ArxLevelPortal> portals) {
  if (portal == ARX_INVALID_INDEX) return {};
  CHECK(portal < portals.size());
  if (portal >= portals.size()) return {};
  return equivalence::stringView(portals[portal].name);
}

inline std::string_view texturePath(ArxTextureIndex texture, std::span<const ArxTextureView> textures) {
  if (texture == ARX_NO_TEXTURE) return {};
  CHECK(texture < textures.size());
  if (texture >= textures.size()) return {};
  return equivalence::stringView(textures[texture].path);
}

inline bool cornerEquivalent(const ArxLevelCorner& lhs, const ArxLevelCorner& rhs,
                             std::span<const ArxLevelVertex> lhs_vertices, std::span<const ArxLevelVertex> rhs_vertices,
                             float epsilon) {
  if (lhs.vertex >= lhs_vertices.size() || rhs.vertex >= rhs_vertices.size()) return false;
  return equivalence::vectorEquivalent(lhs_vertices[lhs.vertex].position, rhs_vertices[rhs.vertex].position, epsilon) &&
         equivalence::vectorEquivalent(lhs.normal, rhs.normal, epsilon) &&
         equivalence::floatEquivalent(lhs.u, rhs.u, epsilon) && equivalence::floatEquivalent(lhs.v, rhs.v, epsilon) &&
         equivalence::colorEquivalent(lhs.color, rhs.color, epsilon);
}

inline bool faceEquivalent(const ArxLevelFace& lhs, const ArxLevelFace& rhs,
                           std::span<const ArxLevelVertex> lhs_vertices, std::span<const ArxLevelVertex> rhs_vertices,
                           std::span<const ArxTextureView> lhs_textures, std::span<const ArxTextureView> rhs_textures,
                           std::span<const ArxLevelRoom> lhs_rooms, std::span<const ArxLevelRoom> rhs_rooms,
                           float epsilon) {
  if (texturePath(lhs.texture, lhs_textures) != texturePath(rhs.texture, rhs_textures) || lhs.flags != rhs.flags ||
      !equivalence::floatEquivalent(lhs.transval, rhs.transval, epsilon) ||
      roomName(lhs.room, lhs_rooms) != roomName(rhs.room, rhs_rooms))
    return false;
  for (std::size_t offset = 0; offset < 3; ++offset) {
    bool equivalent = true;
    for (std::size_t corner = 0; corner < 3; ++corner) {
      if (!cornerEquivalent(
              lhs.corners[corner], rhs.corners[(corner + offset) % 3], lhs_vertices, rhs_vertices, epsilon)) {
        equivalent = false;
        break;
      }
    }
    if (equivalent) return true;
  }
  return false;
}

inline ArxVector3 faceAnchor(const ArxLevelFace& face, std::span<const ArxLevelVertex> vertices) {
  ArxVector3 result = vertices[face.corners[0].vertex].position;
  for (std::size_t corner = 1; corner < 3; ++corner) {
    const ArxVector3 point = vertices[face.corners[corner].vertex].position;
    result.x = std::min(result.x, point.x);
    result.y = std::min(result.y, point.y);
    result.z = std::min(result.z, point.z);
  }
  return result;
}

template <class Item, class Name, class Equivalent>
void checkNamedItems(std::string_view collection, std::span<const Item> lhs, std::span<const Item> rhs, Name name,
                     Equivalent equivalent) {
  CAPTURE(collection);
  CHECK(lhs.size() == rhs.size());
  std::unordered_multimap<std::string_view, const Item*> rhs_by_name;
  rhs_by_name.reserve(rhs.size());
  for (const Item& item : rhs) rhs_by_name.emplace(name(item), &item);
  for (const Item& item : lhs) {
    CAPTURE(name(item));
    const auto [first, last] = rhs_by_name.equal_range(name(item));
    const auto found =
        std::find_if(first, last, [&](const auto& candidate) { return equivalent(item, *candidate.second); });
    CHECK(found != last);
    if (found == last) continue;
    rhs_by_name.erase(found);
  }
}

inline bool portalEquivalent(const ArxLevelPortal& lhs, const ArxLevelPortal& rhs,
                             std::span<const ArxLevelRoom> lhs_rooms, std::span<const ArxLevelRoom> rhs_rooms,
                             float epsilon) {
  if (lhs.shape != rhs.shape || roomName(lhs.room_1, lhs_rooms) != roomName(rhs.room_1, rhs_rooms) ||
      roomName(lhs.room_2, lhs_rooms) != roomName(rhs.room_2, rhs_rooms))
    return false;
  for (std::size_t vertex = 0; vertex < lhs.shape; ++vertex) {
    if (!equivalence::vectorEquivalent(lhs.vertices[vertex], rhs.vertices[vertex], epsilon)) return false;
  }
  return true;
}

inline bool lightEquivalent(const ArxLevelLight& lhs, const ArxLevelLight& rhs, float epsilon) {
  return equivalence::vectorEquivalent(lhs.position, rhs.position, epsilon) &&
         equivalence::colorEquivalent(lhs.color, rhs.color, epsilon) &&
         equivalence::floatEquivalent(lhs.fallstart, rhs.fallstart, epsilon) &&
         equivalence::floatEquivalent(lhs.fallend, rhs.fallend, epsilon) &&
         equivalence::floatEquivalent(lhs.intensity, rhs.intensity, epsilon) &&
         equivalence::colorEquivalent(lhs.flicker, rhs.flicker, epsilon) &&
         equivalence::floatEquivalent(lhs.effect_radius, rhs.effect_radius, epsilon) &&
         equivalence::floatEquivalent(lhs.effect_frequency, rhs.effect_frequency, epsilon) &&
         equivalence::floatEquivalent(lhs.effect_size, rhs.effect_size, epsilon) &&
         equivalence::floatEquivalent(lhs.effect_speed, rhs.effect_speed, epsilon) &&
         equivalence::floatEquivalent(lhs.flare_size, rhs.flare_size, epsilon) && lhs.flags == rhs.flags;
}

inline bool fogEquivalent(const ArxLevelFog& lhs, const ArxLevelFog& rhs, float epsilon) {
  return equivalence::vectorEquivalent(lhs.position, rhs.position, epsilon) &&
         equivalence::colorEquivalent(lhs.color, rhs.color, epsilon) &&
         equivalence::floatEquivalent(lhs.size, rhs.size, epsilon) && lhs.directional == rhs.directional &&
         equivalence::floatEquivalent(lhs.scale, rhs.scale, epsilon) &&
         equivalence::rotationEquivalent(lhs.rotation, rhs.rotation, epsilon) &&
         equivalence::floatEquivalent(lhs.speed, rhs.speed, epsilon) &&
         equivalence::floatEquivalent(lhs.rotate_speed, rhs.rotate_speed, epsilon) &&
         lhs.lifetime_ms == rhs.lifetime_ms && equivalence::floatEquivalent(lhs.frequency, rhs.frequency, epsilon);
}

inline std::vector<ArxVector2> zonePerimeter(const pistoris::Level& level, pistoris::ZoneIndex index,
                                             std::size_t count) {
  const auto perimeter = level.zonePerimeter(index);
  CHECK(perimeter);
  if (!perimeter) return {};
  CHECK(perimeter->size() == count);
  return materialize(*perimeter);
}

inline bool cyclicPerimeterEquivalent(std::span<const ArxVector2> lhs, std::span<const ArxVector2> rhs, float epsilon) {
  if (lhs.size() != rhs.size()) return false;
  if (lhs.empty()) return true;
  for (std::size_t offset = 0; offset < rhs.size(); ++offset) {
    bool equivalent = true;
    for (std::size_t point = 0; point < lhs.size(); ++point) {
      if (!equivalence::vectorEquivalent(lhs[point], rhs[(point + offset) % rhs.size()], epsilon)) {
        equivalent = false;
        break;
      }
    }
    if (equivalent) return true;
    equivalent = true;
    for (std::size_t point = 0; point < lhs.size(); ++point) {
      const std::size_t rhs_point = (offset + rhs.size() - point) % rhs.size();
      if (!equivalence::vectorEquivalent(lhs[point], rhs[rhs_point], epsilon)) {
        equivalent = false;
        break;
      }
    }
    if (equivalent) return true;
  }
  return false;
}

struct NamedConnection {
  std::string_view first;
  std::string_view second;

  auto operator<=>(const NamedConnection&) const = default;
};

inline std::optional<std::vector<NamedConnection>> namedConnections(
    std::span<const ArxLevelAnchorConnection> connections, std::span<const ArxLevelAnchor> anchors) {
  std::vector<NamedConnection> result;
  result.reserve(connections.size());
  for (const ArxLevelAnchorConnection& connection : connections) {
    const bool first_valid = connection.first < anchors.size();
    const bool second_valid = connection.second < anchors.size();
    CHECK(first_valid);
    CHECK(second_valid);
    if (!first_valid || !second_valid) return std::nullopt;
    NamedConnection named{equivalence::stringView(anchors[connection.first].name),
                          equivalence::stringView(anchors[connection.second].name)};
    if (named.second < named.first) std::swap(named.first, named.second);
    result.push_back(named);
  }
  std::ranges::sort(result);
  return result;
}

}  // namespace level_equivalence_detail

inline void checkLevelsEquivalent(const pistoris::Level& lhs, const pistoris::Level& rhs,
                                  LevelEquivalenceOptions options = {}) {
  namespace detail = level_equivalence_detail;
  const float epsilon = options.comparison_epsilon;

  CHECK(lhs.resourcePath() == rhs.resourcePath());

  const std::vector<ArxLevelVertex> lhs_vertices = detail::materialize(lhs.vertices());
  const std::vector<ArxLevelVertex> rhs_vertices = detail::materialize(rhs.vertices());
  const std::vector<ArxLevelFace> lhs_faces = detail::materialize(lhs.faces());
  const std::vector<ArxLevelFace> rhs_faces = detail::materialize(rhs.faces());
  const std::vector<ArxTextureView> lhs_textures = detail::materialize(lhs.textures());
  const std::vector<ArxTextureView> rhs_textures = detail::materialize(rhs.textures());
  const std::vector<ArxLevelRoom> lhs_rooms = detail::materialize(lhs.rooms());
  const std::vector<ArxLevelRoom> rhs_rooms = detail::materialize(rhs.rooms());

  bool vertex_indices_valid = true;
  for (const ArxLevelFace& face : lhs_faces)
    for (const ArxLevelCorner& corner : face.corners) {
      const bool valid = corner.vertex < lhs_vertices.size();
      CHECK(valid);
      vertex_indices_valid = vertex_indices_valid && valid;
    }
  for (const ArxLevelFace& face : rhs_faces)
    for (const ArxLevelCorner& corner : face.corners) {
      const bool valid = corner.vertex < rhs_vertices.size();
      CHECK(valid);
      vertex_indices_valid = vertex_indices_valid && valid;
    }
  if (!vertex_indices_valid) return;
  const float coordinate_scale =
      std::max(equivalence::maxAbsCoordinate<ArxLevelVertex>(
                   lhs_vertices, [](const ArxLevelVertex& vertex) { return vertex.position; }),
               equivalence::maxAbsCoordinate<ArxLevelVertex>(
                   rhs_vertices, [](const ArxLevelVertex& vertex) { return vertex.position; }));
  CHECK(equivalence::spatialMultisetEquivalent<ArxLevelFace>(
      lhs_faces,
      rhs_faces,
      [&](const ArxLevelFace& face) { return detail::faceAnchor(face, lhs_vertices); },
      [&](const ArxLevelFace& face) { return detail::faceAnchor(face, rhs_vertices); },
      [&](const ArxLevelFace& left, const ArxLevelFace& right) {
        return detail::faceEquivalent(
            left, right, lhs_vertices, rhs_vertices, lhs_textures, rhs_textures, lhs_rooms, rhs_rooms, epsilon);
      },
      epsilon,
      coordinate_scale));

  std::unordered_set<std::string_view> used_texture_paths;
  for (const ArxLevelFace& face : lhs_faces) {
    const std::string_view path = detail::texturePath(face.texture, lhs_textures);
    if (!path.empty()) used_texture_paths.insert(path);
  }
  std::unordered_map<std::string_view, ArxEncodedImageView> rhs_images;
  for (const ArxTextureView& texture : rhs_textures)
    rhs_images.emplace(equivalence::stringView(texture.path), texture.encoded_image);
  for (const ArxTextureView& texture : lhs_textures) {
    const std::string_view path = equivalence::stringView(texture.path);
    if (!used_texture_paths.contains(path)) continue;
    const auto found = rhs_images.find(path);
    CHECK(found != rhs_images.end());
    if (found == rhs_images.end()) continue;
    equivalence::checkImageViewsEqual(texture.encoded_image, found->second);
  }

  std::unordered_set<std::string_view> active_rooms;
  for (const ArxLevelFace& face : lhs_faces) active_rooms.insert(detail::roomName(face.room, lhs_rooms));
  std::vector<ArxLevelRoom> compared_lhs_rooms;
  std::vector<ArxLevelRoom> compared_rhs_rooms;
  for (const ArxLevelRoom& room : lhs_rooms) {
    if (options.domain == LevelEquivalenceDomain::kNativeBundle ||
        active_rooms.contains(equivalence::stringView(room.name)))
      compared_lhs_rooms.push_back(room);
  }
  for (const ArxLevelRoom& room : rhs_rooms) {
    if (options.domain == LevelEquivalenceDomain::kNativeBundle ||
        active_rooms.contains(equivalence::stringView(room.name)))
      compared_rhs_rooms.push_back(room);
  }
  detail::checkNamedItems<ArxLevelRoom>(
      "rooms",
      compared_lhs_rooms,
      compared_rhs_rooms,
      [](const ArxLevelRoom& room) { return equivalence::stringView(room.name); },
      [](const ArxLevelRoom&, const ArxLevelRoom&) { return true; });

  std::vector<ArxLevelPortal> lhs_portals = detail::materialize(lhs.portals());
  std::vector<ArxLevelPortal> rhs_portals = detail::materialize(rhs.portals());
  if (options.domain == LevelEquivalenceDomain::kGlb) {
    std::erase_if(lhs_portals, [&](const ArxLevelPortal& portal) {
      return !active_rooms.contains(detail::roomName(portal.room_1, lhs_rooms)) ||
             !active_rooms.contains(detail::roomName(portal.room_2, lhs_rooms));
    });
    std::erase_if(rhs_portals, [&](const ArxLevelPortal& portal) {
      return !active_rooms.contains(detail::roomName(portal.room_1, rhs_rooms)) ||
             !active_rooms.contains(detail::roomName(portal.room_2, rhs_rooms));
    });
  }
  detail::checkNamedItems<ArxLevelPortal>(
      "portals",
      lhs_portals,
      rhs_portals,
      [](const ArxLevelPortal& portal) { return equivalence::stringView(portal.name); },
      [&](const ArxLevelPortal& left, const ArxLevelPortal& right) {
        return detail::portalEquivalent(left, right, lhs_rooms, rhs_rooms, epsilon);
      });

  if (options.domain == LevelEquivalenceDomain::kNativeBundle) {
    const std::vector<ArxLevelRoomDistance> lhs_distances = detail::materialize(lhs.roomDistances());
    const std::vector<ArxLevelRoomDistance> rhs_distances = detail::materialize(rhs.roomDistances());
    CHECK(lhs_distances.size() == rhs_distances.size());
    std::vector<bool> matched(rhs_distances.size(), false);
    for (const ArxLevelRoomDistance& left : lhs_distances) {
      const auto found = std::ranges::find_if(rhs_distances, [&](const ArxLevelRoomDistance& right) {
        const bool same_rooms = detail::roomName(left.room_a, lhs_rooms) == detail::roomName(right.room_a, rhs_rooms) &&
                                detail::roomName(left.room_b, lhs_rooms) == detail::roomName(right.room_b, rhs_rooms);
        return same_rooms &&
               detail::portalName(left.portal_a, lhs_portals) == detail::portalName(right.portal_a, rhs_portals) &&
               detail::portalName(left.portal_b, lhs_portals) == detail::portalName(right.portal_b, rhs_portals) &&
               equivalence::floatEquivalent(left.distance, right.distance, epsilon) &&
               !matched[static_cast<std::size_t>(&right - rhs_distances.data())];
      });
      CHECK(found != rhs_distances.end());
      if (found == rhs_distances.end()) continue;
      matched[static_cast<std::size_t>(&*found - rhs_distances.data())] = true;
    }
  }

  const std::vector<ArxLevelAnchor> lhs_anchors = detail::materialize(lhs.anchors());
  const std::vector<ArxLevelAnchor> rhs_anchors = detail::materialize(rhs.anchors());
  detail::checkNamedItems<ArxLevelAnchor>(
      "anchors",
      lhs_anchors,
      rhs_anchors,
      [](const ArxLevelAnchor& anchor) { return equivalence::stringView(anchor.name); },
      [&](const ArxLevelAnchor& left, const ArxLevelAnchor& right) {
        return equivalence::vectorEquivalent(left.position, right.position, epsilon) &&
               equivalence::floatEquivalent(left.radius, right.radius, epsilon) &&
               equivalence::floatEquivalent(left.height, right.height, epsilon) && left.flags == right.flags;
      });
  const std::vector<ArxLevelAnchorConnection> lhs_connection_items = detail::materialize(lhs.anchorConnections());
  const std::vector<ArxLevelAnchorConnection> rhs_connection_items = detail::materialize(rhs.anchorConnections());
  const auto lhs_connections = detail::namedConnections(lhs_connection_items, lhs_anchors);
  const auto rhs_connections = detail::namedConnections(rhs_connection_items, rhs_anchors);
  if (!lhs_connections || !rhs_connections) return;
  CHECK(*lhs_connections == *rhs_connections);

  const ArxLevelNavSurfaceInfo lhs_nav = lhs.navSurfaceInfo();
  const ArxLevelNavSurfaceInfo rhs_nav = rhs.navSurfaceInfo();
  CHECK(lhs_nav.has_surface == rhs_nav.has_surface);
  if (lhs_nav.has_surface != 0 && rhs_nav.has_surface != 0) {
    const std::vector<ArxLevelVertex> lhs_nav_vertices = detail::materialize(lhs.navSurfaceVertices());
    const std::vector<ArxLevelVertex> rhs_nav_vertices = detail::materialize(rhs.navSurfaceVertices());
    const std::vector<ArxLevelNavSurfaceTriangle> lhs_triangles = detail::materialize(lhs.navSurfaceTriangles());
    const std::vector<ArxLevelNavSurfaceTriangle> rhs_triangles = detail::materialize(rhs.navSurfaceTriangles());
    bool nav_indices_valid = true;
    for (const ArxLevelNavSurfaceTriangle& triangle : lhs_triangles) {
      for (ArxVertexIndex vertex : triangle.vertices) {
        const bool valid = vertex < lhs_nav_vertices.size();
        CHECK(valid);
        nav_indices_valid = nav_indices_valid && valid;
      }
    }
    for (const ArxLevelNavSurfaceTriangle& triangle : rhs_triangles) {
      for (ArxVertexIndex vertex : triangle.vertices) {
        const bool valid = vertex < rhs_nav_vertices.size();
        CHECK(valid);
        nav_indices_valid = nav_indices_valid && valid;
      }
    }
    if (!nav_indices_valid) return;
    const auto anchor = [](const ArxLevelNavSurfaceTriangle& triangle, std::span<const ArxLevelVertex> vertices) {
      ArxVector3 result = vertices[triangle.vertices[0]].position;
      for (std::size_t vertex = 1; vertex < 3; ++vertex) {
        const ArxVector3 point = vertices[triangle.vertices[vertex]].position;
        result.x = std::min(result.x, point.x);
        result.y = std::min(result.y, point.y);
        result.z = std::min(result.z, point.z);
      }
      return result;
    };
    CHECK(equivalence::spatialMultisetEquivalent<ArxLevelNavSurfaceTriangle>(
        lhs_triangles,
        rhs_triangles,
        [&](const ArxLevelNavSurfaceTriangle& triangle) { return anchor(triangle, lhs_nav_vertices); },
        [&](const ArxLevelNavSurfaceTriangle& triangle) { return anchor(triangle, rhs_nav_vertices); },
        [&](const ArxLevelNavSurfaceTriangle& left, const ArxLevelNavSurfaceTriangle& right) {
          for (std::size_t offset = 0; offset < 3; ++offset) {
            bool same = true;
            for (std::size_t vertex = 0; vertex < 3; ++vertex) {
              if (!equivalence::vectorEquivalent(lhs_nav_vertices[left.vertices[vertex]].position,
                                                 rhs_nav_vertices[right.vertices[(vertex + offset) % 3]].position,
                                                 epsilon)) {
                same = false;
                break;
              }
            }
            if (same) return true;
          }
          return false;
        },
        epsilon));
  }

  const std::vector<ArxLevelLight> lhs_lights = detail::materialize(lhs.lights());
  const std::vector<ArxLevelLight> rhs_lights = detail::materialize(rhs.lights());
  detail::checkNamedItems<ArxLevelLight>(
      "lights",
      lhs_lights,
      rhs_lights,
      [](const ArxLevelLight& light) { return equivalence::stringView(light.name); },
      [&](const ArxLevelLight& left, const ArxLevelLight& right) {
        return detail::lightEquivalent(left, right, epsilon);
      });

  const ArxLevelPlayerSpawn lhs_spawn = lhs.playerSpawn();
  const ArxLevelPlayerSpawn rhs_spawn = rhs.playerSpawn();
  CHECK(lhs_spawn.is_usable == rhs_spawn.is_usable);
  if (lhs_spawn.is_usable != 0) {
    CHECK(equivalence::vectorEquivalent(lhs_spawn.position, rhs_spawn.position, epsilon));
    CHECK(equivalence::rotationEquivalent(lhs_spawn.rotation, rhs_spawn.rotation, epsilon));
  }

  const std::vector<ArxLevelEntity> lhs_entities = detail::materialize(lhs.entities());
  const std::vector<ArxLevelEntity> rhs_entities = detail::materialize(rhs.entities());
  CHECK(lhs_entities.size() == rhs_entities.size());
  std::unordered_map<std::string_view, const ArxLevelEntity*> rhs_entity_by_name;
  for (const ArxLevelEntity& entity : rhs_entities) {
    CHECK(rhs_entity_by_name.emplace(equivalence::stringView(entity.name), &entity).second);
  }
  for (const ArxLevelEntity& left : lhs_entities) {
    CAPTURE(equivalence::stringView(left.name));
    const auto found = rhs_entity_by_name.find(equivalence::stringView(left.name));
    CHECK(found != rhs_entity_by_name.end());
    if (found == rhs_entity_by_name.end()) continue;
    const ArxLevelEntity& right = *found->second;
    CHECK(equivalence::stringView(left.class_path) == equivalence::stringView(right.class_path));
    CHECK(left.ident == right.ident);
    CHECK(equivalence::vectorEquivalent(left.position, right.position, epsilon));
    const bool rotation_equivalent = equivalence::rotationEquivalent(left.rotation, right.rotation, epsilon);
    if (!rotation_equivalent) {
      CAPTURE(left.rotation.w);
      CAPTURE(left.rotation.x);
      CAPTURE(left.rotation.y);
      CAPTURE(left.rotation.z);
      CAPTURE(right.rotation.w);
      CAPTURE(right.rotation.x);
      CAPTURE(right.rotation.y);
      CAPTURE(right.rotation.z);
      CHECK(rotation_equivalent);
    }
  }

  const std::vector<ArxLevelFog> lhs_fogs = detail::materialize(lhs.fogs());
  const std::vector<ArxLevelFog> rhs_fogs = detail::materialize(rhs.fogs());
  detail::checkNamedItems<ArxLevelFog>(
      "fogs",
      lhs_fogs,
      rhs_fogs,
      [](const ArxLevelFog& fog) { return equivalence::stringView(fog.name); },
      [&](const ArxLevelFog& left, const ArxLevelFog& right) { return detail::fogEquivalent(left, right, epsilon); });

  const std::vector<ArxLevelZone> lhs_zones = detail::materialize(lhs.zones());
  const std::vector<ArxLevelZone> rhs_zones = detail::materialize(rhs.zones());
  CHECK(lhs_zones.size() == rhs_zones.size());
  std::unordered_map<std::string_view, pistoris::ZoneIndex> rhs_zone_by_name;
  for (pistoris::ZoneIndex index = 0; index < rhs_zones.size(); ++index) {
    CHECK(rhs_zone_by_name.emplace(equivalence::stringView(rhs_zones[index].name), index).second);
  }
  for (pistoris::ZoneIndex index = 0; index < lhs_zones.size(); ++index) {
    const ArxLevelZone& left = lhs_zones[index];
    const auto found = rhs_zone_by_name.find(equivalence::stringView(left.name));
    CHECK(found != rhs_zone_by_name.end());
    if (found == rhs_zone_by_name.end()) continue;
    const ArxLevelZone& right = rhs_zones[found->second];
    CHECK(left.height_mode == right.height_mode);
    CHECK(left.has_color == right.has_color);
    CHECK(left.has_farclip == right.has_farclip);
    CHECK(left.has_ambiance == right.has_ambiance);
    if (left.height_mode == ARX_ZONE_HEIGHT_FINITE) {
      CHECK(equivalence::floatEquivalent(left.reference_y, right.reference_y, epsilon));
      CHECK(equivalence::floatEquivalent(left.height, right.height, epsilon));
    }
    if (left.has_color != 0) CHECK(equivalence::colorEquivalent(left.color, right.color, epsilon));
    if (left.has_farclip != 0) CHECK(equivalence::floatEquivalent(left.farclip, right.farclip, epsilon));
    if (left.has_ambiance != 0) {
      CHECK(equivalence::stringView(left.ambiance.name) == equivalence::stringView(right.ambiance.name));
      CHECK(equivalence::floatEquivalent(left.ambiance.volume, right.ambiance.volume, epsilon));
    }
    const auto left_perimeter = detail::zonePerimeter(lhs, index, left.perimeter_count);
    const auto right_perimeter = detail::zonePerimeter(rhs, found->second, right.perimeter_count);
    CHECK(detail::cyclicPerimeterEquivalent(left_perimeter, right_perimeter, epsilon));
  }

  const std::vector<ArxLevelPath> lhs_paths = detail::materialize(lhs.paths());
  const std::vector<ArxLevelPath> rhs_paths = detail::materialize(rhs.paths());
  CHECK(lhs_paths.size() == rhs_paths.size());
  std::unordered_map<std::string_view, pistoris::PathIndex> rhs_path_by_name;
  for (pistoris::PathIndex index = 0; index < rhs_paths.size(); ++index) {
    CHECK(rhs_path_by_name.emplace(equivalence::stringView(rhs_paths[index].name), index).second);
  }
  for (pistoris::PathIndex index = 0; index < lhs_paths.size(); ++index) {
    const ArxLevelPath& left = lhs_paths[index];
    const auto found = rhs_path_by_name.find(equivalence::stringView(left.name));
    CHECK(found != rhs_path_by_name.end());
    if (found == rhs_path_by_name.end()) continue;
    const ArxLevelPath& right = rhs_paths[found->second];
    CHECK(equivalence::vectorEquivalent(left.position, right.position, epsilon));
    CHECK(left.node_count == right.node_count);
    const auto lhs_nodes_result = lhs.pathNodes(index);
    const auto rhs_nodes_result = rhs.pathNodes(found->second);
    CHECK(lhs_nodes_result);
    CHECK(rhs_nodes_result);
    if (!lhs_nodes_result || !rhs_nodes_result) return;
    const std::vector<ArxLevelPathNode> lhs_nodes = detail::materialize(*lhs_nodes_result);
    const std::vector<ArxLevelPathNode> rhs_nodes = detail::materialize(*rhs_nodes_result);
    for (std::size_t node = 0; node < std::min(lhs_nodes.size(), rhs_nodes.size()); ++node) {
      CHECK(
          equivalence::vectorEquivalent(lhs_nodes[node].relative_position, rhs_nodes[node].relative_position, epsilon));
      CHECK(lhs_nodes[node].type == rhs_nodes[node].type);
      CHECK(lhs_nodes[node].time_ms == rhs_nodes[node].time_ms);
    }
  }

  if (options.domain == LevelEquivalenceDomain::kGlb) {
    const pistoris::Level::MinimapView lhs_minimap = lhs.minimap();
    const pistoris::Level::MinimapView rhs_minimap = rhs.minimap();
    equivalence::checkImageViewsEqual(lhs_minimap.encoded_image, rhs_minimap.encoded_image);
    CHECK(equivalence::floatEquivalent(lhs_minimap.world_xz_bounds.min.x, rhs_minimap.world_xz_bounds.min.x, epsilon));
    CHECK(equivalence::floatEquivalent(lhs_minimap.world_xz_bounds.min.y, rhs_minimap.world_xz_bounds.min.y, epsilon));
    CHECK(equivalence::floatEquivalent(lhs_minimap.world_xz_bounds.max.x, rhs_minimap.world_xz_bounds.max.x, epsilon));
    CHECK(equivalence::floatEquivalent(lhs_minimap.world_xz_bounds.max.y, rhs_minimap.world_xz_bounds.max.y, epsilon));
  }
}

}  // namespace test_support
