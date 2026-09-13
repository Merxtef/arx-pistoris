// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "utils/resource_path.h"

#include "utils/portable_filename.h"
#include "utils/unique_value.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

char lowerAscii(char value) noexcept {
  return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

bool forbiddenComponentCharacter(unsigned char value) noexcept {
  if (value < 32) return true;
  switch (value) {
    case '<':
    case '>':
    case ':':
    case '"':
    case '#':
    case '|':
    case '?':
    case '*':
      return true;
    default:
      return false;
  }
}

std::size_t extensionOffset(std::string_view component) noexcept {
  const std::size_t dot = component.find_last_of('.');
  return dot != std::string_view::npos && dot != 0 && dot + 1U < component.size() ? dot : component.size();
}

void constrainComponent(std::string& component, ResourcePathRepair& repair) {
  if (component.size() <= kPortableNameMax) return;
  const std::size_t extension = extensionOffset(component);
  const std::string ending = extension == component.size() ? std::string{} : component.substr(extension);
  const std::size_t prefix_size =
      ending.size() < kPortableNameMax ? kPortableNameMax - ending.size() : kPortableNameMax;
  component = component.substr(0, prefix_size);
  if (ending.size() < kPortableNameMax) component += ending;
  repair |= ResourcePathRepair::kLength;
}

void repairTrailingCharacters(std::string& component, ResourcePathRepair& repair) {
  while (!component.empty() && (component.back() == '.' || component.back() == ' ')) {
    component.back() = '-';
    repair |= ResourcePathRepair::kTrailing;
  }
}

void repairReservedName(std::string& component, ResourcePathRepair& repair) {
  if (!isPortableReservedName(component)) return;
  const std::size_t extension = component.find('.');
  if (component.size() == kPortableNameMax) component.pop_back();
  component.insert(extension == std::string::npos ? component.size() : extension, 1, '-');
  repair |= ResourcePathRepair::kReserved;
}

ResourcePathError normalizeComponent(std::string_view source, std::string& out, ResourcePathRepair& repair) {
  if (source.empty() || source == "." || source == ".." || source.find('\0') != std::string_view::npos)
    return ResourcePathError::kBadPath;
  out.clear();
  out.reserve(std::min(source.size(), kPortableNameMax));
  for (unsigned char input : source) {
    char value = static_cast<char>(input);
    const char lowered = lowerAscii(value);
    if (lowered != value) repair |= ResourcePathRepair::kCase;
    value = lowered;
    if (forbiddenComponentCharacter(input)) {
      value = '-';
      repair |= ResourcePathRepair::kCharacters;
    }
    out.push_back(value);
  }
  repairTrailingCharacters(out, repair);
  constrainComponent(out, repair);
  repairTrailingCharacters(out, repair);
  repairReservedName(out, repair);
  assert(out.empty() || isPortableResourcePathComponent(out));
  return out.empty() || !isPortableResourcePathComponent(out) ? ResourcePathError::kBadPath : ResourcePathError::kNone;
}

ResourcePathError normalizePathImpl(std::string_view source, std::string& out, ResourcePathRepair& repair) {
  if (source.empty() || source.front() == '/' || source.front() == '\\' ||
      (source.size() >= 2U && ((source[0] >= 'A' && source[0] <= 'Z') || (source[0] >= 'a' && source[0] <= 'z')) &&
       source[1] == ':'))
    return ResourcePathError::kBadPath;

  out.clear();
  std::size_t begin = 0;
  for (;;) {
    const std::size_t separator = source.find_first_of("/\\", begin);
    std::string component;
    const ResourcePathError error = normalizeComponent(source.substr(begin, separator - begin), component, repair);
    if (error != ResourcePathError::kNone) return error;
    if (!out.empty()) out.push_back('/');
    out += component;
    if (separator == std::string_view::npos) break;
    if (source[separator] == '\\') repair |= ResourcePathRepair::kSeparators;
    begin = separator + 1U;
  }
  return ResourcePathError::kNone;
}

std::optional<std::string> suffixedPath(std::string_view path, std::size_t ordinal) {
  const std::size_t slash = path.find_last_of('/');
  const std::size_t component_begin = slash == std::string_view::npos ? 0 : slash + 1U;
  const std::string_view component = path.substr(component_begin);
  const std::size_t extension = extensionOffset(component);
  const std::string suffix = "_" + std::to_string(ordinal);
  std::string_view ending = component.substr(extension);
  const std::size_t ending_limit = kPortableNameMax > suffix.size() ? kPortableNameMax - suffix.size() : 0;
  ending = ending.substr(0, ending_limit);
  const std::size_t prefix_limit = kPortableNameMax - suffix.size() - ending.size();
  std::string result(path.substr(0, component_begin));
  result += component.substr(0, std::min(extension, prefix_limit));
  result += suffix;
  result += ending;
  return result;
}

}  // namespace

ResourcePathNormalization normalizeResourcePath(std::string_view requested) {
  ResourcePathNormalization result;
  result.error = normalizePathImpl(requested, result.value, result.repair);
  return result;
}

ResourcePathNormalization normalizeResourceDirectory(std::string_view requested) {
  if (requested.empty()) return {};
  ResourcePathNormalization result;
  if (requested.front() == '/' || requested.front() == '\\') {
    result.error = ResourcePathError::kBadPath;
    return result;
  }
  while (!requested.empty() && (requested.back() == '/' || requested.back() == '\\')) requested.remove_suffix(1);
  if (requested.empty()) {
    result.error = ResourcePathError::kBadPath;
    return result;
  }
  return normalizeResourcePath(requested);
}

bool isPortableResourcePathComponent(std::string_view component) noexcept {
  if (component.empty() || component == "." || component == ".." || component.size() > kPortableNameMax ||
      component.back() == '.' || component.back() == ' ' || isPortableReservedName(component))
    return false;
  for (unsigned char value : component) {
    if (value == '/' || value == '\\' || forbiddenComponentCharacter(value)) return false;
  }
  return true;
}

bool isResourcePath(std::string_view path) noexcept {
  if (path.empty() || path.front() == '/' || path.front() == '\\' || path.back() == '/' ||
      path.find('\\') != std::string_view::npos || path.find('\0') != std::string_view::npos ||
      (path.size() >= 2U && ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
       path[1] == ':'))
    return false;

  std::size_t begin = 0;
  for (;;) {
    const std::size_t end = path.find('/', begin);
    const std::string_view component = path.substr(begin, end - begin);
    if (!isPortableResourcePathComponent(component)) return false;
    for (unsigned char value : component) {
      if (value >= 'A' && value <= 'Z') return false;
    }
    if (end == std::string_view::npos) return true;
    begin = end + 1U;
  }
}

void normalizeResourcePathIdentity(std::string& path) noexcept {
  for (char& value : path) value = lowerAscii(value == '\\' ? '/' : value);
}

std::string resourcePathIdentityKey(std::string_view path) {
  std::string key(path);
  normalizeResourcePathIdentity(key);
  return key;
}

std::size_t ResourcePathIdentityHash::operator()(std::string_view path) const noexcept {
  std::size_t hash = 0;
  for (char value : path) {
    const char normalized = lowerAscii(value == '\\' ? '/' : value);
    hash = hash * 131U + static_cast<unsigned char>(normalized);
  }
  return hash;
}

bool ResourcePathIdentityEqual::operator()(std::string_view left, std::string_view right) const noexcept {
  if (left.size() != right.size()) return false;
  for (std::size_t index = 0; index < left.size(); ++index) {
    const char left_value = lowerAscii(left[index] == '\\' ? '/' : left[index]);
    const char right_value = lowerAscii(right[index] == '\\' ? '/' : right[index]);
    if (left_value != right_value) return false;
  }
  return true;
}

void ResourcePathUniquifier::reserve(std::size_t count, std::size_t occupied_count) {
  paths_.reserve(count);
  values_.reserve(count + occupied_count);
}

ResourcePathError ResourcePathUniquifier::occupy(std::string_view path) {
  ResourcePathNormalization normalized = normalizeResourcePath(path);
  if (normalized.error == ResourcePathError::kNone) values_.occupy(std::move(normalized.value));
  return normalized.error;
}

void ResourcePathUniquifier::add(std::string& path) { paths_.push_back(&path); }

ResourcePathError ResourcePathUniquifier::apply(ResourcePathRepairSummary* summary,
                                                std::span<ResourcePathRepair> repairs) {
  if (!repairs.empty() && repairs.size() != paths_.size()) return ResourcePathError::kBadPath;
  std::vector<std::string> candidates;
  candidates.reserve(paths_.size());
  std::vector<ResourcePathRepair> local_repairs(paths_.size(), ResourcePathRepair::kNone);
  for (std::size_t index = 0; index < paths_.size(); ++index) {
    ResourcePathNormalization candidate = normalizeResourcePath(*paths_[index]);
    if (candidate.error != ResourcePathError::kNone) return candidate.error;
    local_repairs[index] = candidate.repair;
    candidates.push_back(std::move(candidate.value));
  }

  std::vector<std::uint8_t> duplicates(candidates.size(), 0);
  const ValueUniquifierResult result = values_.apply(std::span(candidates), suffixedPath, duplicates);
  if (result.error == ValueUniquifierError::kExhausted) return ResourcePathError::kBadPath;
  ResourcePathRepairSummary local_summary{.deduplicated = result.changed};
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    if (duplicates[index] != 0) local_repairs[index] |= ResourcePathRepair::kDuplicate;
    if (local_repairs[index] != ResourcePathRepair::kNone) ++local_summary.changed;
    if (local_repairs[index] != ResourcePathRepair::kNone && local_repairs[index] != ResourcePathRepair::kDuplicate)
      ++local_summary.normalized;
  }
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    *paths_[index] = std::move(candidates[index]);
    if (!repairs.empty()) repairs[index] = local_repairs[index];
  }
  if (summary) *summary = local_summary;
  return ResourcePathError::kNone;
}

}  // namespace pistoris
