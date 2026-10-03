// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstddef>
#include <new>
#include <stdexcept>

namespace pistoris {

template <class ResizableContainer>
[[nodiscard]] bool tryResize(ResizableContainer& container, std::size_t new_size) noexcept {
  try {
    container.resize(new_size);
    return true;
  } catch (const std::bad_alloc&) {
    return false;
  } catch (const std::length_error&) {
    return false;
  } catch (...) {
    return false;
  }
}

template <class ReservableContainer>
[[nodiscard]] bool tryReserve(ReservableContainer& container, std::size_t new_capacity) noexcept {
  try {
    container.reserve(new_capacity);
    return true;
  } catch (const std::bad_alloc&) {
    return false;
  } catch (const std::length_error&) {
    return false;
  } catch (...) {
    return false;
  }
}

}  // namespace pistoris
