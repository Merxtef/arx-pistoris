// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/paths/types.h"

#include "formats/format.h"
#include "resources/layout.h"
#include "resources/selector.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cli {

constexpr std::size_t kNoClassifiedPath = static_cast<std::size_t>(-1);

enum class PayloadKind : std::uint8_t {
  kUnknown,
  kGlb,
  kFtl,
  kTea,
  kFts,
  kLlf,
  kDlf,
  kAmb,
  kObj,
};

struct FileFacts {
  Format format = Format::kUnknown;
  PayloadKind kind = PayloadKind::kUnknown;
};

struct ClassifiedPath {
  std::string path;
  PathLocation location;
  std::vector<std::uint8_t> buffer;
  FileFacts facts{};
  std::size_t positional_index = 0;
  ArxResourceKind resource_kind = ARX_RESOURCE_KIND_NONE;
  ResourceLayout layout = ResourceLayout::kLoose;
};

FileFacts classifyInput(std::span<const std::uint8_t> buffer, std::string_view path);

}  // namespace cli
