// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "io/native_text.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/binary.hpp"
#include "arx_pistoris/native/text.hpp"

#include <string>
#include <string_view>

namespace cli::io_detail {

ArxReturnCode nativeTextToUtf8(std::string_view raw, pistoris::NativeTextMode mode, std::string& out) {
  out.clear();
  if (raw.find('\0') != std::string_view::npos) return ARX_INVALID_OPTIONS;

  if (mode == pistoris::NativeTextMode::kLatin1) return pistoris::binary::latin1ToUtf8(raw, out);
  if (mode != pistoris::NativeTextMode::kAuto && mode != pistoris::NativeTextMode::kUtf8) return ARX_INVALID_OPTIONS;

  if (pistoris::binary::classifyTextEncoding(raw) == pistoris::binary::TextEncoding::kLatin1) {
    if (mode == pistoris::NativeTextMode::kUtf8) return ARX_TEXT_INVALID_UTF8;
    return pistoris::binary::latin1ToUtf8(raw, out);
  }

  out.assign(raw);
  return ARX_OK;
}

}  // namespace cli::io_detail
