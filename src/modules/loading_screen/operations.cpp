// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "modules/loading_screen.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace pistoris::loading_screen {

void setImage(LoadingScreenData& loading_screen, std::vector<std::uint8_t> encoded) noexcept {
  loading_screen.encoded_image = std::move(encoded);
}

void clear(LoadingScreenData& loading_screen) noexcept { loading_screen = {}; }

}  // namespace pistoris::loading_screen
