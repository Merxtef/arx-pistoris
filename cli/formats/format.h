// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace cli {

enum class Format : std::uint8_t {
  kUnset,
  kUnknown,
  kFtl,
  kFts,
  kDlf,
  kLlf,
  kTea,
  kAmb,
  kObj,
  kJson,
  kGlb,
};

const char* formatName(Format format);
Format formatFromExtension(std::string_view extension) noexcept;
Format formatFromPath(std::string_view path) noexcept;
std::string resourceFormatStem(std::string_view path);

}  // namespace cli
