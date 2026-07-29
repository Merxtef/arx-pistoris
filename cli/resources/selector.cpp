// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/selector.h"

#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include "console/diagnostics.h"
#include "formats/format.h"
#include "io/path_location.h"
#include "io/service.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace cli {
namespace {

char lowerAscii(char value) {
  if (value >= 'A' && value <= 'Z') return static_cast<char>(value - 'A' + 'a');
  return value;
}

bool equalAsciiInsensitive(std::string_view lhs, std::string_view rhs) {
  if (lhs.size() != rhs.size()) return false;
  for (std::size_t i = 0; i < lhs.size(); ++i)
    if (lowerAscii(lhs[i]) != lowerAscii(rhs[i])) return false;
  return true;
}

bool registeredExtension(std::string_view extension) {
  static constexpr std::string_view kExtensions[] = {
      ".ftl",
      ".fts",
      ".dlf",
      ".llf",
      ".tea",
      ".obj",
      ".json",
      ".glb",
  };
  for (std::string_view candidate : kExtensions)
    if (equalAsciiInsensitive(extension, candidate)) return true;
  return false;
}

bool parseLevelSelector(std::string_view argument, ResourceSelector& out, std::string& error) {
  std::uint32_t level = 0;
  if (!pistoris::paths::levelFromShorthand(argument, level)) {
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
  if (!pistoris::paths::modelFromShorthand(argument, model) || !pistoris::paths::modelFtl(model, out.logical_path)) {
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
  if (!pistoris::paths::animationFromShorthand(argument, animation) ||
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
  if (!pistoris::paths::cinematicFromShorthand(argument, cinematic) ||
      !pistoris::paths::cinematicFile(cinematic, out.logical_path)) {
    error = "cinematic selector must be cinematic:<name>";
    return false;
  }
  out.kind = ARX_RESOURCE_KIND_CINEMATIC;
  out.name = cinematic.name;
  return true;
}

bool parseAmbianceSelector(std::string_view argument, ResourceSelector& out, std::string& error) {
  pistoris::paths::AmbiancePathView ambiance;
  if (!pistoris::paths::ambianceFromShorthand(argument, ambiance) ||
      !pistoris::paths::ambianceFile(ambiance, out.logical_path)) {
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
  return Format::kUnknown;
}

}  // namespace

SelectorParseStatus parseResourceSelector(std::string_view argument, ResourceSelector& out, std::string& error) {
  out = {};
  error = {};
  bool valid = false;
  switch (pistoris::paths::resourceShorthandKind(argument)) {
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
    return true;
  }

  std::string path_error;
  OutputLocation location;
  if (!io.resolveOutputLocation(argument, location, path_error)) {
    diagnostic(DiagnosticCode::kIoCreateFailed, "Invalid output path '%s': %s", argument, path_error.c_str());
    return false;
  }
  static_cast<OutputLocation&>(out) = std::move(location);
  out.format = formatFromPath(argument);
  return true;
}

std::string resourceParentPath(std::string_view path) {
  std::size_t separator = path.find_last_of("/\\");
  return separator == std::string_view::npos ? std::string{} : std::string(path.substr(0, separator + 1));
}

std::string resourceStem(std::string_view path, bool strip_any_extension) {
  std::size_t separator = path.find_last_of("/\\");
  if (separator != std::string_view::npos) path.remove_prefix(separator + 1);
  std::size_t dot = path.find_last_of('.');
  if (dot != std::string_view::npos && (strip_any_extension || registeredExtension(path.substr(dot)))) {
    path = path.substr(0, dot);
  }
  return std::string(path);
}

}  // namespace cli
