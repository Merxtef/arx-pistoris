// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstddef>
#include <limits>
#include <string>

namespace pistoris {

inline constexpr std::size_t kNoInputIndex = std::numeric_limits<std::size_t>::max();
inline constexpr std::size_t kNoElementIndex = std::numeric_limits<std::size_t>::max();

template <class ElementKind>
struct ResourceLocation {
  std::string resource_path;
  std::size_t input_index = kNoInputIndex;
  ElementKind element = {};
  std::size_t index = kNoElementIndex;
  std::size_t subindex = kNoElementIndex;
  std::string label;
};

}  // namespace pistoris
