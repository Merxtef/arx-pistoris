// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/level/operations.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/debug/level/diagnostics.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/runtime.hpp"

#include "console/diagnostics.h"
#include "routes/conversion_failure.h"
#include "routes/level/options.h"

#include <variant>

namespace cli::level::operations {
namespace {

bool operationFailure(const char* what, ArxReturnCode rc) {
  diagnostic(DiagnosticCode::kLevelModuleFailed,
             "%s failed: %s (code %d)",
             what,
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

template <class Result>
bool resultFailure(const char* what, const Result& result) {
  return conversionStageFailure(DiagnosticCode::kLevelModuleFailed, what, result);
}

}  // namespace

bool apply(pistoris::Level& level, const LevelOptions& options, OperationDiagnostics& diagnostics) {
  const bool has_navigation_operations = options.generate_nav_surface || options.prune_nav_surface_islands ||
                                         options.generate_anchors || options.connect_anchors ||
                                         options.prune_anchor_islands;
  if (std::holds_alternative<pistoris::level_debug::NavigationDiagnostics>(diagnostics) && !has_navigation_operations) {
    diagnostics.emplace<std::monostate>();
  }
  if (std::holds_alternative<pistoris::level_debug::RoomDistanceGenDiagnostics>(diagnostics) &&
      !options.generate_room_distances) {
    diagnostics.emplace<std::monostate>();
  }
  auto* navigation = std::get_if<pistoris::level_debug::NavigationDiagnostics>(&diagnostics);
  auto* room_distances = std::get_if<pistoris::level_debug::RoomDistanceGenDiagnostics>(&diagnostics);

  if (options.weld_vertices) {
    const auto result = level.weldVertices(options.vertex_welding);
    if (!result) {
      resultFailure("Level vertex welding", result);
      return false;
    }
  }

  if (options.flatten_portals) {
    const auto result = level.flattenPortals();
    if (!result) {
      resultFailure("Level portal flattening", result);
      return false;
    }
  }

  if (options.snap_to_portals) {
    const auto result = level.snapGeometryToPortals(options.portal_snapping);
    if (!result) {
      resultFailure("Level portal snapping", result);
      return false;
    }
  }

  if (options.generate_nav_surface) {
    if (navigation) {
      const ArxReturnCode result =
          options.nav_surface_from_floor
              ? pistoris::level_debug::setNavSurfaceFromFloor(
                    level,
                    static_cast<const pistoris::Level::NavSurfaceSourceOptions&>(options.nav_surface_generation),
                    *navigation)
              : pistoris::level_debug::generateNavSurface(level, options.nav_surface_generation, *navigation);
      if (result != ARX_OK) return operationFailure("Level navigation surface generation", result);
    } else if (options.nav_surface_from_floor) {
      const auto result = level.setNavSurfaceFromFloor(
          static_cast<const pistoris::Level::NavSurfaceSourceOptions&>(options.nav_surface_generation));
      if (!result) return resultFailure("Level navigation surface generation", result);
    } else {
      const auto result = level.generateNavSurface(options.nav_surface_generation);
      if (!result) return resultFailure("Level navigation surface generation", result);
    }
  }

  if (options.prune_nav_surface_islands) {
    if (navigation) {
      const ArxReturnCode result =
          pistoris::level_debug::pruneNavSurfaceIslands(level, options.nav_surface_pruning, *navigation);
      if (result != ARX_OK) return operationFailure("Level navigation surface pruning", result);
    } else {
      const auto result = level.pruneNavSurfaceIslands(options.nav_surface_pruning);
      if (!result) return resultFailure("Level navigation surface pruning", result);
    }
  }

  if (options.generate_anchors) {
    pistoris::Level::AnchorGenOptions generation = effectiveAnchorGenerationOptions(options);
    if (navigation) {
      const ArxReturnCode result = pistoris::level_debug::generateAnchors(level, generation, *navigation);
      if (result != ARX_OK) return operationFailure("Level anchor generation", result);
    } else {
      const auto result = level.generateAnchors(generation);
      if (!result) return resultFailure("Level anchor generation", result);
    }
  }

  if (options.connect_anchors) {
    pistoris::Level::AnchorGenOptions generation = effectiveAnchorGenerationOptions(options);
    pistoris::Level::AnchorConnectionGenOptions connection = effectiveAnchorConnectionOptions(options, generation);
    if (navigation) {
      const ArxReturnCode result = pistoris::level_debug::generateAnchorConnections(level, connection, *navigation);
      if (result != ARX_OK) return operationFailure("Level anchor link generation", result);
    } else {
      const auto result = level.generateAnchorConnections(connection);
      if (!result) return resultFailure("Level anchor link generation", result);
    }
  }

  if (options.prune_anchor_islands) {
    if (navigation) {
      const ArxReturnCode result =
          pistoris::level_debug::pruneAnchorIslands(level, options.anchor_pruning, *navigation);
      if (result != ARX_OK) return operationFailure("Level anchor pruning", result);
    } else {
      const auto result = level.pruneAnchorIslands(options.anchor_pruning);
      if (!result) return resultFailure("Level anchor pruning", result);
    }
  }

  if (options.generate_room_distances) {
    if (room_distances) {
      const ArxReturnCode result =
          pistoris::level_debug::generateRoomDistances(level, options.room_distance_generation, *room_distances);
      if (result != ARX_OK) return operationFailure("Level room-distance generation", result);
    } else {
      const auto result = level.generateRoomDistances(options.room_distance_generation);
      if (!result) return resultFailure("Level room-distance generation", result);
    }
  }

  if (options.generate_static_lighting) {
    const auto result = level.generateStaticLighting(options.static_lighting_generation);
    if (!result) {
      resultFailure("Level static lighting generation", result);
      return false;
    }
  }

  if (options.generate_minimap) {
    const auto result = level.generateMinimap(options.minimap_generation);
    if (!result) {
      resultFailure("Level minimap generation", result);
      return false;
    }
  }
  return true;
}

}  // namespace cli::level::operations
