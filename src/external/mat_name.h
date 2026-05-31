// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

// Shared Arx material naming for OBJ and GLB

#pragma once

#include "arx_pistoris/common_data.hpp"
#include "arx_pistoris/ftl_data.hpp"

#include "utils/log.h"

#include <format>
#include <set>
#include <string>
#include <string_view>

namespace pistoris {

static inline std::string_view pathStem(std::string_view p) {
  auto sep = p.find_last_of("/\\");
  if (sep != std::string_view::npos) p = p.substr(sep + 1);
  auto dot = p.rfind('.');
  if (dot != std::string_view::npos) p = p.substr(0, dot);
  return p;
}

static inline std::string_view pathExt(std::string_view p) {
  auto sep = p.find_last_of("/\\");
  if (sep != std::string_view::npos) p = p.substr(sep + 1);
  auto dot = p.rfind('.');
  if (dot == std::string_view::npos) return "";
  return p.substr(dot);
}

static inline std::string_view pathFilename(std::string_view p) {
  auto sep = p.find_last_of("/\\");
  if (sep != std::string_view::npos) return p.substr(sep + 1);
  return p;
}

struct FlagEntry {
  FaceType bit;
  const char* name;
};

static constexpr FlagEntry kFlagNames[] = {
    {kFaceBitNoShadow, "NO_SHADOW"},
    {kFaceBitDoublesided, "DOUBLESIDED"},
    {kFaceBitTrans, "TRANS"},
    {kFaceBitWater, "WATER"},
    {kFaceBitGlow, "GLOW"},
    {kFaceBitIgnore, "IGNORE"},
    {kFaceBitQuad, "QUAD"},
    {kFaceBitTiled, "TILED"},
    {kFaceBitMetal, "METAL"},
    {kFaceBitHide, "HIDE"},
    {kFaceBitStone, "STONE"},
    {kFaceBitWood, "WOOD"},
    {kFaceBitGravel, "GRAVEL"},
    {kFaceBitEarth, "EARTH"},
    {kFaceBitNocol, "NOCOL"},
    {kFaceBitLava, "LAVA"},
    {kFaceBitClimb, "CLIMB"},
    {kFaceBitFall, "FALL"},
    {kFaceBitNopath, "NOPATH"},
    {kFaceBitNodraw, "NODRAW"},
    {kFaceBitPrecisePath, "PRECISE_PATH"},
    {kFaceBitNoClimb, "NO_CLIMB"},
    {kFaceBitAngular, "ANGULAR"},
    {kFaceBitAngularIdX0, "ANGULAR_IDX0"},
    {kFaceBitAngularIdX1, "ANGULAR_IDX1"},
    {kFaceBitAngularIdX2, "ANGULAR_IDX2"},
    {kFaceBitAngularIdX3, "ANGULAR_IDX3"},
    {kFaceBitLateMip, "LATE_MIP"},
};

static inline std::string flagSuffix(FaceType type) {
  std::string s;
  for (const auto& f : kFlagNames) {
    if (type & f.bit) {
      s += "__";
      s += f.name;
    }
  }
  return s;
}

// "no_tex" stem when empty. "__" in a stem breaks re-import; Arx assets never contain it
static inline std::string matName(std::string_view tex_stem, FaceType type) {
  std::string name(tex_stem.empty() ? "no_tex" : tex_stem);
  name += flagSuffix(type);
  return name;
}

static inline FaceType decodeFlags(std::string_view suffix) {
  FaceType type = 0;
  while (suffix.starts_with("__")) {
    suffix.remove_prefix(2);
    auto next    = suffix.find("__");
    auto token   = suffix.substr(0, next);
    bool matched = false;
    for (const auto& f : kFlagNames) {
      if (token == f.name) {
        type |= f.bit;
        matched = true;
        break;
      }
    }
    if (!matched) log(ARX_LOG_WARN, std::format("unknown material flag '{}'", token));
    if (next == std::string_view::npos) break;
    suffix = suffix.substr(next);
  }
  return type;
}

static inline std::pair<std::string_view, FaceType> decodeMatName(std::string_view name) {
  auto sep   = name.find("__");
  auto stem  = (sep == std::string_view::npos) ? name : name.substr(0, sep);
  auto flags = (sep == std::string_view::npos) ? std::string_view{} : name.substr(sep);
  if (stem == "no_tex") stem = {};
  return {stem, decodeFlags(flags)};
}

static inline std::set<std::pair<std::int16_t, FaceType>> collectMaterials(const ftl::Data& d) {
  std::set<std::pair<std::int16_t, FaceType>> result;
  for (const auto& face : d.faces) result.emplace(face.texture_id, face.type);
  return result;
}

}  // namespace pistoris
