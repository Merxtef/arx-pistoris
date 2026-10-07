// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/resource_output.h"

#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/resource_io/location.hpp"
#include "arx_pistoris/resource_io/output.hpp"
#include "arx_pistoris/runtime/types.h"

#include "console/diagnostics.h"
#include "console/logging.h"
#include "io/native_path.h"
#include "io/path_location.h"
#include "io/service.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cli {
namespace {

ArxResourceKind resourceKind(ResourceAssetKind kind) noexcept {
  switch (kind) {
    case ResourceAssetKind::kAmbiance:
      return ARX_RESOURCE_KIND_AMBIANCE;
    case ResourceAssetKind::kAnimation:
      return ARX_RESOURCE_KIND_ANIMATION;
    case ResourceAssetKind::kCinematic:
      return ARX_RESOURCE_KIND_CINEMATIC;
    case ResourceAssetKind::kLevel:
      return ARX_RESOURCE_KIND_LEVEL;
    case ResourceAssetKind::kModel:
      return ARX_RESOURCE_KIND_MODEL;
  }
  return ARX_RESOURCE_KIND_NONE;
}

const char* assetName(ArxResourceKind kind) noexcept {
  switch (kind) {
    case ARX_RESOURCE_KIND_AMBIANCE:
      return "Ambiance";
    case ARX_RESOURCE_KIND_ANIMATION:
      return "Animation";
    case ARX_RESOURCE_KIND_CINEMATIC:
      return "Cinematic";
    case ARX_RESOURCE_KIND_LEVEL:
      return "Level";
    case ARX_RESOURCE_KIND_MODEL:
      return "Model";
    case ARX_RESOURCE_KIND_NONE:
    default:
      return "asset";
  }
}

template <class Candidate, class Asset>
pistoris::resource_io::ResourceOutput convertCandidate(const Candidate& candidate, const Asset& asset) {
  pistoris::resource_io::ResourceOutput output;
  output.address = candidate.target.address == PathAddress::kMountRelative
                       ? pistoris::resource_io::ResourceOutputAddress::kLogical
                       : pistoris::resource_io::ResourceOutputAddress::kNative;
  switch (candidate.kind) {
    case ResourceFileKind::kData:
      output.kind = pistoris::resource_io::ResourceOutputKind::kData;
      break;
    case ResourceFileKind::kAudio:
      output.kind = pistoris::resource_io::ResourceOutputKind::kAudio;
      break;
    case ResourceFileKind::kImage:
      output.kind = pistoris::resource_io::ResourceOutputKind::kImage;
      break;
  }
  output.primary = candidate.primary;
  output.owner_kind = resourceKind(asset.kind);
  output.owner_identity = asset.identity;
  if (output.address == pistoris::resource_io::ResourceOutputAddress::kLogical) {
    output.resource_path = candidate.target.path;
  } else {
    const auto* first = reinterpret_cast<const char8_t*>(candidate.target.path.data());
    output.native_path = std::filesystem::path(std::u8string_view(first, candidate.target.path.size()));
  }
  const auto* data = static_cast<const std::uint8_t*>(candidate.payload());
  if (candidate.size != 0) output.data.assign(data, data + candidate.size);
  return output;
}

void printCandidates(const pistoris::resource_io::ResourceWriteEntry& entry, bool dry_run, bool keep_first) {
  const auto candidates = entry.candidates();
  const char* resource = "resource";
  if (!candidates.empty()) {
    switch (candidates.front().kind) {
      case pistoris::resource_io::ResourceOutputKind::kData:
        resource = "primary output";
        break;
      case pistoris::resource_io::ResourceOutputKind::kAudio:
        resource = "audio";
        break;
      case pistoris::resource_io::ResourceOutputKind::kImage:
        resource = "image";
        break;
    }
  }
  const std::string display = io_detail::pathToUtf8(entry.nativePath());
  if (dry_run) {
    log(ARX_LOG_WARN, "dry-run: %zu different %s payloads target '%s'", candidates.size(), resource, display.c_str());
  } else if (keep_first) {
    log(ARX_LOG_WARN, "%s resource collision at '%s': keeping the first candidate", resource, display.c_str());
  } else {
    std::fprintf(stderr, "Different %s data targets '%s':\n\n", resource, display.c_str());
  }
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const auto& candidate = candidates[index];
    const char* kept = !dry_run && keep_first && index == 0 ? " (kept)" : "";
    if (dry_run || keep_first) {
      log(ARX_LOG_INFO,
          "  %zu. %s '%s': %zu bytes%s",
          index + 1U,
          assetName(candidate.owner_kind),
          candidate.owner_identity.c_str(),
          candidate.data.size(),
          kept);
    } else {
      std::fprintf(stderr,
                   "  %zu. %s '%s' - %zu bytes\n",
                   index + 1U,
                   assetName(candidate.owner_kind),
                   candidate.owner_identity.c_str(),
                   candidate.data.size());
    }
  }
}

bool selectCandidate(pistoris::resource_io::ResourceWriteEntry& entry) {
  const std::size_t count = entry.candidates().size();
  for (;;) {
    std::fprintf(stderr, "\nSelect the resource data to write [1-%zu]: ", count);
    char buffer[64] = {};
    if (!std::fgets(buffer, sizeof(buffer), stdin)) {
      std::fputc('\n', stderr);
      diagnostic(DiagnosticCode::kResourceOutputCollision,
                 "No resource collision response; use --keep-first-resource for unattended conversion");
      return false;
    }
    char* end = nullptr;
    errno = 0;
    const unsigned long value = std::strtoul(buffer, &end, 10);
    while (end && (*end == ' ' || *end == '\t')) ++end;
    if (errno == 0 && end != buffer && end && (*end == '\n' || *end == '\r' || *end == '\0') && value != 0 &&
        value <= count)
      return entry.selectCandidate(static_cast<std::size_t>(value - 1U));
    std::fprintf(stderr, "Please enter a number from 1 to %zu\n", count);
  }
}

}  // namespace

ResourceAssetId ResourceOutputPlan::addAsset(ResourceAssetKind kind, std::string identity) {
  const ResourceAssetId id = assets_.size();
  assets_.push_back({kind, std::move(identity)});
  return id;
}

void ResourceOutputPlan::addPrimary(PathLocation target, const void* data, std::size_t size, ResourceAssetId asset) {
  candidates_.push_back({ResourceFileKind::kData, std::move(target), data, size, asset, true, {}});
  resolved_ = false;
}

void ResourceOutputPlan::addPrimaryOwned(PathLocation target, std::vector<std::uint8_t> data, ResourceAssetId asset) {
  Candidate candidate;
  candidate.kind = ResourceFileKind::kData;
  candidate.target = std::move(target);
  candidate.size = data.size();
  candidate.asset = asset;
  candidate.primary = true;
  candidate.owned_data = std::move(data);
  candidates_.push_back(std::move(candidate));
  resolved_ = false;
}

void ResourceOutputPlan::add(ResourceFileKind kind, PathLocation target, const void* data, std::size_t size,
                             ResourceAssetId asset) {
  candidates_.push_back({kind, std::move(target), data, size, asset, false, {}});
  resolved_ = false;
}

void ResourceOutputPlan::addOwned(ResourceFileKind kind, PathLocation target, std::vector<std::uint8_t> data,
                                  ResourceAssetId asset) {
  Candidate candidate;
  candidate.kind = kind;
  candidate.target = std::move(target);
  candidate.size = data.size();
  candidate.asset = asset;
  candidate.owned_data = std::move(data);
  candidates_.push_back(std::move(candidate));
  resolved_ = false;
}

bool ResourceOutputPlan::resolve(IoService& io, bool dry_run, bool keep_first) {
  resolved_ = false;
  prepared_.reset();
  std::vector<pistoris::resource_io::ResourceOutput> outputs;
  outputs.reserve(candidates_.size());
  for (const Candidate& candidate : candidates_) {
    if (candidate.asset >= assets_.size() || (candidate.size != 0 && !candidate.payload())) {
      diagnostic(DiagnosticCode::kResourceOutputInvalid, "Resource output has invalid candidate metadata");
      return false;
    }
    outputs.push_back(convertCandidate(candidate, assets_[candidate.asset]));
  }

  auto plan = io.resources().prepareWrite(std::move(outputs));
  if (!plan) {
    const auto* error = plan.error();
    diagnostic(
        DiagnosticCode::kResourceOutputInvalid,
        "Cannot prepare resource outputs: %s",
        error ? pistoris::resource_io::describeError(*error).c_str() : pistoris::resource_io::errorString(plan.code()));
    return false;
  }
  for (auto& entry : plan->entries()) {
    if (entry.status() != pistoris::resource_io::ResourceWriteStatus::kNeedsCandidate) continue;
    printCandidates(entry, dry_run, keep_first);
    if (dry_run) continue;
    if (keep_first) {
      if (!entry.selectCandidate(0)) return false;
    } else if (!selectCandidate(entry)) {
      return false;
    }
  }
  prepared_ = std::move(*plan);
  resolved_ = true;
  return true;
}

bool ResourceOutputPlan::write(IoService& io) {
  if (!resolved_ || !prepared_) {
    diagnostic(DiagnosticCode::kResourceOutputInvalid, "Resource output plan has not been resolved");
    return false;
  }
  return io.executeWritePlan(*prepared_);
}

std::size_t ResourceOutputPlan::selectedCount() const noexcept {
  if (!prepared_) return 0;
  return static_cast<std::size_t>(std::ranges::count_if(
      prepared_->entries(), [](const auto& entry) { return entry.selectedCandidate().has_value(); }));
}

bool ResourceOutputService::resolve(ResourceOutputPlan& plan) const { return plan.resolve(io_, dry_run_, keep_first_); }

bool ResourceOutputService::write(ResourceOutputPlan& plan) const { return plan.write(io_); }

}  // namespace cli
