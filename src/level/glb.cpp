// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "external/glb/level/api.h"
#include "level/data.h"

#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace pistoris {

ArxReturnCode Level::fromGlb(Level& out, std::span<const std::uint8_t> data) {
  return fromGlb(out, data, GlbImportOptions{});
}

ArxReturnCode Level::fromGlb(Level& out, std::span<const std::uint8_t> data, const GlbImportOptions& options,
                             GlbImportInfo* info) {
  Level tmp;
  GlbImportInfo import_info;
  ArxReturnCode rc =
      importLevelFromGlb(data, static_cast<LevelModules&>(*tmp.data_), options, &tmp.data_->validation, &import_info);
  if (rc != ARX_OK) return rc;
  out.swap(tmp);
  if (info) *info = import_info;
  return ARX_OK;
}

ArxReturnCode Level::exportGlb(std::vector<std::uint8_t>& out) const { return exportGlb(out, GlbExportOptions{}); }

ArxReturnCode Level::exportGlb(std::vector<std::uint8_t>& out, const GlbExportOptions& options) const {
  ArxReturnCode rc = validate();
  if (rc != ARX_OK) return rc;
  if (!data_->validation.derived.referenced_bounds) return ARX_LEVEL_NO_GEOMETRY;
  std::vector<std::uint8_t> encoded;
  rc = exportLevelToGlb(
      static_cast<const LevelModules&>(*data_), *data_->validation.derived.referenced_bounds, options, encoded);
  if (rc == ARX_OK) out = std::move(encoded);
  return rc;
}

}  // namespace pistoris
