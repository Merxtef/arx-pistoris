// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/selector.h"

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include "console/diagnostics.h"
#include "formats/format.h"
#include "io/path_location.h"
#include "io/service.h"
#include "resources/layout.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace cli {
namespace {

bool parseLevelSelector(std::string_view argument, ResourceSelector& out, std::string& error) {
  std::uint32_t level = 0;
  if (!pistoris::paths::levelFromSelector(argument, level)) {
    error = "level selector must be level:<uint32>";
    return false;
  }
  out.kind = ARX_RESOURCE_KIND_LEVEL;
  out.level = level;
  out.name = "level" + std::to_string(level);
  out.logical_path = pistoris::paths::levelDlf(level);
  return true;
}

bool parseModelSelector(std::string_view argument, ResourceSelector& out, std::string& error) {
  pistoris::paths::ModelPathView model;
  if (!pistoris::paths::modelFromSelector(argument, model) || !pistoris::paths::modelFtl(model, out.logical_path)) {
    error = "model selector must be model:<type>:<name>[:<tweak>]";
    return false;
  }
  out.kind = ARX_RESOURCE_KIND_MODEL;
  out.type = model.type;
  out.name = model.name;
  out.tweak = model.tweak;
  return true;
}

bool parseAnimationSelector(std::string_view argument, ResourceSelector& out, std::string& error) {
  pistoris::paths::AnimationPathView animation;
  if (!pistoris::paths::animationFromSelector(argument, animation) ||
      !pistoris::paths::animationTea(animation, out.logical_path)) {
    error = "animation selector must be anim:<npc|fix_inter>:<name>";
    return false;
  }
  out.kind = ARX_RESOURCE_KIND_ANIMATION;
  out.type = animation.type;
  out.name = animation.name;
  return true;
}

bool parseCinematicSelector(std::string_view argument, ResourceSelector& out, std::string& error) {
  pistoris::paths::CinematicPathView cinematic;
  if (!pistoris::paths::cinematicFromSelector(argument, cinematic) ||
      !pistoris::paths::cinematicCin(cinematic, out.logical_path)) {
    error = "cinematic selector must be cinematic:<name>";
    return false;
  }
  out.kind = ARX_RESOURCE_KIND_CINEMATIC;
  out.name = cinematic.name;
  return true;
}

bool parseAmbianceSelector(std::string_view argument, ResourceSelector& out, std::string& error) {
  pistoris::paths::AmbiancePathView ambiance;
  if (!pistoris::paths::ambianceFromSelector(argument, ambiance) ||
      !pistoris::paths::ambianceAmb(ambiance, out.logical_path)) {
    error = "ambiance selector must be ambiance:<name>";
    return false;
  }
  out.kind = ARX_RESOURCE_KIND_AMBIANCE;
  out.name = ambiance.name;
  return true;
}

Format selectorFormat(ArxResourceKind kind) {
  if (kind == ARX_RESOURCE_KIND_LEVEL) return Format::kDlf;
  if (kind == ARX_RESOURCE_KIND_MODEL) return Format::kFtl;
  if (kind == ARX_RESOURCE_KIND_ANIMATION) return Format::kTea;
  if (kind == ARX_RESOURCE_KIND_AMBIANCE) return Format::kAmb;
  if (kind == ARX_RESOURCE_KIND_CINEMATIC) return Format::kCin;
  return Format::kUnknown;
}

}  // namespace

SelectorParseStatus parseResourceSelector(std::string_view argument, ResourceSelector& out, std::string& error) {
  out = {};
  error = {};
  bool valid = false;
  switch (pistoris::paths::resourceSelectorKind(argument)) {
    case ARX_RESOURCE_KIND_LEVEL:
      valid = parseLevelSelector(argument, out, error);
      break;
    case ARX_RESOURCE_KIND_MODEL:
      valid = parseModelSelector(argument, out, error);
      break;
    case ARX_RESOURCE_KIND_ANIMATION:
      valid = parseAnimationSelector(argument, out, error);
      break;
    case ARX_RESOURCE_KIND_CINEMATIC:
      valid = parseCinematicSelector(argument, out, error);
      break;
    case ARX_RESOURCE_KIND_AMBIANCE:
      valid = parseAmbianceSelector(argument, out, error);
      break;
    case ARX_RESOURCE_KIND_NONE:
    default:
      return SelectorParseStatus::kNotSelector;
  }
  return valid ? SelectorParseStatus::kValid : SelectorParseStatus::kInvalid;
}

bool resolveOutputTarget(const char* argument, const IoService& io, OutputTarget& out) {
  out = {};
  std::string error;
  SelectorParseStatus status = parseResourceSelector(argument, out.selector, error);
  if (status == SelectorParseStatus::kInvalid) {
    diagnostic(
        DiagnosticCode::kResourceSelectorInvalid, "Invalid output resource selector '%s': %s", argument, error.c_str());
    return false;
  }
  if (status == SelectorParseStatus::kValid) {
    out.path = out.selector.logical_path;
    out.format = selectorFormat(out.selector.kind);
    out.layout = ResourceLayout::kGame;
    return true;
  }

  std::string path_error;
  OutputLocation location;
  if (!io.resolveOutputLocation(argument, location, path_error)) {
    diagnostic(DiagnosticCode::kIoPathInvalid, "Invalid output path '%s': %s", argument, path_error.c_str());
    return false;
  }
  static_cast<OutputLocation&>(out) = std::move(location);
  out.format = formatFromPath(argument);
  out.layout = primaryResourceLayout(out.format, out.address);
  return true;
}

}  // namespace cli
