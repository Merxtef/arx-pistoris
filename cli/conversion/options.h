// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

namespace cli {

struct SharedConversionOptions {
  float rotate[3] = {0.0f, 0.0f, 0.0f};  // Euler XYZ degrees
  float scale[3] = {1.0f, 1.0f, 1.0f};
  float offset[3] = {0.0f, 0.0f, 0.0f};
  bool has_xform = false;
};

}  // namespace cli
