// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/animation_output.h"

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"

#include "base/resource_path.h"
#include "console/logging.h"
#include "formats/format.h"
#include "io/path_location.h"
#include "io/paths.h"
#include "resources/layout.h"
#include "resources/selector.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cli {
namespace {

std::string animationExtension(Format format) {
  switch (format) {
    case Format::kTea:
      return ".tea";
    case Format::kJson:
      return ".json";
    default:
      return {};
  }
}

std::string withOrdinal(std::string_view base, std::size_t ordinal) {
  constexpr std::size_t kPortableFilenameStemMax = 240;
  const std::string suffix = std::to_string(ordinal);
  const std::size_t prefix_size =
      suffix.size() < kPortableFilenameStemMax ? kPortableFilenameStemMax - suffix.size() : 0;
  std::string candidate(base.substr(0, prefix_size));
  while (!candidate.empty() && candidate.back() == '.') candidate.pop_back();
  candidate += suffix;
  return sanitizeFilename(candidate);
}

struct AnimationNamePlan {
  std::size_t index = 0;
  std::string source_name;
  std::string source_key;
  std::string base;
  std::string output_path;
};

}  // namespace

bool buildAnimationTargets(std::span<const AnimationOutputIdentity> animations, const OutputTarget& base,
                           std::string_view resource_directory, Format output_format,
                           std::span<const std::string_view> reserved_paths, std::vector<OutputTarget>& out,
                           std::string& error) {
  out.clear();
  error.clear();

  const std::string extension = animationExtension(output_format);
  if (extension.empty()) {
    error = "unsupported animation output format";
    return false;
  }

  std::string directory = resource_directory.empty() ? resourceParentPath(base.path) : std::string(resource_directory);
  if (!directory.empty() && directory.back() != '/' && directory.back() != '\\') directory.push_back('/');
  std::string fallback = base.selector.kind != ARX_RESOURCE_KIND_NONE ? base.selector.name : resourceStem(base.path);
  fallback = sanitizeFilename(fallback);
  if (fallback.empty()) fallback = "animation";

  std::vector<AnimationNamePlan> plans;
  plans.reserve(animations.size());
  for (std::size_t index = 0; index < animations.size(); ++index) {
    const AnimationOutputIdentity& animation = animations[index];
    std::string source_name =
        animation.resource_path.empty() ? std::string(animation.name) : resourceStem(animation.resource_path);
    std::string stem = sanitizeFilename(source_name.empty() ? fallback : source_name);
    if (stem.empty()) stem = fallback;
    plans.push_back({index, std::move(source_name), {}, std::move(stem), {}});
    plans.back().source_key = resourcePathKey(plans.back().source_name);
  }

  std::vector<std::size_t> order(plans.size());
  for (std::size_t index = 0; index < order.size(); ++index) order[index] = index;
  std::sort(order.begin(), order.end(), [&](std::size_t left_index, std::size_t right_index) {
    const AnimationNamePlan& left = plans[left_index];
    const AnimationNamePlan& right = plans[right_index];
    if (left.source_key != right.source_key) return left.source_key < right.source_key;
    if (left.source_name != right.source_name) return left.source_name < right.source_name;
    return left.index < right.index;
  });

  const auto output_path = [&](std::string_view stem) {
    std::string path;
    path.reserve(directory.size() + stem.size() + extension.size());
    path.append(directory);
    path.append(stem);
    path.append(extension);
    return path;
  };

  std::unordered_set<std::string> natural_paths;
  for (const AnimationNamePlan& plan : plans) natural_paths.insert(resourcePathKey(output_path(plan.base)));

  std::unordered_set<std::string> used_paths;
  for (const std::string_view path : reserved_paths) used_paths.insert(resourcePathKey(path));

  out.resize(animations.size());
  for (std::size_t plan_index : order) {
    const AnimationNamePlan& plan = plans[plan_index];
    std::string stem = plan.base;
    std::string path = output_path(stem);
    std::string key = resourcePathKey(path);
    for (std::size_t ordinal = 2; used_paths.contains(key); ++ordinal) {
      stem = withOrdinal(plan.base, ordinal);
      path = output_path(stem);
      key = resourcePathKey(path);
      while (natural_paths.contains(key) || used_paths.contains(key)) {
        ++ordinal;
        stem = withOrdinal(plan.base, ordinal);
        path = output_path(stem);
        key = resourcePathKey(path);
      }
    }
    used_paths.insert(key);

    if (plan.source_name.empty()) {
      log(ARX_LOG_WARN, "unnamed animation uses output name '%s'", stem.c_str());
    } else if (plan.base != plan.source_name) {
      log(ARX_LOG_WARN,
          "animation name '%s' sanitized to '%s' for output",
          plan.source_name.c_str(),
          plan.base.c_str());
    }
    if (stem != plan.base) {
      log(ARX_LOG_WARN, "animation output name '%s' disambiguated as '%s'", plan.base.c_str(), stem.c_str());
    }

    OutputTarget target;
    target.path = std::move(path);
    target.address = base.address;
    target.format = output_format;
    target.layout = base.layout;
    out[plan.index] = std::move(target);
  }
  return true;
}

bool buildGameAnimationTargets(std::span<const AnimationOutputIdentity> animations, std::string_view fallback_type,
                               std::span<const std::string_view> reserved_paths, std::vector<OutputTarget>& out,
                               std::string& error) {
  out.clear();
  error.clear();
  std::vector<AnimationNamePlan> plans;
  plans.reserve(animations.size());
  std::vector<bool> inferred;
  inferred.reserve(animations.size());
  for (std::size_t index = 0; index < animations.size(); ++index) {
    const AnimationOutputIdentity& animation = animations[index];
    std::string path;
    pistoris::paths::AnimationPathView parsed;
    bool is_inferred = animation.resource_path.empty();
    std::string source_name;
    if (!is_inferred) {
      if (!pistoris::paths::animationFromTea(animation.resource_path, parsed) ||
          !pistoris::paths::animationTea(parsed, path)) {
        error = "invalid Animation resource path";
        return false;
      }
      source_name = resourceStem(path);
    } else {
      source_name = std::string(animation.name);
      std::string stem = sanitizeFilename(source_name.empty() ? "animation" : source_name);
      if (stem.empty()) stem = "animation";
      if (!pistoris::paths::animationTea({fallback_type, stem}, path)) {
        error = "invalid inferred Animation output path";
        return false;
      }
    }
    std::string stem = resourceStem(path);
    plans.push_back({index, std::move(source_name), resourcePathKey(path), std::move(stem), std::move(path)});
    inferred.push_back(is_inferred);
  }

  std::vector<std::size_t> order(plans.size());
  for (std::size_t index = 0; index < order.size(); ++index) order[index] = index;
  std::sort(order.begin(), order.end(), [&](std::size_t left_index, std::size_t right_index) {
    const AnimationNamePlan& left = plans[left_index];
    const AnimationNamePlan& right = plans[right_index];
    if (left.source_key != right.source_key) return left.source_key < right.source_key;
    if (inferred[left.index] != inferred[right.index]) return !inferred[left.index];
    if (left.source_name != right.source_name) return left.source_name < right.source_name;
    return left.index < right.index;
  });

  std::unordered_set<std::string> natural_paths;
  for (const AnimationNamePlan& plan : plans) natural_paths.insert(plan.source_key);
  std::unordered_set<std::string> used_paths;
  used_paths.reserve(reserved_paths.size() + plans.size());
  for (const std::string_view path : reserved_paths) used_paths.insert(resourcePathKey(path));
  out.resize(animations.size());
  for (const std::size_t plan_index : order) {
    const AnimationNamePlan& plan = plans[plan_index];
    const std::string directory = resourceParentPath(plan.output_path);
    std::string stem = plan.base;
    std::string path = directory + stem + ".tea";
    std::string key = resourcePathKey(path);
    for (std::size_t ordinal = 2; used_paths.contains(key); ++ordinal) {
      stem = withOrdinal(plan.base, ordinal);
      path = directory + stem + ".tea";
      key = resourcePathKey(path);
      while (natural_paths.contains(key) || used_paths.contains(key)) {
        ++ordinal;
        stem = withOrdinal(plan.base, ordinal);
        path = directory + stem + ".tea";
        key = resourcePathKey(path);
      }
    }
    used_paths.insert(key);

    if (plan.source_name.empty()) {
      log(ARX_LOG_WARN, "unnamed animation uses output name '%s'", stem.c_str());
    } else if (inferred[plan.index] && plan.base != plan.source_name) {
      log(ARX_LOG_WARN,
          "animation name '%s' sanitized to '%s' for output",
          plan.source_name.c_str(),
          plan.base.c_str());
    }
    if (stem != plan.base)
      log(ARX_LOG_WARN, "animation output name '%s' disambiguated as '%s'", plan.base.c_str(), stem.c_str());
    if (inferred[plan.index])
      log(ARX_LOG_WARN,
          "animation '%s' has no resource path; inferred output '%s'",
          plan.source_name.c_str(),
          path.c_str());

    OutputTarget target;
    target.path = std::move(path);
    target.address = PathAddress::kMountRelative;
    target.format = Format::kTea;
    target.layout = ResourceLayout::kGame;
    out[plan.index] = std::move(target);
  }
  return true;
}

}  // namespace cli
