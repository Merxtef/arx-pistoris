// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>

namespace cli {

enum class OverwriteMode : std::uint8_t {
  kAsk,
  kAlwaysYes,
  kAlwaysNo,
};

struct State {
  OverwriteMode overwrite = OverwriteMode::kAsk;
};

}  // namespace cli
