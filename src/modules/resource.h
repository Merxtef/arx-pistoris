// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/paths/types.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace pistoris {

struct ResourceData {
  std::string path;
};

namespace resource {

enum class Error : std::uint8_t {
  kNone,
  kBadKind,
  kBadPath,
};

// --- Validation ---

Error validate(const ResourceData& resource, ArxResourceKind kind);

// --- Mutation ---

void setPath(ResourceData& resource, std::string path) noexcept;

// --- Repair ---

Error repairPath(ArxResourceKind kind, std::string_view input, std::string& out);

}  // namespace resource
}  // namespace pistoris
