// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <vector>

namespace cli {

enum class Format : std::uint8_t {
  kUnset,
  kUnknown,
  kFtl,
  kTea,
  kObj,
  kJson,
  kGlb,
};

enum class RouteKind : std::uint8_t {
  kUnknown,
  kModel,
  kAnimation,
};

struct Route {
  RouteKind kind = RouteKind::kUnknown;
  Format input   = Format::kUnknown;
  Format output  = Format::kUnset;  // Unset = inspect
};

const char* formatName(Format fmt);
Format formatFromPath(const char* path);
Route detectRoute(const std::vector<std::uint8_t>& buf, const char* path);

}  // namespace cli
