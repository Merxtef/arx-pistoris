// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/resource_io/catalog.hpp"

#include "arx_pistoris/base/result.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/resource_io/location.hpp"
#include "arx_pistoris/resource_io/resource_mounts.hpp"
#include "arx_pistoris/resource_io/resources.hpp"

#include "result_failure.h"

#include <cstdint>
#include <map>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

std::string asciiLower(std::string_view value) {
  std::string result(value);
  for (char& character : result) {
    if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + ('a' - 'A'));
  }
  return result;
}

}  // namespace

namespace pistoris::resource_io {

ResourceIoResult<ResourceCatalog> Resources::scanCatalog(const ResourceLookupOptions& lookup) const noexcept {
  try {
    std::map<std::string, ResourceCatalogEntry> entries;
    const auto append =
        [&](const paths::ResourceSearchLocation& location, auto parse, auto select) -> ResourceIoResult<void> {
      auto files = mounts_.enumerate(location.base_path, location.max_discovery_depth, lookup);
      if (!files) return std::move(files).propagate<void>();
      for (const ResourceFile& file : *files) {
        auto parsed = parse(file.logical_path);
        if (!parsed) continue;
        std::string selector;
        if (!select(*parsed, selector)) continue;
        paths::ResourceSelector resource;
        if (!paths::parseResourceSelector(selector, resource)) continue;
        const std::string key = asciiLower(selector);
        auto [entry, inserted] =
            entries.try_emplace(key, ResourceCatalogEntry{std::move(resource), std::move(selector), file.mount_mask});
        if (!inserted) entry->second.mount_mask |= file.mount_mask;
      }
      return ResourceIoResult<void>::success();
    };

    for (paths::ModelPathType type : paths::modelPathTypes()) {
      paths::ResourceSearchLocation location;
      if (!paths::modelSearchLocation(type, location)) continue;
      auto result = append(
          location,
          [](std::string_view path) -> std::optional<paths::ModelPathView> {
            paths::ModelPathView value;
            return paths::modelFromFtl(path, value) ? std::optional(value) : std::nullopt;
          },
          paths::modelSelector);
      if (!result) return std::move(result).propagate<ResourceCatalog>();
    }
    for (paths::AnimationPathType type : paths::animationPathTypes()) {
      paths::ResourceSearchLocation location;
      if (!paths::animationSearchLocation(type, location)) continue;
      auto result = append(
          location,
          [](std::string_view path) -> std::optional<paths::AnimationPathView> {
            paths::AnimationPathView value;
            return paths::animationFromTea(path, value) ? std::optional(value) : std::nullopt;
          },
          paths::animationSelector);
      if (!result) return std::move(result).propagate<ResourceCatalog>();
    }
    {
      auto result = append(
          paths::levelSearchLocation(),
          [](std::string_view path) -> std::optional<std::uint32_t> {
            std::uint32_t value = 0;
            return paths::levelFromDlf(path, value) ? std::optional(value) : std::nullopt;
          },
          [](std::uint32_t level, std::string& out) {
            out = paths::levelSelector(level);
            return true;
          });
      if (!result) return std::move(result).propagate<ResourceCatalog>();
    }
    {
      auto result = append(
          paths::cinematicSearchLocation(),
          [](std::string_view path) -> std::optional<paths::CinematicPathView> {
            paths::CinematicPathView value;
            return paths::cinematicFromCin(path, value) ? std::optional(value) : std::nullopt;
          },
          paths::cinematicSelector);
      if (!result) return std::move(result).propagate<ResourceCatalog>();
    }
    {
      auto result = append(
          paths::ambianceSearchLocation(),
          [](std::string_view path) -> std::optional<paths::AmbiancePathView> {
            paths::AmbiancePathView value;
            return paths::ambianceFromAmb(path, value) ? std::optional(value) : std::nullopt;
          },
          paths::ambianceSelector);
      if (!result) return std::move(result).propagate<ResourceCatalog>();
    }

    std::vector<ResourceCatalogEntry> result;
    result.reserve(entries.size());
    for (auto& [unused, entry] : entries) result.push_back(std::move(entry));
    return ResourceIoResult<ResourceCatalog>::success(ResourceCatalog(std::move(result)));
  } catch (const std::bad_alloc&) {
    return detail::resourceIoFailure<ResourceCatalog>(
        ARX_BAD_ALLOC, ResourceIoOperation::kScanCatalog, {}, {}, lookup.mount_mask);
  } catch (...) {
    return detail::resourceIoFailure<ResourceCatalog>(
        ARX_INTERNAL_ERROR, ResourceIoOperation::kScanCatalog, {}, {}, lookup.mount_mask);
  }
}

}  // namespace pistoris::resource_io
