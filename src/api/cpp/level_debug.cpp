// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/native/fts.hpp"

#include "api/status_boundary.h"
#include "external/glb/level/api.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace pistoris::level_debug {

ArxReturnCode exportFtsCellsDebugGlb(const fts::Data& fts, std::vector<std::uint8_t>& out,
                                     const Level::GlbExportOptions& options) noexcept {
  return api_detail::statusBoundary([&] {
    std::vector<std::uint8_t> result;
    ArxReturnCode rc = buildFtsCellsDebugGlb(fts, result, options);
    if (rc == ARX_OK) out = std::move(result);
    return rc;
  });
}

ArxReturnCode exportFtsRoomsDebugGlb(const fts::Data& fts, std::vector<std::uint8_t>& out) noexcept {
  return api_detail::statusBoundary([&] {
    std::vector<std::uint8_t> result;
    ArxReturnCode rc = buildFtsRoomsDebugGlb(fts, result);
    if (rc == ARX_OK) out = std::move(result);
    return rc;
  });
}

}  // namespace pistoris::level_debug
