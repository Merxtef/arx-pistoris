// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/inventory_icon_io.h"

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"

#include "base/resource_path.h"
#include "console/diagnostics.h"
#include "console/logging.h"
#include "formats/classification.h"
#include "formats/format.h"
#include "io/path_location.h"
#include "io/service.h"
#include "media/encoded.h"
#include "resources/layout.h"
#include "resources/read_diagnostics.h"
#include "resources/resource_output.h"
#include "resources/selector.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cli {
namespace {

bool siblingStem(const PathLocation& primary, std::string_view path, IoService& io, PathLocation& out) {
  PathLocation parent;
  std::string error;
  if (!io.parentPathLocation(primary, parent, error) ||
      !io.appendPathLocation(parent, resourceStem(path) + "[icon]", out, error)) {
    diagnostic(DiagnosticCode::kIoPathInvalid, "Cannot resolve Model inventory icon path: %s", error.c_str());
    return false;
  }
  return true;
}

bool modelEntityClassFromFtl(std::string_view path, std::string& out) {
  pistoris::paths::ModelPathView model;
  if (pistoris::paths::modelFromFtl(path, model)) return pistoris::paths::baseEntityClassFromModel(model, out);
  return pistoris::paths::entityClassFromFtl(path, out);
}

}  // namespace

bool resolveInventoryIconInput(const ClassifiedPath& input, std::string_view explicit_path, IoService& io,
                               InventoryIconInput& out) {
  InventoryIconInput resolved;
  std::string error;
  if (!explicit_path.empty()) {
    if (!io.resolvePathLocation(explicit_path, resolved.location, error)) {
      diagnostic(DiagnosticCode::kIoPathInvalid,
                 "Invalid inventory icon input '%.*s': %s",
                 static_cast<int>(explicit_path.size()),
                 explicit_path.data(),
                 error.c_str());
      return false;
    }
    resolved.enabled = true;
    resolved.required = true;
    resolved.exact = true;
    out = std::move(resolved);
    return true;
  }

  if (input.facts.format == Format::kFtl && input.layout == ResourceLayout::kGame) {
    std::string entity_class;
    if (!modelEntityClassFromFtl(input.path, entity_class)) {
      out = std::move(resolved);
      return true;
    }
    if (!pistoris::paths::itemIconFromEntityClass(entity_class, resolved.path)) return false;
    if (resolved.path.empty()) {
      out = std::move(resolved);
      return true;
    }
    resolved.enabled = true;
    resolved.lookup = ImageLookupMode::kGamePriority;
    out = std::move(resolved);
    return true;
  }

  if (!siblingStem(input.location, input.path, io, resolved.location)) return false;
  if (!io.parentPathLocation(resolved.location, resolved.base, error)) {
    diagnostic(DiagnosticCode::kIoPathInvalid, "Cannot resolve Model inventory icon folder: %s", error.c_str());
    return false;
  }
  resolved.path = resourceFormatStem(resolved.location.path);
  resolved.enabled = true;
  resolved.lookup = ImageLookupMode::kGamePriority;
  out = std::move(resolved);
  return true;
}

bool resolveInventoryIconOutput(const OutputTarget& output, IoService& io, InventoryIconOutput& out) {
  InventoryIconOutput resolved;
  if (output.format == Format::kFtl && output.layout == ResourceLayout::kGame) {
    resolved.item_only = true;
    std::string entity_class;
    std::string icon;
    if (modelEntityClassFromFtl(output.path, entity_class) &&
        pistoris::paths::itemIconFromEntityClass(entity_class, icon) && !icon.empty()) {
      resolved.enabled = true;
      resolved.stem = {.path = std::move(icon), .address = PathAddress::kMountRelative};
    }
    out = std::move(resolved);
    return true;
  }
  if (!siblingStem(output, output.path, io, resolved.stem)) return false;
  resolved.enabled = true;
  out = std::move(resolved);
  return true;
}

bool readInventoryIcon(IoService& io, const InventoryIconInput& input, LoadedInventoryIcon& out) {
  LoadedInventoryIcon loaded;
  if (!input.enabled) {
    out = std::move(loaded);
    return true;
  }

  const ResourceReadResult result =
      input.exact ? io.readPath(input.location, loaded.encoded, &loaded.resolved_path)
                  : io.readImage(input.base, input.path, input.lookup, loaded.encoded, nullptr, &loaded.resolved_path);
  if (result != ResourceReadResult::kSuccess) {
    if (result == ResourceReadResult::kNotFound && !input.required) {
      out = std::move(loaded);
      return true;
    }
    if (input.required) {
      reportRequiredReadFailure(
          result, "Inventory icon", input.exact ? std::string_view(input.location.path) : std::string_view(input.path));
      return false;
    }
    log(ARX_LOG_WARN, "Model inventory icon could not be read and was skipped: %s", input.path.c_str());
    out = std::move(loaded);
    return true;
  }
  out = std::move(loaded);
  return true;
}

bool projectInventoryIcon(const pistoris::Model& model, const pistoris::Model::InventoryIconRenderOptions& options,
                          std::vector<std::uint8_t>& out) {
  const ArxReturnCode rc = model.renderIconPng(options, out);
  if (rc != ARX_OK) {
    diagnostic(DiagnosticCode::kModelOutputFailed,
               "Model inventory icon rendering failed: %s (code %d)",
               pistoris::errorString(rc),
               static_cast<int>(rc));
    return false;
  }
  return true;
}

bool addInventoryIconOutput(ResourceOutputPlan& plan, const InventoryIconOutput& output,
                            std::span<const std::uint8_t> encoded, ArxImageFormat format, ResourceAssetId asset) {
  if (encoded.empty()) return true;
  if (!output.enabled) {
    if (output.item_only) log(ARX_LOG_WARN, "Model inventory icon omitted for non-item native output");
    return true;
  }
  const std::string_view extension = media::imageExtension(format);
  if (extension.empty()) {
    diagnostic(DiagnosticCode::kModelOutputFailed, "Model inventory icon is invalid");
    return false;
  }
  PathLocation target = output.stem;
  target.path += extension;
  plan.add(ResourceFileKind::kImage, std::move(target), encoded.data(), encoded.size(), asset);
  return true;
}

bool addInventoryIconOutput(ResourceOutputPlan& plan, const InventoryIconOutput& output, const pistoris::Model& model,
                            const pistoris::Model::InventoryIconRenderOptions& options,
                            std::vector<std::uint8_t>& rendered, ResourceAssetId asset) {
  if (!projectInventoryIcon(model, options, rendered)) return false;
  return addInventoryIconOutput(plan, output, rendered, ARX_IMAGE_FORMAT_PNG, asset);
}

}  // namespace cli
