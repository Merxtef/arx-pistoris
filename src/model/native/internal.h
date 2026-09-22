// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/bake.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/texture.hpp"

#include "model/data.h"
#include "modules/action_points.h"
#include "modules/geometry.h"
#include "modules/selections.h"
#include "modules/skeleton.h"
#include "native/ftl.h"
#include "utils/identifier.h"
#include "utils/log.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pistoris::model_native {

inline constexpr ArxVector3 kSyntheticNormal = {0.0f, -1.0f, 0.0f};
inline constexpr double kNormalRepairEpsilon = 1.0e-4;
inline constexpr std::array<std::string_view, 6> kCutSelectionNames = {
    "cut_head",
    "cut_torso",
    "cut_larm",
    "cut_rarm",
    "cut_lleg",
    "cut_rleg",
};

inline bool reportableIdentifierRepair(IdentifierRepair repair) noexcept {
  return repair != IdentifierRepair::kNone && repair != IdentifierRepair::kCase;
}

template <std::size_t N>
std::string_view fixedString(const char (&value)[N]) noexcept {
  const char* end = static_cast<const char*>(std::memchr(value, '\0', N));
  return {value, end ? static_cast<std::size_t>(end - value) : N};
}

inline bool cutSelection(std::string_view name) noexcept {
  return std::find(kCutSelectionNames.begin(), kCutSelectionNames.end(), name) != kCutSelectionNames.end();
}

inline std::uint16_t& vertexComponent(Vec3<std::uint16_t>& value, std::size_t component) noexcept {
  if (component == 0) return value.x;
  if (component == 1) return value.y;
  return value.z;
}

inline std::uint16_t vertexComponent(const Vec3<std::uint16_t>& value, std::size_t component) noexcept {
  if (component == 0) return value.x;
  if (component == 1) return value.y;
  return value.z;
}

inline float& vectorComponent(ArxVector3& value, std::size_t component) noexcept {
  if (component == 0) return value.x;
  if (component == 1) return value.y;
  return value.z;
}

inline float vectorComponent(const ArxVector3& value, std::size_t component) noexcept {
  if (component == 0) return value.x;
  if (component == 1) return value.y;
  return value.z;
}

}  // namespace pistoris::model_native
