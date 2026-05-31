// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "text_cursor.h"

#include <cstdint>
#include <string_view>

namespace pistoris {

TextCursor::TextCursor(std::string_view text, std::string_view special_chars) noexcept
    : text_(text), special_chars_(special_chars) {}

Token TextCursor::next() noexcept {
  while (true) {
    while (pos_ < text_.size()) {
      char c = text_[pos_];
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
        if (c == '\n') {
          line_++;
          col_ = 1;
        } else {
          col_++;
        }
        pos_++;
      } else {
        break;
      }
    }

    if (pos_ >= text_.size()) return {{}, TokenKind::kEnd, line_, col_};

    char c = text_[pos_];

    if (special_chars_.find(c) != std::string_view::npos) {
      Token tok{text_.substr(pos_, 1), TokenKind::kSpecialChar, line_, col_};
      pos_++;
      col_++;
      return tok;
    }

    auto start     = pos_;
    auto start_col = col_;
    while (pos_ < text_.size()) {
      char ch = text_[pos_];
      if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' || special_chars_.find(ch) != std::string_view::npos)
        break;
      pos_++;
      col_++;
    }
    return {text_.substr(start, pos_ - start), TokenKind::kString, line_, start_col};
  }
}

Token TextCursor::restOfLine() noexcept {
  while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t')) {
    pos_++;
    col_++;
  }

  if (pos_ >= text_.size()) return {{}, TokenKind::kEnd, line_, col_};

  auto start     = pos_;
  auto start_col = col_;

  auto end = pos_;
  while (end < text_.size() && text_[end] != '\r' && text_[end] != '\n') end++;

  auto rend = end;
  while (rend > start && (text_[rend - 1] == ' ' || text_[rend - 1] == '\t')) rend--;

  pos_ = end;
  col_ += static_cast<std::uint32_t>(end - start);

  if (rend == start) return {{}, TokenKind::kEnd, line_, start_col};
  return {text_.substr(start, rend - start), TokenKind::kString, line_, start_col};
}

}  // namespace pistoris
