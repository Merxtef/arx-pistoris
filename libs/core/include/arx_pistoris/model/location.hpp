// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/animation/location.hpp"
#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/result.hpp"

#include <cstdint>
#include <variant>

namespace pistoris {

enum class ModelElement : std::uint8_t {
  kResource,
  kInventoryIcon,
  kVertex,
  kFace,
  kTexture,
  kBone,
  kActionPoint,
  kSelection,
};

using ModelLocation = ResourceLocation<ModelElement>;

template <class T>
using ModelResult = Result<T, ModelLocation>;

using ModelGlbExportLocation = std::variant<ModelLocation, AnimationLocation>;

template <class T>
using ModelGlbExportResult = Result<T, ModelGlbExportLocation>;

}  // namespace pistoris
