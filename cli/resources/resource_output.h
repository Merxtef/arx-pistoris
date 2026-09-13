// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "io/path_location.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cli {

class IoService;

enum class ResourceFileKind : std::uint8_t {
  kAudio,
  kImage,
};

enum class ResourceAssetKind : std::uint8_t {
  kAmbiance,
  kAnimation,
  kLevel,
  kModel,
};

using ResourceAssetId = std::size_t;

class ResourceOutputPlan {
 public:
  ResourceAssetId addAsset(ResourceAssetKind kind, std::string identity);
  void reserveOutput(PathLocation target);
  void add(ResourceFileKind kind, PathLocation target, const void* data, std::size_t size, ResourceAssetId asset);

  bool resolve(bool dry_run, bool keep_first);
  bool write(IoService& io) const;

  [[nodiscard]] std::size_t selectedCount() const noexcept { return selected_.size(); }

 private:
  struct Asset {
    ResourceAssetKind kind = ResourceAssetKind::kAnimation;
    std::string identity;
  };

  struct Candidate {
    ResourceFileKind kind = ResourceFileKind::kAudio;
    PathLocation target;
    const void* data = nullptr;
    std::size_t size = 0;
    ResourceAssetId asset = 0;
  };

  std::vector<Asset> assets_;
  std::vector<PathLocation> reserved_outputs_;
  std::vector<Candidate> candidates_;
  std::vector<std::size_t> selected_;
  bool resolved_ = false;
};

class ResourceOutputService {
 public:
  ResourceOutputService(IoService& io, bool dry_run, bool keep_first) noexcept
      : io_(io), dry_run_(dry_run), keep_first_(keep_first) {}

  bool resolve(ResourceOutputPlan& plan) const;
  bool write(const ResourceOutputPlan& plan) const;

 private:
  IoService& io_;
  bool dry_run_ = false;
  bool keep_first_ = false;
};

}  // namespace cli
