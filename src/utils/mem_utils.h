// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstddef>
#include <new>

namespace pistoris {

template <class ResizableContainer>
[[nodiscard]] bool tryResize(ResizableContainer& container, std::size_t new_size) noexcept {
  try {
    container.resize(new_size);
    return true;
  } catch (const std::bad_alloc&) {
    return false;
  }
}

}  // namespace pistoris
