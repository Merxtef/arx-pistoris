// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "state.h"

#include <cstddef>
#include <limits>
#include <vector>

enum class BoneOriginReferenceMode {
  kNone,
  kSnapOrigins,
  kDeltaDeform,
  kHierarchyDeform,
};

struct CliArgs {
  std::vector<const char*> positionals;
  bool pretty = false;

  // Transform
  float rotate[3] = {0.0f, 0.0f, 0.0f};  // Euler XYZ degrees
  float scale[3]  = {1.0f, 1.0f, 1.0f};
  float offset[3] = {0.0f, 0.0f, 0.0f};
  bool has_xform  = false;

  const char* overwrite_texture = nullptr;
  const char* rename_selections = nullptr;
  const char* reference_ftl     = nullptr;
  bool autosize_to_reference    = false;
  BoneOriginReferenceMode bone_origin_reference_mode = BoneOriginReferenceMode::kNone;
  std::size_t hierarchy_deform_step_limit = std::numeric_limits<std::size_t>::max();
  bool snap_action_points       = false;
  bool copy_reference_affiliations = false;
  cli::OverwriteMode overwrite  = cli::OverwriteMode::kAsk;
};
