// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace pistoris::text_encoding {

enum class Encoding : std::uint8_t {
  kAscii,
  kUtf8,
  kLatin1,
};

enum class Error : std::uint8_t {
  kNone,
  kInvalidUtf8,
  kNotLatin1,
};

[[nodiscard]] Encoding classify(std::string_view text) noexcept;
void latin1ToUtf8(std::string_view input, std::string& out);
[[nodiscard]] Error utf8ToLatin1(std::string_view input, std::string& out);

}  // namespace pistoris::text_encoding
