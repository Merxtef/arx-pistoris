// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/math.h"
#include "arx_pistoris/level.hpp"

#include "io/path_location.h"
#include "media/encoded.h"
#include "resources/layout.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cli {

class IoService;
class ResourceOutputPlan;
struct ClassifiedPath;
struct OutputTarget;

namespace level {

struct LevelImageLocation {
  bool enabled = false;
  PathLocation base;
  std::string stem;
};

struct LevelImageInput {
  LevelImageLocation minimap;
  LevelImageLocation loading_screen;
  ResourceLayout layout = ResourceLayout::kLoose;
  std::uint32_t level = 0;
};

struct LevelImageOutput {
  bool minimap_enabled = false;
  bool loading_screen_enabled = false;
  PathLocation minimap_stem;
  PathLocation loading_screen_stem;
  ResourceLayout layout = ResourceLayout::kLoose;
  std::uint32_t level = 0;
  pistoris::ArxVector2 projection_offset{};
  pistoris::ArxColor3 minimap_border_color{1.0f, 1.0f, 1.0f};
};

struct LoadedMinimap {
  media::PreparedImage image;
  pistoris::ArxVector2 projection_offset{};
};

struct LoadedLevelImages {
  LoadedMinimap minimap;
  media::PreparedImage loading_screen;
};

struct GeneratedLevelImages {
  std::vector<std::uint8_t> minimap;
  std::vector<std::uint8_t> loading_screen;
};

bool resolveLevelImageInput(const ClassifiedPath& input, IoService& io, LevelImageInput& out);
bool resolveLevelImageOutput(const OutputTarget& output, IoService& io, LevelImageOutput& out);
void loadLevelImages(IoService& io, const LevelImageInput& input, LoadedLevelImages& out);
void applyLevelImages(pistoris::Level& level, LoadedLevelImages& loaded);
bool addDirectLevelImageOutputs(ResourceOutputPlan& plan, const LevelImageInput& input, const LevelImageOutput& output,
                                const LoadedLevelImages& images, GeneratedLevelImages& generated, std::size_t asset);
bool addIntermediateLevelImageOutputs(ResourceOutputPlan& plan, const LevelImageOutput& output,
                                      const pistoris::Level& level, GeneratedLevelImages& generated, std::size_t asset);

}  // namespace level
}  // namespace cli
