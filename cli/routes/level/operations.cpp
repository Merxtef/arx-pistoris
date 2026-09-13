// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/level/operations.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/debug/level/diagnostics.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/runtime.hpp"

#include "console/diagnostics.h"
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
    ArxReturnCode rc = level.weldVertices(options.vertex_welding);
    if (rc != ARX_OK) {
      operationFailure("Level vertex welding", rc);
      return false;
    }
  }

  if (options.generate_nav_surface) {
    ArxReturnCode rc;
    if (options.nav_surface_from_floor) {
      const auto& generation =
          static_cast<const pistoris::Level::NavSurfaceSourceOptions&>(options.nav_surface_generation);
      rc = navigation ? pistoris::level_debug::setNavSurfaceFromFloor(level, generation, *navigation)
                      : level.setNavSurfaceFromFloor(generation);
    } else {
      rc = navigation ? pistoris::level_debug::generateNavSurface(level, options.nav_surface_generation, *navigation)
                      : level.generateNavSurface(options.nav_surface_generation);
    }
    if (rc != ARX_OK) {
      operationFailure("Level navigation surface generation", rc);
      return false;
    }
  }

  if (options.prune_nav_surface_islands) {
    ArxReturnCode rc =
        navigation ? pistoris::level_debug::pruneNavSurfaceIslands(level, options.nav_surface_pruning, *navigation)
                   : level.pruneNavSurfaceIslands(options.nav_surface_pruning);
    if (rc != ARX_OK) {
      operationFailure("Level navigation surface pruning", rc);
      return false;
    }
  }

  if (options.generate_anchors) {
    pistoris::Level::AnchorGenOptions generation = effectiveAnchorGenerationOptions(options);
    ArxReturnCode rc = navigation ? pistoris::level_debug::generateAnchors(level, generation, *navigation)
                                  : level.generateAnchors(generation);
    if (rc != ARX_OK) {
      operationFailure("Level anchor generation", rc);
      return false;
    }
  }

  if (options.connect_anchors) {
    pistoris::Level::AnchorGenOptions generation = effectiveAnchorGenerationOptions(options);
    pistoris::Level::AnchorConnectionGenOptions connection = effectiveAnchorConnectionOptions(options, generation);
    ArxReturnCode rc = navigation ? pistoris::level_debug::generateAnchorConnections(level, connection, *navigation)
                                  : level.generateAnchorConnections(connection);
    if (rc != ARX_OK) {
      operationFailure("Level anchor link generation", rc);
      return false;
    }
  }

  if (options.prune_anchor_islands) {
    ArxReturnCode rc = navigation
                           ? pistoris::level_debug::pruneAnchorIslands(level, options.anchor_pruning, *navigation)
                           : level.pruneAnchorIslands(options.anchor_pruning);
    if (rc != ARX_OK) {
      operationFailure("Level anchor pruning", rc);
      return false;
    }
  }

  if (options.generate_room_distances) {
    ArxReturnCode rc = room_distances ? pistoris::level_debug::generateRoomDistances(
                                            level, options.room_distance_generation, *room_distances)
                                      : level.generateRoomDistances(options.room_distance_generation);
    if (rc != ARX_OK) {
      operationFailure("Level room-distance generation", rc);
      return false;
    }
  }

  if (options.generate_static_lighting) {
    ArxReturnCode rc = level.generateStaticLighting(options.static_lighting_generation);
    if (rc != ARX_OK) {
      operationFailure("Level static lighting generation", rc);
      return false;
    }
  }

  if (options.generate_minimap) {
    ArxReturnCode rc = level.generateMinimap(options.minimap_generation);
    if (rc != ARX_OK) {
      operationFailure("Level minimap generation", rc);
      return false;
    }
  }
  return true;
}

}  // namespace cli::level::operations
