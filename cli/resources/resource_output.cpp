// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/resource_output.h"

#include "arx_pistoris/runtime/types.h"

#include "base/ascii.h"
#include "console/diagnostics.h"
#include "console/logging.h"
#include "io/path_location.h"
#include "io/service.h"
#include "resources/output.h"
#include "resources/selector.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cli {
namespace {

const char* resourceName(ResourceFileKind kind) noexcept {
  switch (kind) {
    case ResourceFileKind::kAudio:
      return "audio";
    case ResourceFileKind::kImage:
      return "image";
  }
  return "resource";
}

const char* assetName(ResourceAssetKind kind) noexcept {
  switch (kind) {
    case ResourceAssetKind::kAmbiance:
      return "Ambiance";
    case ResourceAssetKind::kAnimation:
      return "Animation";
    case ResourceAssetKind::kCinematic:
      return "Cinematic";
    case ResourceAssetKind::kLevel:
      return "Level";
    case ResourceAssetKind::kModel:
      return "Model";
  }
  return "asset";
}

std::string outputKey(const PathLocation& target) {
  std::string key;
  key.reserve(target.path.size() + 2U);
  key.push_back(target.address == PathAddress::kMountRelative ? 'r' : 'a');
  key.push_back(':');
  bool insensitive = target.address == PathAddress::kMountRelative;
#ifdef _WIN32
  insensitive = true;
#endif
  for (char value : target.path) key.push_back(insensitive ? lowerAscii(value) : value);
  return key;
}

}  // namespace

ResourceAssetId ResourceOutputPlan::addAsset(ResourceAssetKind kind, std::string identity) {
  const ResourceAssetId id = assets_.size();
  assets_.push_back({kind, std::move(identity)});
  return id;
}

void ResourceOutputPlan::reserveOutput(PathLocation target) {
  reserved_outputs_.push_back(std::move(target));
  resolved_ = false;
}

void ResourceOutputPlan::add(ResourceFileKind kind, PathLocation target, const void* data, std::size_t size,
                             ResourceAssetId asset) {
  Candidate candidate;
  candidate.kind = kind;
  candidate.target = std::move(target);
  candidate.data = data;
  candidate.size = size;
  candidate.asset = asset;
  candidates_.push_back(std::move(candidate));
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

bool ResourceOutputPlan::resolve(bool dry_run, bool keep_first) {
  selected_.clear();
  resolved_ = false;

  for (const Candidate& candidate : candidates_) {
    if (candidate.asset >= assets_.size()) {
      diagnostic(DiagnosticCode::kResourceOutputInvalid, "Resource output has an invalid asset reference");
      return false;
    }
    if (candidate.size != 0 && !candidate.payload()) {
      diagnostic(DiagnosticCode::kResourceOutputInvalid,
                 "%s resource output '%s' has no payload data",
                 resourceName(candidate.kind),
                 candidate.target.path.c_str());
      return false;
    }
  }

  struct Group {
    std::size_t first = std::numeric_limits<std::size_t>::max();
    std::size_t last = std::numeric_limits<std::size_t>::max();
    std::size_t count = 0;
  };
  std::vector<Group> groups;
  groups.reserve(candidates_.size());
  std::vector<std::size_t> next(candidates_.size(), std::numeric_limits<std::size_t>::max());
  std::unordered_set<std::string> reserved;
  reserved.reserve(reserved_outputs_.size());
  for (const PathLocation& output : reserved_outputs_) reserved.insert(outputKey(output));
  std::unordered_map<std::string, std::size_t> by_target;
  by_target.reserve(candidates_.size());
  for (std::size_t index = 0; index < candidates_.size(); ++index) {
    const std::string key = outputKey(candidates_[index].target);
    if (reserved.contains(key)) {
      diagnostic(DiagnosticCode::kResourceOutputCollision,
                 "%s resource output '%s' collides with a primary asset output",
                 resourceName(candidates_[index].kind),
                 candidates_[index].target.path.c_str());
      return false;
    }
    auto [entry, inserted] = by_target.emplace(key, groups.size());
    if (inserted) groups.emplace_back();
    Group& group = groups[entry->second];
    if (group.first == std::numeric_limits<std::size_t>::max()) {
      group.first = index;
    } else {
      next[group.last] = index;
    }
    group.last = index;
    ++group.count;
  }

  const auto same_payload = [&](std::size_t left_index, std::size_t right_index) {
    const Candidate& left = candidates_[left_index];
    const Candidate& right = candidates_[right_index];
    if (left.size != right.size) return false;
    if (left.size == 0) return true;
    const auto* left_data = static_cast<const std::uint8_t*>(left.payload());
    const auto* right_data = static_cast<const std::uint8_t*>(right.payload());
    return std::equal(left_data, left_data + left.size, right_data);
  };

  for (const Group& group : groups) {
    const std::size_t first = group.first;
    bool identical = true;
    for (std::size_t index = next[first]; index != std::numeric_limits<std::size_t>::max(); index = next[index]) {
      if (!same_payload(first, index)) {
        identical = false;
        break;
      }
    }
    if (identical) {
      selected_.push_back(first);
      continue;
    }

    const Candidate& first_candidate = candidates_[first];
    const char* resource = resourceName(first_candidate.kind);
    if (dry_run) {
      log(ARX_LOG_WARN,
          "dry-run: %zu different %s payloads target '%s'",
          group.count,
          resource,
          first_candidate.target.path.c_str());
    } else if (keep_first) {
      log(ARX_LOG_WARN,
          "%s resource collision at '%s': keeping the first candidate",
          resource,
          first_candidate.target.path.c_str());
    } else {
      std::fprintf(stderr, "Different %s data targets '%s':\n\n", resource, first_candidate.target.path.c_str());
    }

    std::size_t option = 0;
    for (std::size_t index = first; index != std::numeric_limits<std::size_t>::max(); index = next[index], ++option) {
      const Candidate& candidate = candidates_[index];
      const Asset& asset = assets_[candidate.asset];
      if (dry_run || keep_first) {
        log(ARX_LOG_INFO,
            "  %zu. %s '%s': %zu bytes%s",
            option + 1U,
            assetName(asset.kind),
            asset.identity.c_str(),
            candidate.size,
            !dry_run && keep_first && option == 0 ? " (kept)" : "");
      } else {
        std::fprintf(stderr,
                     "  %zu. %s '%s' - %zu bytes\n",
                     option + 1U,
                     assetName(asset.kind),
                     asset.identity.c_str(),
                     candidate.size);
      }
    }

    if (dry_run) continue;
    if (keep_first) {
      selected_.push_back(first);
      continue;
    }

    for (;;) {
      std::fprintf(stderr, "\nSelect the resource data to write [1-%zu]: ", group.count);
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
          value <= group.count) {
        std::size_t selected = first;
        for (std::size_t offset = 1; offset < value; ++offset) selected = next[selected];
        selected_.push_back(selected);
        break;
      }
      std::fprintf(stderr, "Please enter a number from 1 to %zu\n", group.count);
    }
  }

  resolved_ = true;
  return true;
}

bool ResourceOutputPlan::write(IoService& io) const {
  if (!resolved_) {
    diagnostic(DiagnosticCode::kResourceOutputInvalid, "Resource output plan has not been resolved");
    return false;
  }
  for (const std::size_t index : selected_) {
    const Candidate& candidate = candidates_[index];
    if (!writeOutput(io, candidate.target, candidate.payload(), candidate.size)) return false;
  }
  return true;
}

bool ResourceOutputService::resolve(ResourceOutputPlan& plan) const { return plan.resolve(dry_run_, keep_first_); }

bool ResourceOutputService::write(const ResourceOutputPlan& plan) const { return plan.write(io_); }

}  // namespace cli
