// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/ambiance/operations.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"

#include "console/diagnostics.h"
#include "console/logging.h"
#include "routes/ambiance/options.h"

#include <cstddef>

namespace cli::ambiance::operations {
namespace {

bool operationFailure(const char* what, ArxReturnCode rc) {
  diagnostic(DiagnosticCode::kAmbianceModuleFailed,
             "%s failed: %s (code %d)",
             what,
             pistoris::errorString(rc),
             static_cast<int>(rc));
  return false;
}

}  // namespace

bool apply(pistoris::Ambiance& ambiance, const AmbianceOptions& options) {
  if (!options.trim_tracks_to_master) return true;

  std::size_t trimmed_tracks = 0;
  const ArxReturnCode rc = ambiance.trimTracksToMaster(&trimmed_tracks);
  if (rc != ARX_OK) return operationFailure("Ambiance track trimming", rc);
  if (trimmed_tracks != 0)
    log(ARX_LOG_INFO, "trimmed %zu non-master Ambiance track(s) to the master duration", trimmed_tracks);
  return true;
}

}  // namespace cli::ambiance::operations
