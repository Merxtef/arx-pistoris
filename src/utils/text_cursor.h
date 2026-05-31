// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <string_view>

namespace pistoris {

enum class TokenKind : std::uint8_t {
  kString,
  kSpecialChar,
  kEnd,
};

struct Token {
  std::string_view text;
  TokenKind kind;
  std::uint32_t line;  // 1-based
  std::uint32_t col;
};

// each char in special_chars emits its own kSpecialChar
struct TextCursor {
  TextCursor(std::string_view text, std::string_view special_chars = {}) noexcept;

  Token next() noexcept;

  // current pos to end of line, whitespace-trimmed; kEnd if blank
  Token restOfLine() noexcept;

 private:
  std::string_view text_;
  std::string_view special_chars_;
  std::size_t pos_    = 0;
  std::uint32_t line_ = 1;
  std::uint32_t col_  = 1;
};

}  // namespace pistoris
