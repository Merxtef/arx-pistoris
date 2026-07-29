// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>

namespace cli {

enum class Format : std::uint8_t {
  kUnset,
  kUnknown,
  kFtl,
  kFts,
  kDlf,
  kLlf,
  kTea,
  kObj,
  kJson,
  kGlb,
};

const char* formatName(Format format);
Format formatFromPath(const char* path);

}  // namespace cli
