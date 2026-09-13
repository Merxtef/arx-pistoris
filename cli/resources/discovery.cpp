// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/discovery.h"

#include "arx_pistoris/paths.hpp"

#include "base/natural_order.h"
#include "console/diagnostics.h"
#include "io/service.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cli {
namespace {

bool enumerate(const pistoris::paths::ResourceSearchLocation& location, IoService& io, std::vector<std::string>& out) {
  ResourceEnumerationResult result = io.enumerateResources(location.base_path, location.max_discovery_depth, out);
  if (result == ResourceEnumerationResult::kSuccess) return true;
  diagnostic(DiagnosticCode::kIoStatFailed,
             "Cannot enumerate mounted resources below '%.*s'",
             static_cast<int>(location.base_path.size()),
             location.base_path.data());
  return false;
}

bool appendModels(IoService& io, std::vector<std::string>& out) {
  std::vector<std::string> paths;
  for (std::string_view type : pistoris::paths::modelSelectorTypes()) {
    pistoris::paths::ResourceSearchLocation location;
    if (!pistoris::paths::modelSearchLocation(type, location) || !enumerate(location, io, paths)) return false;
    for (const std::string& path : paths) {
      pistoris::paths::ModelPathView model;
      std::string selector;
      if (pistoris::paths::modelFromFtl(path, model) && pistoris::paths::modelSelector(model, selector)) {
        out.push_back(std::move(selector));
      }
    }
  }
  return true;
}

bool appendAnimations(IoService& io, std::vector<std::string>& out) {
  std::vector<std::string> paths;
  for (std::string_view type : pistoris::paths::animationSelectorTypes()) {
    pistoris::paths::ResourceSearchLocation location;
    if (!pistoris::paths::animationSearchLocation(type, location) || !enumerate(location, io, paths)) return false;
    for (const std::string& path : paths) {
      pistoris::paths::AnimationPathView animation;
      std::string selector;
      if (pistoris::paths::animationFromTea(path, animation) &&
          pistoris::paths::animationSelector(animation, selector)) {
        out.push_back(std::move(selector));
      }
    }
  }
  return true;
}

bool appendLevels(IoService& io, std::vector<std::string>& out) {
  std::vector<std::string> paths;
  if (!enumerate(pistoris::paths::levelSearchLocation(), io, paths)) return false;
  for (const std::string& path : paths) {
    std::uint32_t level = 0;
    if (pistoris::paths::levelFromDlf(path, level)) out.push_back(pistoris::paths::levelSelector(level));
  }
  return true;
}

bool appendCinematics(IoService& io, std::vector<std::string>& out) {
  std::vector<std::string> paths;
  if (!enumerate(pistoris::paths::cinematicSearchLocation(), io, paths)) return false;
  for (const std::string& path : paths) {
    pistoris::paths::CinematicPathView cinematic;
    std::string selector;
    if (pistoris::paths::cinematicFromCin(path, cinematic) && pistoris::paths::cinematicSelector(cinematic, selector)) {
      out.push_back(std::move(selector));
    }
  }
  return true;
}

bool appendAmbiances(IoService& io, std::vector<std::string>& out) {
  std::vector<std::string> paths;
  if (!enumerate(pistoris::paths::ambianceSearchLocation(), io, paths)) return false;
  for (const std::string& path : paths) {
    pistoris::paths::AmbiancePathView ambiance;
    std::string selector;
    if (pistoris::paths::ambianceFromAmb(path, ambiance) && pistoris::paths::ambianceSelector(ambiance, selector)) {
      out.push_back(std::move(selector));
    }
  }
  return true;
}

bool appendKind(ResourceListingKind kind, IoService& io, std::vector<std::string>& out) {
  switch (kind) {
    case ResourceListingKind::kLevel:
      return appendLevels(io, out);
    case ResourceListingKind::kModel:
      return appendModels(io, out);
    case ResourceListingKind::kAnimation:
      return appendAnimations(io, out);
    case ResourceListingKind::kCinematic:
      return appendCinematics(io, out);
    case ResourceListingKind::kAmbiance:
      return appendAmbiances(io, out);
    case ResourceListingKind::kNone:
    case ResourceListingKind::kAll:
      break;
  }
  return false;
}

}  // namespace

bool printResourceListing(ResourceListingKind kind, IoService& io) {
  std::vector<std::string> resources;
  if (kind == ResourceListingKind::kAll) {
    constexpr ResourceListingKind kKinds[] = {
        ResourceListingKind::kLevel,
        ResourceListingKind::kModel,
        ResourceListingKind::kAnimation,
        ResourceListingKind::kCinematic,
        ResourceListingKind::kAmbiance,
    };
    for (ResourceListingKind current : kKinds)
      if (!appendKind(current, io, resources)) return false;
  } else if (!appendKind(kind, io, resources)) {
    return false;
  }

  std::ranges::sort(resources, naturalStringLess);
  for (const std::string& resource : resources) std::printf("%s\n", resource.c_str());
  return true;
}

}  // namespace cli
