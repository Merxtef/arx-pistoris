// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/texture.h"
#include "arx_pistoris/texture.hpp"

#include "api/c/internal.h"

#include <vector>

struct arx_pistoris_native_texture_files {
  std::vector<pistoris::NativeTextureFile> value;
};

struct arx_pistoris_texture_source_paths {
  std::vector<std::string> value;
};

namespace pistoris::c_api {

inline bool valid(const ArxTextureView& value) noexcept {
  return valid(value.path) && valid(value.encoded_image) && valid(value.external_image_extension);
}

}  // namespace pistoris::c_api
