// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/paths/types.h"

#include "formats/format.h"
#include "io/path_location.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace cli {

enum class SelectorParseStatus : std::uint8_t {
  kNotSelector,
  kValid,
  kInvalid,
};

struct ResourceSelector {
  ArxResourceKind kind = ARX_RESOURCE_KIND_NONE;
  std::string logical_path;
  std::string type;
  std::string name;
  std::string tweak;
  std::uint32_t level = 0;
};

struct OutputTarget : OutputLocation {
  ResourceSelector selector;
  Format format = Format::kUnknown;
};

SelectorParseStatus parseResourceSelector(std::string_view argument, ResourceSelector& out, std::string& error);
class IoService;

bool resolveOutputTarget(const char* argument, const IoService& io, OutputTarget& out);

std::string resourceParentPath(std::string_view path);
std::string resourceStem(std::string_view path, bool strip_any_extension = true);

}  // namespace cli
