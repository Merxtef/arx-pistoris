// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>
#include <string>

namespace cli {

enum class PathAddress : std::uint8_t {
  kMountRelative,
  kAbsolute,
};

struct PathLocation {
  std::string path;
  PathAddress address = PathAddress::kMountRelative;
};

using OutputAddress = PathAddress;
using OutputLocation = PathLocation;

}  // namespace cli
