// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "nlohmann/json.hpp"
#include "utils/log.h"
#include "utils/parse_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <format>
#include <limits>
#include <new>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris::json_detail {

using Json = nlohmann::json;

inline Json vector(const ArxVector3& value) { return {{"x", value.x}, {"y", value.y}, {"z", value.z}}; }

inline ArxVector3 add(const ArxVector3& left, const ArxVector3& right) {
  return {left.x + right.x, left.y + right.y, left.z + right.z};
}

inline ArxVector3 subtract(const ArxVector3& left, const ArxVector3& right) {
  return {left.x - right.x, left.y - right.y, left.z - right.z};
}

inline Json angle(const ArxAngle& value) { return {{"a", value.pitch}, {"b", value.yaw}, {"g", value.roll}}; }

inline Json color(const ArxColor3& value) {
  const auto channel = [](float component) {
    return static_cast<std::uint8_t>(std::clamp(std::floor(component * 255.0f), 0.0f, 255.0f));
  };
  return {{"r", channel(value.r)}, {"g", channel(value.g)}, {"b", channel(value.b)}, {"a", 1.0f}};
}

inline const Json* member(const Json& object, const char* key) {
  if (!object.is_object()) return nullptr;
  const auto found = object.find(key);
  return found == object.end() ? nullptr : &*found;
}

inline bool getFloat(const Json& value, float& out) {
  if (!value.is_number()) return false;
  out = value.get<float>();
  return std::isfinite(out);
}

template <class Int>
bool getSigned(const Json& value, Int& out) {
  static_assert(std::numeric_limits<Int>::is_signed);
  if (!value.is_number_integer()) return false;
  const std::int64_t parsed = value.get<std::int64_t>();
  if (parsed < static_cast<std::int64_t>(std::numeric_limits<Int>::min()) ||
      parsed > static_cast<std::int64_t>(std::numeric_limits<Int>::max())) {
    return false;
  }
  out = static_cast<Int>(parsed);
  return true;
}

template <class Int>
bool getUnsigned(const Json& value, Int& out) {
  static_assert(!std::numeric_limits<Int>::is_signed);
  if (!value.is_number_integer()) return false;
  std::uint64_t parsed = 0;
  if (value.is_number_unsigned()) {
    parsed = value.get<std::uint64_t>();
  } else {
    const std::int64_t signed_value = value.get<std::int64_t>();
    if (signed_value < 0) return false;
    parsed = static_cast<std::uint64_t>(signed_value);
  }
  if (parsed > static_cast<std::uint64_t>(std::numeric_limits<Int>::max())) return false;
  out = static_cast<Int>(parsed);
  return true;
}

inline bool getString(const Json& value, std::string& out) {
  if (!value.is_string()) return false;
  out = value.get<std::string>();
  return true;
}

inline bool getBool(const Json& value, bool& out) {
  if (!value.is_boolean()) return false;
  out = value.get<bool>();
  return true;
}

inline bool getVector(const Json& value, ArxVector3& out) {
  const Json* x = member(value, "x");
  const Json* y = member(value, "y");
  const Json* z = member(value, "z");
  return x && y && z && getFloat(*x, out.x) && getFloat(*y, out.y) && getFloat(*z, out.z);
}

inline bool getAngle(const Json& value, ArxAngle& out) {
  const Json* a = member(value, "a");
  const Json* b = member(value, "b");
  const Json* g = member(value, "g");
  return a && b && g && getFloat(*a, out.pitch) && getFloat(*b, out.yaw) && getFloat(*g, out.roll);
}

inline bool getColor(const Json& value, ArxColor3& out) {
  const Json* r = member(value, "r");
  const Json* g = member(value, "g");
  const Json* b = member(value, "b");
  const Json* a = member(value, "a");
  std::uint8_t red = 0;
  std::uint8_t green = 0;
  std::uint8_t blue = 0;
  float alpha = 0.0f;
  if (!r || !g || !b || !a || !getUnsigned(*r, red) || !getUnsigned(*g, green) || !getUnsigned(*b, blue) ||
      !getFloat(*a, alpha) || alpha < 0.0f || alpha > 1.0f) {
    return false;
  }
  out = {static_cast<float>(red) / 255.0f, static_cast<float>(green) / 255.0f, static_cast<float>(blue) / 255.0f};
  return true;
}

inline ArxReturnCode arrayMember(const Json& object, const char* key, const Json*& out, std::size_t limit) {
  out = member(object, key);
  if (!out || !out->is_array()) return ARX_JSON_BAD_SCHEMA;
  return out->size() <= limit ? ARX_OK : ARX_JSON_LIMIT_EXCEEDED;
}

inline bool validSchema(const Json& root, std::string_view expected) {
  const Json* schema = member(root, "$schema");
  return !schema || (schema->is_string() && schema->get_ref<const std::string&>() == expected);
}

template <std::size_t N>
std::string fixedString(const char (&value)[N]) {
  const void* terminator = std::memchr(value, '\0', N);
  const std::size_t size = terminator ? static_cast<const char*>(terminator) - value : static_cast<std::size_t>(N);
  return std::string(value, size);
}

template <std::size_t N>
bool copyFixed(std::string_view source, char (&out)[N]) {
  if (source.size() >= N || source.find('\0') != std::string_view::npos) return false;
  std::memcpy(out, source.data(), source.size());
  std::memset(out + source.size(), 0, N - source.size());
  return true;
}

inline std::string lowerSlashes(std::string_view source) {
  std::string result;
  result.reserve(source.size());
  for (char value : source) {
    if (value == '\\') value = '/';
    if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
    result.push_back(value);
  }
  return result;
}

template <class Fn>
ArxReturnCode guarded(const char* label, Fn&& fn) {
  try {
    return std::forward<Fn>(fn)();
  } catch (const std::bad_alloc&) {
    return ARX_BAD_ALLOC;
  } catch (const nlohmann::json::exception& error) {
    log(ARX_LOG_WARN, std::format("{} JSON: {}", label, error.what()));
    return ARX_JSON_BAD_SCHEMA;
  } catch (const std::exception& error) {
    log(ARX_LOG_WARN, std::format("{} JSON: {}", label, error.what()));
    return ARX_JSON_BAD_SCHEMA;
  }
}

inline ArxReturnCode parse(std::string_view text, Json& out) {
  out = Json::parse(text, nullptr, false);
  return out.is_discarded() ? ARX_JSON_BAD_FORMAT : ARX_OK;
}

inline void dump(const Json& json, bool pretty, std::string& out) { out = pretty ? json.dump(2) : json.dump(); }

}  // namespace pistoris::json_detail
