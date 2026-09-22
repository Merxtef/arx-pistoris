// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/text.hpp"

#include "formats/format.h"

namespace cli {

inline pistoris::NativeTextMode carrierTextMode(Format format, pistoris::NativeTextMode native_mode) noexcept {
  return format == Format::kJson ? pistoris::NativeTextMode::kUtf8 : native_mode;
}

inline pistoris::NativeTextMode directCarrierTextMode(Format input, Format output,
                                                      pistoris::NativeTextMode native_mode) noexcept {
  return carrierTextMode(input == Format::kJson ? output : input, native_mode);
}

}  // namespace cli
