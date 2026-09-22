// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "resources/sidecar_io.h"

#include "arx_pistoris/paths/types.h"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "io/service.h"
#include "resources/layout.h"
#include "resources/selector.h"

#include <string>
#include <string_view>

namespace cli {

SidecarEndpoint sidecarEndpoint(const ClassifiedPath& input, ArxResourceKind selector_kind) noexcept {
  return {.layout = input.layout, .selector_addressed = input.resource_kind == selector_kind};
}

SidecarEndpoint sidecarEndpoint(const OutputTarget& output, ArxResourceKind selector_kind) noexcept {
  return {.layout = output.layout, .selector_addressed = output.selector.kind == selector_kind};
}

SidecarRebaseDirection automaticSidecarRebaseTarget(SidecarEndpoint output) noexcept {
  if (output.layout == ResourceLayout::kLoose) return SidecarRebaseDirection::kToLoose;
  if (output.layout == ResourceLayout::kGame && output.selector_addressed) return SidecarRebaseDirection::kToGame;
  return SidecarRebaseDirection::kNone;
}

SidecarRebaseDirection automaticSidecarRebase(SidecarEndpoint input, SidecarEndpoint output) noexcept {
  switch (automaticSidecarRebaseTarget(output)) {
    case SidecarRebaseDirection::kToLoose:
      return input.layout == ResourceLayout::kGame && input.selector_addressed ? SidecarRebaseDirection::kToLoose
                                                                               : SidecarRebaseDirection::kNone;
    case SidecarRebaseDirection::kToGame:
      return input.layout == ResourceLayout::kLoose ? SidecarRebaseDirection::kToGame : SidecarRebaseDirection::kNone;
    case SidecarRebaseDirection::kNone:
      return SidecarRebaseDirection::kNone;
  }
  return SidecarRebaseDirection::kNone;
}

bool resolveSidecarInputBase(const ClassifiedPath& input, bool use_format_sources, bool input_folder_specified,
                             std::string_view input_folder, std::string_view owner, std::string_view resource,
                             IoService& io, PathLocation& out) {
  if (!use_format_sources) return true;
  std::string error;
  if (input_folder_specified) {
    if (io.resolvePathLocation(input_folder, out, error)) return true;
    diagnostic(DiagnosticCode::kIoPathInvalid,
               "Invalid %.*s input %.*s folder '%.*s': %s",
               static_cast<int>(owner.size()),
               owner.data(),
               static_cast<int>(resource.size()),
               resource.data(),
               static_cast<int>(input_folder.size()),
               input_folder.data(),
               error.c_str());
    return false;
  }
  if (io.parentPathLocation(input.location, out, error)) return true;
  diagnostic(DiagnosticCode::kIoPathInvalid,
             "Cannot resolve default %.*s %.*s folder: %s",
             static_cast<int>(owner.size()),
             owner.data(),
             static_cast<int>(resource.size()),
             resource.data(),
             error.c_str());
  return false;
}

bool resolveSidecarRebase(const SidecarRebasePolicy& policy, std::string_view resource, IoService& io,
                          ResolvedSidecarRebase& out) {
  out = {};
  std::string_view directory;
  if (policy.explicit_requested) {
    directory = policy.explicit_directory;
  } else if (policy.automatic == SidecarRebaseDirection::kToLoose) {
    directory = policy.to_loose_directory;
  } else if (policy.automatic == SidecarRebaseDirection::kToGame) {
    directory = policy.to_game_directory;
  }
  if (!policy.explicit_requested && policy.automatic == SidecarRebaseDirection::kNone) return true;
  if (!policy.explicit_requested && directory.empty()) {
    out.enabled = true;
    return true;
  }
  std::string error;
  if (!io.normalizeResourcePath(directory, out.directory, error)) {
    diagnostic(DiagnosticCode::kResourcePathInvalid,
               "Invalid %.*s resource directory '%.*s': %s",
               static_cast<int>(resource.size()),
               resource.data(),
               static_cast<int>(directory.size()),
               directory.data(),
               error.c_str());
    return false;
  }
  out.enabled = true;
  return true;
}

bool resolveSidecarOutputBase(const OutputTarget& output, std::string_view owner, std::string_view resource,
                              IoService& io, PathLocation& out) {
  std::string error;
  if (io.parentPathLocation(output, out, error)) return true;
  diagnostic(DiagnosticCode::kIoPathInvalid,
             "Cannot resolve %.*s %.*s output folder: %s",
             static_cast<int>(owner.size()),
             owner.data(),
             static_cast<int>(resource.size()),
             resource.data(),
             error.c_str());
  return false;
}

}  // namespace cli
