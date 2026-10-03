// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/native_text.h"

#include "arx_pistoris/native/text.hpp"

#include "utils/text_encoding.h"

#include <string>
#include <string_view>

namespace pistoris::native_text {

bool validMode(NativeTextMode mode) noexcept {
  return mode == NativeTextMode::kAuto || mode == NativeTextMode::kUtf8 || mode == NativeTextMode::kLatin1;
}

bool decode(std::string_view raw, NativeTextMode mode, std::string& out) {
  if (!validMode(mode) || raw.find('\0') != std::string_view::npos) return false;
  if (mode == NativeTextMode::kLatin1) {
    text_encoding::latin1ToUtf8(raw, out);
    return true;
  }
  const text_encoding::Encoding encoding = text_encoding::classify(raw);
  if (encoding == text_encoding::Encoding::kLatin1) {
    if (mode == NativeTextMode::kUtf8) return false;
    text_encoding::latin1ToUtf8(raw, out);
    return true;
  }
  out.assign(raw);
  return true;
}

bool encode(std::string_view text, NativeTextMode mode, std::string& out) {
  if (!validMode(mode) || text.find('\0') != std::string_view::npos) return false;
  if (mode == NativeTextMode::kLatin1) return text_encoding::utf8ToLatin1(text, out) == text_encoding::Error::kNone;
  if (text_encoding::classify(text) == text_encoding::Encoding::kLatin1) return false;
  out.assign(text);
  return true;
}

std::string diagnostic(std::string_view raw) {
  constexpr char kHex[] = "0123456789abcdef";
  std::string result;
  result.reserve(raw.size());
  for (const unsigned char value : raw) {
    if (value >= 0x20U && value <= 0x7eU && value != '\\' && value != '\'') {
      result.push_back(static_cast<char>(value));
      continue;
    }
    if (value == '\\' || value == '\'') {
      result.push_back('\\');
      result.push_back(static_cast<char>(value));
      continue;
    }
    result.append("\\x");
    result.push_back(kHex[value >> 4U]);
    result.push_back(kHex[value & 0x0fU]);
  }
  return result;
}

}  // namespace pistoris::native_text
