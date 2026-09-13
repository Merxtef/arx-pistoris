// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/runtime/types.h"

#include "modules/geometry.h"
#include "modules/navigation.h"
#include "utils/identifier.h"
#include "utils/log.h"
#include "utils/math/finite.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_set>

namespace pistoris::navigation {

bool validNavSurfaceSourceOptions(const NavSurfaceSourceOptions& options) noexcept {
  return math::finite(options.clearance) && options.clearance >= 0.0f && math::finite(options.support_min_up_cos) &&
         options.support_min_up_cos >= 0.0f && options.support_min_up_cos <= 1.0f &&
         (options.support_ignore_flags & ~kFaceBitsAll) == 0;
}

Error validateAnchor(const Anchor& anchor) noexcept {
  if (!isIdentifier(anchor.name, {.allow_empty = true})) return Error::kBadAnchorName;
  if (!math::finite(anchor.position)) return Error::kBadAnchorPosition;
  if (!math::finite(anchor.radius) || anchor.radius < 0.0f) return Error::kBadAnchorRadius;
  if (!math::finite(anchor.height) || anchor.height > 0.0f) return Error::kBadAnchorHeight;
  if ((anchor.flags & ~kAnchorFlagsAll) != 0) return Error::kBadAnchorFlags;
  return Error::kNone;
}

Error validateAnchorCount(std::size_t count) noexcept {
  return count > static_cast<std::size_t>(kInvalidAnchorIndex) ? Error::kTooManyAnchors : Error::kNone;
}

Error validateAnchorDefinitions(std::span<const Anchor> anchors) {
  if (anchors.size() > static_cast<std::size_t>(kInvalidAnchorIndex)) {
    log(ARX_LOG_DEBUG,
        "Navigation validation: anchor count {} exceeds limit {}",
        anchors.size(),
        static_cast<std::size_t>(kInvalidAnchorIndex));
    return Error::kTooManyAnchors;
  }
  std::unordered_set<std::string_view> names;
  names.reserve(anchors.size());
  for (std::size_t index = 0; index < anchors.size(); ++index) {
    const Anchor& anchor = anchors[index];
    Error error = validateAnchor(anchor);
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Navigation validation: anchor {} '{}' is invalid: error {}",
          index,
          anchor.name,
          static_cast<int>(error));
      return error;
    }
    if (!anchor.name.empty() && !names.insert(anchor.name).second) {
      log(ARX_LOG_DEBUG, "Navigation validation: anchor {} duplicates name '{}'", index, anchor.name);
      return Error::kDuplicateAnchorName;
    }
  }
  return Error::kNone;
}

Error validateConnection(const AnchorConnection& connection, std::size_t anchor_count) {
  if (connection.first >= anchor_count || connection.second >= anchor_count) return Error::kBadConnectionIndex;
  if (connection.first >= connection.second) return Error::kBadConnectionOrder;
  return Error::kNone;
}

Error validateConnections(std::span<const Anchor> anchors, std::span<const AnchorConnection> connections) {
  if (connections.size() > static_cast<std::size_t>(kInvalidAnchorConnectionIndex)) {
    log(ARX_LOG_DEBUG,
        "Navigation validation: connection count {} exceeds limit {}",
        connections.size(),
        static_cast<std::size_t>(kInvalidAnchorConnectionIndex));
    return Error::kTooManyConnections;
  }
  AnchorConnection previous{};
  bool has_previous = false;
  for (std::size_t index = 0; index < connections.size(); ++index) {
    const AnchorConnection& connection = connections[index];
    Error error = validateConnection(connection, anchors.size());
    if (error != Error::kNone) {
      log(ARX_LOG_DEBUG,
          "Navigation validation: connection {} ({} -> {}) is invalid for {} anchors: error {}",
          index,
          connection.first,
          connection.second,
          anchors.size(),
          static_cast<int>(error));
      return error;
    }
    if (has_previous) {
      if (connection.first == previous.first && connection.second == previous.second) {
        log(ARX_LOG_DEBUG,
            "Navigation validation: connection {} duplicates {} -> {}",
            index,
            connection.first,
            connection.second);
        return Error::kDuplicateConnection;
      }
      if (connection.first < previous.first ||
          (connection.first == previous.first && connection.second < previous.second)) {
        log(ARX_LOG_DEBUG,
            "Navigation validation: connection {} ({} -> {}) is ordered before previous {} -> {}",
            index,
            connection.first,
            connection.second,
            previous.first,
            previous.second);
        return Error::kBadConnectionOrder;
      }
    }
    previous = connection;
    has_previous = true;
  }
  return Error::kNone;
}

Error validateSurface(const NavSurface& surface) {
  if (surface.vertices.empty() || surface.triangles.empty()) {
    log(ARX_LOG_DEBUG,
        "Navigation validation: surface has {} vertices and {} triangles",
        surface.vertices.size(),
        surface.triangles.size());
    return Error::kBadSurface;
  }
  if (surface.vertices.size() > static_cast<std::size_t>(kInvalidNavSurfaceVertexIndex)) {
    log(ARX_LOG_DEBUG,
        "Navigation validation: surface vertex count {} exceeds limit {}",
        surface.vertices.size(),
        static_cast<std::size_t>(kInvalidNavSurfaceVertexIndex));
    return Error::kTooManySurfaceVertices;
  }
  for (std::size_t index = 0; index < surface.vertices.size(); ++index) {
    const Vertex& vertex = surface.vertices[index];
    if (!math::finite(vertex.position)) {
      log(ARX_LOG_DEBUG,
          "Navigation validation: surface vertex {} has invalid position ({}, {}, {})",
          index,
          vertex.position.x,
          vertex.position.y,
          vertex.position.z);
      return Error::kBadSurfaceVertex;
    }
  }
  for (std::size_t index = 0; index < surface.triangles.size(); ++index) {
    const NavSurfaceTriangle& triangle = surface.triangles[index];
    if (triangle.vertices[0] >= surface.vertices.size() || triangle.vertices[1] >= surface.vertices.size() ||
        triangle.vertices[2] >= surface.vertices.size()) {
      log(ARX_LOG_DEBUG,
          "Navigation validation: surface triangle {} references [{}, {}, {}] with vertex count {}",
          index,
          triangle.vertices[0],
          triangle.vertices[1],
          triangle.vertices[2],
          surface.vertices.size());
      return Error::kBadSurfaceTriangle;
    }
    if (triangle.vertices[0] == triangle.vertices[1] || triangle.vertices[0] == triangle.vertices[2] ||
        triangle.vertices[1] == triangle.vertices[2]) {
      log(ARX_LOG_DEBUG,
          "Navigation validation: surface triangle {} repeats a vertex in [{}, {}, {}]",
          index,
          triangle.vertices[0],
          triangle.vertices[1],
          triangle.vertices[2]);
      return Error::kDegenerateSurfaceTriangle;
    }
    if (geometry::degenerateTriangle(surface.vertices[triangle.vertices[0]].position,
                                     surface.vertices[triangle.vertices[1]].position,
                                     surface.vertices[triangle.vertices[2]].position)) {
      log(ARX_LOG_DEBUG, "Navigation validation: surface triangle {} is degenerate", index);
      return Error::kDegenerateSurfaceTriangle;
    }
  }
  return Error::kNone;
}

Error validateSurface(const std::optional<NavSurface>& surface) {
  if (!surface.has_value()) return Error::kNone;
  return validateSurface(*surface);
}

Error validate(const NavigationData& navigation) {
  Error error = validateAnchorDefinitions(navigation.anchors);
  if (error != Error::kNone) return error;
  error = validateConnections(navigation.anchors, navigation.connections);
  if (error != Error::kNone) return error;
  return validateSurface(navigation.surface);
}

}  // namespace pistoris::navigation
