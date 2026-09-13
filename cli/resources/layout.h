// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "formats/format.h"
#include "io/path_location.h"

#include <cstdint>

namespace cli {

enum class ResourceLayout : std::uint8_t {
  kLoose,
  kGame,
};

constexpr ResourceLayout primaryResourceLayout(Format format, PathAddress address) noexcept {
  if (address == PathAddress::kAbsolute) return ResourceLayout::kLoose;
  switch (format) {
    case Format::kFtl:
    case Format::kTea:
    case Format::kDlf:
    case Format::kAmb:
      return ResourceLayout::kGame;
    default:
      return ResourceLayout::kLoose;
  }
}

}  // namespace cli
