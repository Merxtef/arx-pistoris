// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/text_encoding.h"

#include "utils/utf8.h"

#include <cstddef>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris::text_encoding {

Encoding classify(std::string_view text) noexcept {
  bool ascii = true;
  for (unsigned char value : text) {
    if (value > 0x7fU) {
      ascii = false;
      break;
    }
  }
  if (ascii) return Encoding::kAscii;
  return utf8::valid(text) ? Encoding::kUtf8 : Encoding::kLatin1;
}

void latin1ToUtf8(std::string_view input, std::string& out) {
  std::size_t high_bytes = 0;
  for (unsigned char value : input)
    if (value >= 0x80U) ++high_bytes;

  std::string result;
  if (input.size() > result.max_size() || high_bytes > result.max_size() - input.size()) throw std::bad_alloc();
  result.reserve(input.size() + high_bytes);
  for (unsigned char value : input) {
    if (value < 0x80U) {
      result.push_back(static_cast<char>(value));
    } else {
      result.push_back(static_cast<char>(0xc0U | (value >> 6U)));
      result.push_back(static_cast<char>(0x80U | (value & 0x3fU)));
    }
  }
  out = std::move(result);
}

Error utf8ToLatin1(std::string_view input, std::string& out) {
  if (!utf8::valid(input)) return Error::kInvalidUtf8;

  std::string result;
  result.reserve(input.size());
  for (std::size_t index = 0; index < input.size();) {
    const auto first = static_cast<unsigned char>(input[index++]);
    if (first < 0x80U) {
      result.push_back(static_cast<char>(first));
      continue;
    }
    if (first < 0xc2U || first > 0xc3U) return Error::kNotLatin1;
    const auto second = static_cast<unsigned char>(input[index++]);
    result.push_back(static_cast<char>(((first & 0x1fU) << 6U) | (second & 0x3fU)));
  }
  out = std::move(result);
  return Error::kNone;
}

}  // namespace pistoris::text_encoding
