// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "formats/format.h"

#include <cstdint>

namespace cli {

enum class RouteKind : std::uint8_t {
  kUnknown,
  kModel,
  kAnimation,
  kLevel,
};

enum class ProbeStatus : std::uint8_t {
  kNoMatch,
  kMatch,
  kInvalid,
};

struct ProbeResult {
  ProbeStatus status = ProbeStatus::kNoMatch;
  const char* message = nullptr;
};

struct Route {
  RouteKind kind = RouteKind::kUnknown;
  Format input = Format::kUnknown;
  Format output = Format::kUnknown;
};

}  // namespace cli
