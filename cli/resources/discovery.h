// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <cstdint>

namespace cli {

class IoService;

enum class ResourceListingKind : std::uint8_t {
  kNone,
  kLevel,
  kModel,
  kAnimation,
  kCinematic,
  kAmbiance,
  kAll,
};

bool printResourceListing(ResourceListingKind kind, IoService& io);

}  // namespace cli
