// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/sound.hpp"

#include "external/glb/utils/names.h"
#include "modules/cinematic.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pistoris::glb_cinematic {

struct RootName {
  std::optional<float> fps;
  std::optional<std::int32_t> end_frame;
};

struct IllustrationName {
  std::uint32_t ordinal = 0;
  std::int32_t subdivision_scale = 1;
};

std::string rootName(const CinematicData& cinematic);
bool parseRootName(std::string_view name, RootName& out, glb::ParsedLabel* label = nullptr);
bool rootNameCandidate(std::string_view name) noexcept;

std::string illustrationName(std::uint32_t ordinal, const CinematicIllustration& illustration);
bool parseIllustrationName(std::string_view name, IllustrationName& out, glb::ParsedLabel* label = nullptr);
bool illustrationNameCandidate(std::string_view name) noexcept;

std::string keyName(const CinematicKeyframe& key);
bool parseKeyName(std::string_view name, CinematicKeyframe& out, glb::ParsedLabel* label = nullptr);

std::string flashName(const CinematicKeyframe& key);
bool parseFlashName(std::string_view name, CinematicKeyframe& key, glb::ParsedLabel* label = nullptr);

std::string lightName(const CinematicLight& light);
bool parseLightName(std::string_view name, CinematicLight& out, glb::ParsedLabel* label = nullptr);

std::string soundName(SoundKind kind);
bool parseSoundName(std::string_view name, SoundKind& out, glb::ParsedLabel* label = nullptr);
std::string soundPathName(std::string_view path);
bool parseSoundPathName(std::string_view name, std::string& out);

}  // namespace pistoris::glb_cinematic
