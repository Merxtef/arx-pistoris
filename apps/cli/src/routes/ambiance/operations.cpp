// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "routes/ambiance/operations.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/runtime/types.h"

#include "console/diagnostics.h"
#include "console/logging.h"
#include "routes/ambiance/options.h"
#include "routes/conversion_failure.h"

namespace cli::ambiance::operations {
namespace {

template <class Result>
bool operationFailure(const char* what, const Result& result) {
  return conversionStageFailure(DiagnosticCode::kAmbianceModuleFailed, what, result);
}

}  // namespace

bool apply(pistoris::Ambiance& ambiance, const AmbianceOptions& options) {
  if (!options.trim_tracks_to_master) return true;

  const auto trimmed_tracks = ambiance.trimTracksToMaster();
  if (!trimmed_tracks) return operationFailure("Ambiance track trimming", trimmed_tracks);
  if (*trimmed_tracks != 0)
    log(ARX_LOG_INFO, "trimmed %zu non-master Ambiance track(s) to the master duration", *trimmed_tracks);
  return true;
}

}  // namespace cli::ambiance::operations
