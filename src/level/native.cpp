// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"

#include "api/status_boundary.h"
#include "level/data.h"
#include "level/native/api.h"

#include <string>
#include <utility>
#include <vector>

namespace pistoris {

ArxReturnCode Level::importNative(Level& out, const fts::Data& fts, const llf::Data* llf, const dlf::Data* dlf,
                                  std::vector<std::string>* texture_source_paths) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    Level tmp;
    std::vector<std::string> source_paths;
    ArxReturnCode rc = level_native::buildLevel({fts, llf, dlf},
                                                static_cast<LevelModules&>(*tmp.data_),
                                                &tmp.data_->validation,
                                                texture_source_paths ? &source_paths : nullptr);
    if (rc != ARX_OK) return rc;
    out.swap(tmp);
    if (texture_source_paths) *texture_source_paths = std::move(source_paths);
    return ARX_OK;
  });
}

ArxReturnCode Level::bakeNativeBundle(const NativeBakeOptions& options, NativeLevelBundle& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = validate();
    if (rc != ARX_OK) return rc;
    return level_native::bakeValidatedNativeLevelBundle(static_cast<const LevelModules&>(*data_), options, out);
  });
}

ArxReturnCode Level::bakeDlf(const DlfBakeOptions& options, dlf::Data& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ArxReturnCode rc = validatePlayerSpawn();
    if (rc != ARX_OK) return rc;
    rc = validateEntities();
    if (rc != ARX_OK) return rc;
    rc = validateFogs();
    if (rc != ARX_OK) return rc;
    rc = validateZones();
    if (rc != ARX_OK) return rc;
    rc = validatePaths();
    if (rc != ARX_OK) return rc;
    return level_native::bakeValidatedNativeDlf(data_->scene, options, out);
  });
}

}  // namespace pistoris
