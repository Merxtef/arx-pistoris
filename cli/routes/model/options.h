// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "routes/options.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace cli::model {

enum class BoneOriginReferenceMode : std::uint8_t {
  kNone,
  kSnapOrigins,
  kDeltaDeform,
  kHierarchyDeform,
};

struct ModelOptions final : RouteOptions {
  const char* overwrite_texture = nullptr;
  const char* rename_selections = nullptr;
  const char* reference_ftl = nullptr;

  BoneOriginReferenceMode bone_origin_reference_mode = BoneOriginReferenceMode::kNone;
  std::size_t hierarchy_deform_step_limit = std::numeric_limits<std::size_t>::max();

  bool autosize_to_reference = false;
  bool snap_action_points = false;
  bool copy_reference_affiliations = false;
};

}  // namespace cli::model
