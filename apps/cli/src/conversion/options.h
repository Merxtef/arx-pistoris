// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <string>

namespace cli {

struct DirectoryRebaseRequest {
  bool requested = false;
  std::string directory;
};

struct SharedConversionOptions {
  float rotate[3] = {0.0f, 0.0f, 0.0f};  // Euler XYZ degrees
  float scale = 1.0f;
  float offset[3] = {0.0f, 0.0f, 0.0f};
  bool has_xform = false;
  DirectoryRebaseRequest textures;
  DirectoryRebaseRequest sounds;
};

}  // namespace cli
