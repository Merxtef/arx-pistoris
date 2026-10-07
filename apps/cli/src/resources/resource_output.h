// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/resource_io/output.hpp"

#include "io/path_location.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cli {

class IoService;

enum class ResourceFileKind : std::uint8_t {
  kData,
  kAudio,
  kImage,
};

enum class ResourceAssetKind : std::uint8_t {
  kAmbiance,
  kAnimation,
  kCinematic,
  kLevel,
  kModel,
};

using ResourceAssetId = std::size_t;

class ResourceOutputPlan {
 public:
  ResourceAssetId addAsset(ResourceAssetKind kind, std::string identity);
  void addPrimary(PathLocation target, const void* data, std::size_t size, ResourceAssetId asset);
  void addPrimaryOwned(PathLocation target, std::vector<std::uint8_t> data, ResourceAssetId asset);
  void add(ResourceFileKind kind, PathLocation target, const void* data, std::size_t size, ResourceAssetId asset);
  void addOwned(ResourceFileKind kind, PathLocation target, std::vector<std::uint8_t> data, ResourceAssetId asset);

  bool resolve(IoService& io, bool dry_run, bool keep_first);
  bool write(IoService& io);

  [[nodiscard]] std::size_t selectedCount() const noexcept;

 private:
  struct Asset {
    ResourceAssetKind kind = ResourceAssetKind::kAnimation;
    std::string identity;
  };

  struct Candidate {
    [[nodiscard]] const void* payload() const noexcept { return owned_data.empty() ? data : owned_data.data(); }

    ResourceFileKind kind = ResourceFileKind::kAudio;
    PathLocation target;
    const void* data = nullptr;
    std::size_t size = 0;
    ResourceAssetId asset = 0;
    bool primary = false;
    std::vector<std::uint8_t> owned_data;
  };

  std::vector<Asset> assets_;
  std::vector<Candidate> candidates_;
  std::optional<pistoris::resource_io::ResourceWritePlan> prepared_;
  bool resolved_ = false;
};

class ResourceOutputService {
 public:
  ResourceOutputService(IoService& io, bool dry_run, bool keep_first) noexcept
      : io_(io), dry_run_(dry_run), keep_first_(keep_first) {}

  bool resolve(ResourceOutputPlan& plan) const;
  bool write(ResourceOutputPlan& plan) const;

 private:
  IoService& io_;
  bool dry_run_ = false;
  bool keep_first_ = false;
};

}  // namespace cli
