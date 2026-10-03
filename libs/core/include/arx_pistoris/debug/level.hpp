// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#ifndef ARX_PISTORIS_ENABLE_LEVEL_DEBUG_API
#error "Define ARX_PISTORIS_ENABLE_LEVEL_DEBUG_API to use the volatile Level debug API"
#endif

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/debug/level/diagnostics.hpp"
#include "arx_pistoris/level.hpp"

#include <cstdint>
#include <vector>

namespace pistoris::level_debug {

// Volatile Level tooling surface, not a stable interchange contract

// --- Diagnostic generation ---

[[nodiscard]] ArxReturnCode generateNavSurface(Level& level, NavigationDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode generateNavSurface(Level& level, const Level::NavSurfaceGenOptions& options,
                                               NavigationDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode setNavSurfaceFromFloor(Level& level, NavigationDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode setNavSurfaceFromFloor(Level& level, const Level::NavSurfaceSourceOptions& options,
                                                   NavigationDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode pruneNavSurfaceIslands(Level& level, NavigationDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode pruneNavSurfaceIslands(Level& level, const Level::NavSurfacePruneOptions& options,
                                                   NavigationDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode generateAnchors(Level& level, NavigationDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode generateAnchors(Level& level, const Level::AnchorGenOptions& options,
                                            NavigationDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode generateAnchorConnections(Level& level, NavigationDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode generateAnchorConnections(Level& level, const Level::AnchorConnectionGenOptions& options,
                                                      NavigationDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode pruneAnchorIslands(Level& level, NavigationDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode pruneAnchorIslands(Level& level, const Level::AnchorPruneOptions& options,
                                               NavigationDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode generateRoomDistances(Level& level, RoomDistanceGenDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode generateRoomDistances(Level& level, const Level::RoomDistanceGenOptions& options,
                                                  RoomDistanceGenDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode generateStaticLighting(Level& level, StaticLightingDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode generateStaticLighting(Level& level, const Level::StaticLightingGenOptions& options,
                                                   StaticLightingDiagnostics& diagnostics) noexcept;
[[nodiscard]] ArxReturnCode generateMinimap(Level& level, const Level::MinimapGenerationOptions& options,
                                            MinimapGenerationDiagnostics& diagnostics) noexcept;

// --- Debug export ---

[[nodiscard]] ArxReturnCode exportNavigationDebugGlb(const Level& level, std::vector<std::uint8_t>& out,
                                                     const NavigationDiagnostics* diagnostics = nullptr,
                                                     const Level::GlbExportOptions& options = {}) noexcept;
[[nodiscard]] ArxReturnCode exportRoomDistanceDebugGlb(const Level& level, std::vector<std::uint8_t>& out,
                                                       const RoomDistanceGenDiagnostics* diagnostics = nullptr,
                                                       const Level::GlbExportOptions& options = {}) noexcept;
[[nodiscard]] ArxReturnCode exportRenderSplitsDebugGlb(const Level& level, std::vector<std::uint8_t>& out,
                                                       float normal_weld_degrees = 0.0f,
                                                       const Level::GlbExportOptions& options = {}) noexcept;
[[nodiscard]] ArxReturnCode exportFtsCellsDebugGlb(const fts::Data& fts, std::vector<std::uint8_t>& out,
                                                   const Level::GlbExportOptions& options = {}) noexcept;
[[nodiscard]] ArxReturnCode exportFtsRoomsDebugGlb(const fts::Data& fts, std::vector<std::uint8_t>& out) noexcept;

}  // namespace pistoris::level_debug
