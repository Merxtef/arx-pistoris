// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>

namespace cli {

enum class ConversionPath : std::uint8_t {
  kNative,
  kIntermediate,
};

inline ConversionPath selectConversionPath(bool native_input, bool native_output, bool requires_intermediate) noexcept {
  return native_input && native_output && !requires_intermediate ? ConversionPath::kNative
                                                                 : ConversionPath::kIntermediate;
}

}  // namespace cli
