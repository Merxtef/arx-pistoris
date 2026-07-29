// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/paths/types.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace pistoris::paths {

// Views returned by inverse path helpers borrow from the input path; copy any component that must outlive it
struct ModelPathView {
  std::string_view type = {};
  std::string_view name = {};
  std::string_view tweak = {};
};

struct AnimationPathView {
  std::string_view type = {};
  std::string_view name = {};
};

struct CinematicPathView {
  std::string_view name = {};
};

struct AmbiancePathView {
  std::string_view name = {};
};

struct ResourceSearchLocation {
  std::string_view base_path = {};
  std::uint32_t max_discovery_depth = 0;
};

ArxResourceKind resourceShorthandKind(std::string_view shorthand) noexcept;

// Cross-platform policy for converter-owned emitted filename components. Existing resource identities and host paths
// are not subject to this policy
bool isPortableFilename(std::string_view filename) noexcept;
std::string sanitizePortableFilename(std::string_view filename);

std::string textureFromGame(std::string_view path);
std::string textureToGame(std::string_view path);

bool normalizeZoneAmbiance(std::string_view path, std::string& out);
bool zoneAmbianceFile(std::string_view ambiance, std::string& out);

std::string levelDlf(std::uint32_t level);
std::string levelLlf(std::uint32_t level);
std::string levelFts(std::uint32_t level);
bool levelFromDlf(std::string_view path, std::uint32_t& level) noexcept;
bool levelFromLlf(std::string_view path, std::uint32_t& level) noexcept;
bool levelFromFts(std::string_view path, std::uint32_t& level) noexcept;
bool dlfSceneFromLevelName(std::string_view name, std::string& out);
std::string levelShorthand(std::uint32_t level);
bool levelFromShorthand(std::string_view shorthand, std::uint32_t& level) noexcept;

std::span<const std::string_view> modelTypes() noexcept;
bool modelFtl(ModelPathView model, std::string& out);
bool modelFromFtl(std::string_view path, ModelPathView& out) noexcept;
bool entityClassFromModel(ModelPathView model, std::string& path);
bool modelFromEntityClass(std::string_view path, ModelPathView& out) noexcept;
bool modelShorthand(ModelPathView model, std::string& out);
bool modelFromShorthand(std::string_view shorthand, ModelPathView& out) noexcept;

// The input is an interactive type; non-NPC interactive types map to the fix_inter animation directory
std::span<const std::string_view> animationTypes() noexcept;
bool animationDirectory(std::string_view interactive_type, std::string& out);
bool animationTea(AnimationPathView animation, std::string& out);
bool animationFromTea(std::string_view path, AnimationPathView& out) noexcept;
bool animationShorthand(AnimationPathView animation, std::string& out);
bool animationFromShorthand(std::string_view shorthand, AnimationPathView& out) noexcept;

bool cinematicFile(CinematicPathView cinematic, std::string& out);
bool cinematicFromFile(std::string_view path, CinematicPathView& out) noexcept;
bool cinematicShorthand(CinematicPathView cinematic, std::string& out);
bool cinematicFromShorthand(std::string_view shorthand, CinematicPathView& out) noexcept;

bool ambianceFile(AmbiancePathView ambiance, std::string& out);
bool ambianceFromFile(std::string_view path, AmbiancePathView& out) noexcept;
bool ambianceShorthand(AmbiancePathView ambiance, std::string& out);
bool ambianceFromShorthand(std::string_view shorthand, AmbiancePathView& out) noexcept;

bool modelSearchLocation(std::string_view type, ResourceSearchLocation& out) noexcept;
bool animationSearchLocation(std::string_view type, ResourceSearchLocation& out) noexcept;
ResourceSearchLocation levelSearchLocation() noexcept;
ResourceSearchLocation cinematicSearchLocation() noexcept;
ResourceSearchLocation ambianceSearchLocation() noexcept;

bool ftsFromDlfScene(std::string_view scene_path, std::string& out);

}  // namespace pistoris::paths
