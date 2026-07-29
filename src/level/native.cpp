// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/level.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "arx/conversion/level/api.h"
#include "level/data.h"

namespace pistoris {

ArxReturnCode Level::fromNative(Level& out, const fts::Data& fts, const llf::Data* llf, const dlf::Data* dlf) {
  Level tmp;
  ArxReturnCode rc =
      arx_level_conversion::buildLevel({fts, llf, dlf}, static_cast<LevelModules&>(*tmp.data_), &tmp.data_->validation);
  if (rc != ARX_OK) return rc;
  out.swap(tmp);
  return ARX_OK;
}

ArxReturnCode Level::bakeNativeBundle(const NativeBakeOptions& options, NativeLevelBundle& out) const {
  ArxReturnCode rc = validate();
  if (rc != ARX_OK) return rc;
  return arx_level_conversion::bakeValidatedNativeLevelBundle(static_cast<const LevelModules&>(*data_), options, out);
}

ArxReturnCode Level::bakeNativeDlf(const NativeDlfBakeOptions& options, dlf::Data& out) const {
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
  return arx_level_conversion::bakeValidatedNativeDlf(data_->scene, options, out);
}

}  // namespace pistoris
