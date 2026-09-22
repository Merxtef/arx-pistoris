// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native/resource_path.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris {
namespace {

char lowerAscii(char value) noexcept {
  return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value;
}

}  // namespace

bool normalizeNativeResourcePath(std::string_view source, std::string& out) {
  if (source.find('\0') != std::string_view::npos) return false;

  std::string normalized;
  normalized.reserve(source.size());
  std::size_t begin = 0;
  while (begin < source.size()) {
    std::size_t end = source.find_first_of("/\\", begin);
    if (end == std::string_view::npos) end = source.size();
    const std::string_view component = source.substr(begin, end - begin);
    begin = end + 1U;
    if (component.empty() || component == ".") continue;
    if (component == "..") {
      if (normalized.empty() || normalized == ".." || normalized.ends_with("/..")) {
        if (!normalized.empty()) normalized.push_back('/');
        normalized += "..";
      } else {
        const std::size_t separator = normalized.find_last_of('/');
        normalized.resize(separator == std::string::npos ? 0 : separator);
      }
      continue;
    }
    if (!normalized.empty()) normalized.push_back('/');
    for (char value : component) normalized.push_back(lowerAscii(value));
  }
  out = std::move(normalized);
  return true;
}

bool normalizeNativeResourceStem(std::string_view source, std::string& out) {
  if (!normalizeNativeResourcePath(source, out)) return false;
  const bool has_info = !out.empty() && out != ".." && !out.ends_with("/..");
  const std::size_t extension = has_info ? out.find_last_of("/.") : std::string::npos;
  if (extension != std::string::npos && out[extension] == '.') out.resize(extension);
  return true;
}

bool encodeNativeResourceStem(std::string_view source, std::string& out) {
  std::string normalized;
  if (!normalizeNativeResourcePath(source, normalized) || normalized != source) return false;
  out = std::move(normalized);
  if (!out.empty()) out.push_back('.');
  return true;
}

bool encodeNativeResourceStem(std::string_view source, std::size_t capacity, std::string& out) {
  if (!encodeNativeResourceStem(source, out)) return false;
  if (out.size() < capacity) return true;

  std::string decoded;
  if (source.size() >= capacity || !normalizeNativeResourceStem(source, decoded) || decoded != source) return false;
  out.assign(source);
  return true;
}

}  // namespace pistoris
