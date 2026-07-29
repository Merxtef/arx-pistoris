// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#ifndef ARX_PISTORIS_ENABLE_LEVEL_DEBUG_API
#error "Define ARX_PISTORIS_ENABLE_LEVEL_DEBUG_API to use the volatile Level debug API"
#endif

#include "arx_pistoris/debug/level_diagnostics.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

#include <vector>

namespace pistoris::level_debug {

// Volatile Level tooling surface, not a stable interchange contract

ArxReturnCode generateNavSurface(Level& level, NavigationDiagnostics& diagnostics);
ArxReturnCode generateNavSurface(Level& level, const Level::NavSurfaceGenOptions& options,
                                 NavigationDiagnostics& diagnostics);
ArxReturnCode setNavSurfaceFromFloor(Level& level, NavigationDiagnostics& diagnostics);
ArxReturnCode setNavSurfaceFromFloor(Level& level, const Level::NavSurfaceSourceOptions& options,
                                     NavigationDiagnostics& diagnostics);
ArxReturnCode pruneNavSurfaceIslands(Level& level, NavigationDiagnostics& diagnostics);
ArxReturnCode pruneNavSurfaceIslands(Level& level, const Level::NavSurfacePruneOptions& options,
                                     NavigationDiagnostics& diagnostics);
ArxReturnCode generateAnchors(Level& level, NavigationDiagnostics& diagnostics);
ArxReturnCode generateAnchors(Level& level, const Level::AnchorGenOptions& options, NavigationDiagnostics& diagnostics);
ArxReturnCode generateAnchorConnections(Level& level, NavigationDiagnostics& diagnostics);
ArxReturnCode generateAnchorConnections(Level& level, const Level::AnchorConnectionGenOptions& options,
                                        NavigationDiagnostics& diagnostics);
ArxReturnCode pruneAnchorIslands(Level& level, NavigationDiagnostics& diagnostics);
ArxReturnCode pruneAnchorIslands(Level& level, const Level::AnchorPruneOptions& options,
                                 NavigationDiagnostics& diagnostics);
ArxReturnCode generateRoomDistances(Level& level, RoomDistanceGenDiagnostics& diagnostics);
ArxReturnCode generateRoomDistances(Level& level, const Level::RoomDistanceGenOptions& options,
                                    RoomDistanceGenDiagnostics& diagnostics);
ArxReturnCode generateStaticLighting(Level& level, StaticLightingDiagnostics& diagnostics);
ArxReturnCode generateStaticLighting(Level& level, const Level::StaticLightingGenOptions& options,
                                     StaticLightingDiagnostics& diagnostics);

ArxReturnCode exportNavigationDebugGlb(const Level& level, std::vector<std::uint8_t>& out,
                                       const NavigationDiagnostics* diagnostics = nullptr,
                                       const Level::GlbExportOptions& options = {});
ArxReturnCode exportRoomDistanceDebugGlb(const Level& level, std::vector<std::uint8_t>& out,
                                         const RoomDistanceGenDiagnostics* diagnostics = nullptr,
                                         const Level::GlbExportOptions& options = {});
ArxReturnCode exportRenderSplitsDebugGlb(const Level& level, std::vector<std::uint8_t>& out,
                                         float normal_weld_degrees = 0.0f, const Level::GlbExportOptions& options = {});
ArxReturnCode exportFtsCellsDebugGlb(const fts::Data& fts, std::vector<std::uint8_t>& out,
                                     const Level::GlbExportOptions& options = {});
ArxReturnCode exportFtsRoomsDebugGlb(const fts::Data& fts, std::vector<std::uint8_t>& out);

}  // namespace pistoris::level_debug
