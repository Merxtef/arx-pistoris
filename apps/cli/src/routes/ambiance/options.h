// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ambiance.hpp"

#include "formats/modifiers.h"
#include "routes/options.h"

#include <optional>
#include <string>

namespace cli::ambiance {

struct AmbianceOptions final : RouteOptions {
  pistoris::Ambiance::GlbImportOptions glb_import;
  pistoris::Ambiance::GlbExportOptions glb_export;
  bool trim_tracks_to_master = false;
  std::optional<std::string> reference_model;
};

inline void applyFormatModifiers(AmbianceOptions& options, const FormatModifierOptions& modifiers) {
  if (!modifiers.glb.arx_units_per_unit) return;
  options.glb_import.arx_units_per_glb_unit = *modifiers.glb.arx_units_per_unit;
  options.glb_export.arx_units_per_glb_unit = *modifiers.glb.arx_units_per_unit;
}

}  // namespace cli::ambiance
