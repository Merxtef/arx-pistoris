// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/result.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace pistoris {

enum class GlbElement : std::uint8_t {
  kDocument,
  kScene,
  kNode,
  kMesh,
  kPrimitive,
  kAccessor,
  kMaterial,
  kTexture,
  kImage,
  kSkin,
  kAnimation,
  kChannel,
  kSampler,
};

struct GlbLocation {
  GlbElement element = GlbElement::kDocument;
  std::size_t index = kNoElementIndex;
  std::size_t subindex = kNoElementIndex;
  std::string label;
  std::string property;
};

template <class T>
using GlbResult = Result<T, GlbLocation>;

}  // namespace pistoris
