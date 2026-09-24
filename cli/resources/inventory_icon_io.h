// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/model.hpp"

#include "io/path_location.h"
#include "io/service.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cli {

class IoService;
class ResourceOutputPlan;
struct ClassifiedPath;
struct OutputTarget;
using ResourceAssetId = std::size_t;

struct InventoryIconInput {
  bool enabled = false;
  bool required = false;
  bool exact = false;
  PathLocation location;
  PathLocation base;
  std::string path;
  ImageLookupMode lookup = ImageLookupMode::kGamePriority;
};

struct InventoryIconOutput {
  bool enabled = false;
  bool item_only = false;
  bool bmp_companion = false;
  PathLocation stem;
};

struct LoadedInventoryIcon {
  std::vector<std::uint8_t> encoded;
  std::string resolved_path;
};

bool resolveInventoryIconInput(const ClassifiedPath& input, std::string_view explicit_path, IoService& io,
                               InventoryIconInput& out);
bool resolveInventoryIconOutput(const OutputTarget& output, IoService& io, InventoryIconOutput& out);
bool readInventoryIcon(IoService& io, const InventoryIconInput& input, LoadedInventoryIcon& out);
bool projectInventoryIcon(const pistoris::Model& model, const pistoris::Model::InventoryIconRenderOptions& options,
                          std::vector<std::uint8_t>& out);
bool projectInventoryIconBmp(const pistoris::Model& model, const pistoris::Model::InventoryIconRenderOptions& options,
                             std::vector<std::uint8_t>& out);
bool addInventoryIconOutput(ResourceOutputPlan& plan, const InventoryIconOutput& output,
                            std::span<const std::uint8_t> encoded, ArxImageFormat format, ResourceAssetId asset);
bool addInventoryIconOutput(ResourceOutputPlan& plan, const InventoryIconOutput& output, const pistoris::Model& model,
                            const pistoris::Model::InventoryIconRenderOptions& options,
                            std::vector<std::uint8_t>& rendered, ResourceAssetId asset);

}  // namespace cli
