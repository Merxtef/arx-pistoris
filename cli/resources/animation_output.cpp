// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/animation_output.h"

#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/pistoris_types.h"

#include "console/logging.h"
#include "formats/format.h"
#include "io/paths.h"
#include "resources/selector.h"

#include <algorithm>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cli {
namespace {

std::string lowerPath(std::string_view path) {
  std::string result;
  result.reserve(path.size());
  for (char value : path) {
    if (value == '\\') value = '/';
    if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
    result.push_back(value);
  }
  return result;
}

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

std::string teaName(const pistoris::tea::Data& tea) {
  const void* end = std::memchr(tea.name, '\0', sizeof(tea.name));
  const std::size_t size = end ? static_cast<std::size_t>(static_cast<const char*>(end) - tea.name) : sizeof(tea.name);
  return std::string(tea.name, size);
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
};

}  // namespace

bool buildAnimationTargets(std::span<const pistoris::tea::Data> teas, const OutputTarget& base,
                           std::string_view resource_directory, Format output_format,
                           std::span<const OutputTarget> reserved_targets, std::vector<OutputTarget>& out,
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
  plans.reserve(teas.size());
  for (std::size_t index = 0; index < teas.size(); ++index) {
    std::string source_name = teaName(teas[index]);
    std::string stem = sanitizeFilename(source_name.empty() ? fallback : source_name);
    if (stem.empty()) stem = fallback;
    plans.push_back({index, std::move(source_name), {}, std::move(stem)});
    plans.back().source_key = lowerPath(plans.back().source_name);
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
  for (const AnimationNamePlan& plan : plans) natural_paths.insert(lowerPath(output_path(plan.base)));

  std::unordered_set<std::string> used_paths;
  for (const OutputTarget& target : reserved_targets) used_paths.insert(lowerPath(target.path));

  out.resize(teas.size());
  for (std::size_t plan_index : order) {
    const AnimationNamePlan& plan = plans[plan_index];
    std::string stem = plan.base;
    std::string path = output_path(stem);
    std::string key = lowerPath(path);
    for (std::size_t ordinal = 2; used_paths.contains(key); ++ordinal) {
      stem = withOrdinal(plan.base, ordinal);
      path = output_path(stem);
      key = lowerPath(path);
      while (natural_paths.contains(key) || used_paths.contains(key)) {
        ++ordinal;
        stem = withOrdinal(plan.base, ordinal);
        path = output_path(stem);
        key = lowerPath(path);
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
    out[plan.index] = std::move(target);
  }
  return true;
}

bool buildNativeAnimationTargets(std::span<const pistoris::tea::Data> teas, const OutputTarget& base,
                                 std::string_view resource_directory, std::vector<OutputTarget>& out,
                                 std::string& error) {
  return buildAnimationTargets(teas, base, resource_directory, Format::kTea, {}, out, error);
}

}  // namespace cli
