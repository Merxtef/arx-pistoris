// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/texture.hpp"

#include <vector>

namespace pistoris {

struct NativeModelBundle {
  ftl::Data ftl;
  std::vector<NativeTextureFile> texture_files;
};

}  // namespace pistoris
