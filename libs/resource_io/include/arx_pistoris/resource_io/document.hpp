// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/resource_io/resource_mounts.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris::resource_io {

enum class ResourceAddress : std::uint8_t { kLogical, kNative };
enum class ResourceLayout : std::uint8_t { kLoose, kGame };

enum class ResourceFormat : std::uint8_t {
  kUnknown,
  kFtl,
  kTea,
  kFts,
  kDlf,
  kLlf,
  kAmb,
  kCin,
  kObj,
  kMtl,
  kJson,
  kGlb,
};

enum class ResourcePayload : std::uint8_t {
  kUnknown,
  kModel,
  kAnimation,
  kLevelGeometry,
  kLevelScene,
  kLevelLighting,
  kAmbiance,
  kCinematic,
  kObj,
  kGlb,
};

struct ResourceClassification {
  ResourceFormat format = ResourceFormat::kUnknown;
  ResourcePayload payload = ResourcePayload::kUnknown;
};

struct ResourceDocument {
  std::string requested_path;
  std::string logical_path;
  std::filesystem::path native_path;
  std::vector<std::uint8_t> data;
  ResourceClassification classification;
  ResourceAddress address = ResourceAddress::kNative;
  ResourceLayout layout = ResourceLayout::kLoose;
  ResourceMountMask mount_mask = 0;
  ResourceMountMask mount_id = 0;
  ResourceIoFlags flags = kResourceIoFlagNone;
  ArxResourceKind selector_kind = ARX_RESOURCE_KIND_NONE;
};

[[nodiscard]] ResourceIoResult<ResourceClassification> classifyResource(std::span<const std::uint8_t> data,
                                                                        std::string_view path) noexcept;
[[nodiscard]] const char* resourceFormatName(ResourceFormat format) noexcept;

}  // namespace pistoris::resource_io
