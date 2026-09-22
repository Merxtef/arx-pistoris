// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>

namespace pistoris {

// Controls text at native carrier boundaries; semantic and JSON text are always UTF-8.
// Auto decodes valid UTF-8 and falls back to Latin-1, but emits UTF-8.
enum class NativeTextMode : std::uint8_t {
  kAuto,
  kUtf8,
  kLatin1,
};

}  // namespace pistoris
