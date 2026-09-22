// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "paths/ambiance.h"

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include "paths/internal.h"
#include "utils/name_tokens.h"
#include "utils/resource_path.h"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris::paths {

using namespace detail;

std::string_view ambianceSoundDirectory() noexcept { return "sfx/ambiance"; }

bool normalizeZoneAmbiance(std::string_view source, std::string& out) {
  if (hasEmbeddedNull(source)) return false;
  const bool absolute_separator = !source.empty() && (source.front() == '/' || source.front() == '\\');
  const bool drive_path = source.size() >= 2 &&
                          ((source[0] >= 'A' && source[0] <= 'Z') || (source[0] >= 'a' && source[0] <= 'z')) &&
                          source[1] == ':';
  if (absolute_separator || drive_path) return false;

  std::string normalized;
  normalized.reserve(source.size());
  bool previous_separator = false;
  for (char value : source) {
    value = lowerAscii(value);
    if (value == '\\' || value == '/') {
      if (!normalized.empty() && !previous_separator) normalized.push_back('/');
      previous_separator = true;
    } else {
      normalized.push_back(value);
      previous_separator = false;
    }
  }
  while (!normalized.empty() && normalized.back() == '/') normalized.pop_back();
  if (normalized.ends_with(".amb")) normalized.resize(normalized.size() - 4U);
  if (!isResourcePath(normalized)) return false;
  out = std::move(normalized);
  return true;
}

bool ambFromZoneAmbiance(std::string_view ambiance, std::string& out) {
  std::string normalized;
  if (!normalizeZoneAmbiance(ambiance, normalized) || normalized == "none") return false;
  std::string result = "sfx/ambiance/";
  result += normalized;
  result += ".amb";
  out = std::move(result);
  return true;
}

bool ambianceAmb(AmbiancePathView ambiance, std::string& out) {
  std::string name;
  if (!normalizeRelativeName(ambiance.name, {".amb"}, name) || equalAsciiInsensitive(name, "none")) return false;
  std::string result = "sfx/ambiance/";
  result += name;
  result += ".amb";
  out = std::move(result);
  return true;
}

bool ambianceFromAmb(std::string_view path, AmbiancePathView& out) noexcept {
  ResourcePathCursor cursor(path);
  if (!takeExpected(cursor, "sfx") || !takeExpected(cursor, "ambiance")) return false;
  std::string_view first;
  std::string_view last;
  if (!cursor.take(first)) return false;
  const std::size_t name_begin = cursor.lastComponentBegin();
  last = first;
  while (!cursor.done()) {
    if (!cursor.take(last)) return false;
  }
  std::string_view stem;
  if (!stripOptionalExtension(last, {".amb"}, stem)) return false;
  const std::size_t name_end = cursor.lastComponentEnd() - (last.size() - stem.size());
  const std::string_view name = path.substr(name_begin, name_end - name_begin);
  if (equalAsciiInsensitive(name, "none")) return false;
  out = {name};
  return true;
}

bool ambianceSelector(AmbiancePathView ambiance, std::string& out) {
  std::string name;
  if (!normalizeRelativeName(ambiance.name, {".amb"}, name) || equalAsciiInsensitive(name, "none")) return false;
  std::string result = "ambiance:";
  result += name;
  out = std::move(result);
  return true;
}

bool ambianceFromSelector(std::string_view selector, AmbiancePathView& out) noexcept {
  std::array<std::string_view, 2> fields;
  std::size_t count = 0;
  std::string_view name;
  if (!splitSelector(selector, fields, count) || count != fields.size() || !selectorKind(fields[0], "ambiance") ||
      !relativeNameView(fields[1], {".amb"}, name) || equalAsciiInsensitive(name, "none"))
    return false;
  out = {name};
  return true;
}

bool parseZoneAmbianceReference(std::string_view reference, std::string& out) {
  if (equalAsciiInsensitive(reference, "none")) {
    out = "none";
    return true;
  }

  AmbiancePathView ambiance;
  if (resourceSelectorKind(reference) == ARX_RESOURCE_KIND_AMBIANCE) {
    if (!ambianceFromSelector(reference, ambiance)) return false;
    return normalizeZoneAmbiance(ambiance.name, out);
  }
  if (ambianceFromAmb(reference, ambiance)) return normalizeZoneAmbiance(ambiance.name, out);
  return normalizeZoneAmbiance(reference, out);
}

bool formatZoneAmbianceReference(std::string_view ambiance, std::string& out) {
  std::string normalized;
  if (!normalizeZoneAmbiance(ambiance, normalized) || normalized != ambiance) return false;
  if (normalized == "none") {
    out = "none";
    return true;
  }
  std::string result = "ambiance:";
  result += normalized;
  out = std::move(result);
  return true;
}

ResourceSearchLocation ambianceSearchLocation() noexcept { return {"sfx/ambiance", 8}; }

}  // namespace pistoris::paths
