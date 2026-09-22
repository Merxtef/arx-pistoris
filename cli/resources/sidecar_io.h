// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/paths/types.h"

#include "io/path_location.h"  // IWYU pragma: export
#include "resources/layout.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace cli {

class IoService;
struct ClassifiedPath;
struct OutputTarget;

inline constexpr std::string_view kLooseTextureDirectory = "textures";
inline constexpr std::string_view kLooseSoundDirectory = "sounds";

struct SidecarEndpoint {
  ResourceLayout layout = ResourceLayout::kLoose;
  bool selector_addressed = false;
};

enum class SidecarRebaseDirection : std::uint8_t {
  kNone,
  kToLoose,
  kToGame,
};

struct SidecarRebasePolicy {
  bool explicit_requested = false;
  std::string_view explicit_directory;
  SidecarRebaseDirection automatic = SidecarRebaseDirection::kNone;
  std::string_view to_loose_directory;
  std::string_view to_game_directory;
};

struct ResolvedSidecarRebase {
  bool enabled = false;
  std::string directory;
};

SidecarEndpoint sidecarEndpoint(const ClassifiedPath& input, ArxResourceKind selector_kind) noexcept;
SidecarEndpoint sidecarEndpoint(const OutputTarget& output, ArxResourceKind selector_kind) noexcept;
SidecarRebaseDirection automaticSidecarRebaseTarget(SidecarEndpoint output) noexcept;
SidecarRebaseDirection automaticSidecarRebase(SidecarEndpoint input, SidecarEndpoint output) noexcept;
bool resolveSidecarInputBase(const ClassifiedPath& input, bool use_format_sources, bool input_folder_specified,
                             std::string_view input_folder, std::string_view owner, std::string_view resource,
                             IoService& io, PathLocation& out);
bool resolveSidecarRebase(const SidecarRebasePolicy& policy, std::string_view resource, IoService& io,
                          ResolvedSidecarRebase& out);
bool resolveSidecarOutputBase(const OutputTarget& output, std::string_view owner, std::string_view resource,
                              IoService& io, PathLocation& out);

}  // namespace cli
