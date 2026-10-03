// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/text.h"
#include "arx_pistoris/native/text.hpp"

namespace pistoris::c_api {

inline bool validNativeTextMode(ArxNativeTextMode mode) noexcept {
  return mode == ARX_NATIVE_TEXT_AUTO || mode == ARX_NATIVE_TEXT_UTF8 || mode == ARX_NATIVE_TEXT_LATIN1;
}

inline NativeTextMode nativeTextMode(ArxNativeTextMode mode) noexcept { return static_cast<NativeTextMode>(mode); }

}  // namespace pistoris::c_api
