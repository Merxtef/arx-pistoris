// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/resource_io/resource_mounts.hpp"

#include <span>
#include <string>
#include <utility>
#include <vector>

namespace pistoris::resource_io {

struct ResourceCatalogEntry {
  paths::ResourceSelector resource;
  std::string selector;
  ResourceMountMask mount_mask = 0;
};

class ResourceCatalog {
 public:
  ResourceCatalog() = default;
  [[nodiscard]] std::span<const ResourceCatalogEntry> entries() const noexcept { return entries_; }

 private:
  explicit ResourceCatalog(std::vector<ResourceCatalogEntry> entries) : entries_(std::move(entries)) {}
  std::vector<ResourceCatalogEntry> entries_;

  friend class Resources;
};

}  // namespace pistoris::resource_io
