// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/model.hpp"

#include "formats/modifiers.h"
#include "routes/options.h"

#include <optional>
#include <string>

namespace cli::model {

struct ModelOptions final : RouteOptions {
  pistoris::Model::GlbImportOptions glb_import;
  pistoris::Model::GlbExportOptions glb_export;
  pistoris::Model::LevelPreviewGlbOptions level_preview_glb;
  pistoris::Model::ReferenceOptions reference;
  pistoris::Model::InventoryIconSetOptions inventory_icon_set;
  pistoris::Model::InventoryIconRenderOptions inventory_icon_render;
  std::optional<std::string> preview_class_path;
  const char* reference_ftl = nullptr;
  const char* input_icon = nullptr;
  bool infer_bone_selections = false;
};

inline void applyFormatModifiers(ModelOptions& options, const FormatModifierOptions& modifiers) {
  if (modifiers.glb.arx_units_per_unit) {
    options.glb_import.arx_units_per_glb_unit = *modifiers.glb.arx_units_per_unit;
    options.glb_export.arx_units_per_glb_unit = *modifiers.glb.arx_units_per_unit;
  }
}

}  // namespace cli::model
