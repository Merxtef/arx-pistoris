// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pistoris {

struct ModelModules;
struct AnimationModules;

ArxReturnCode importModelFromGlb(std::span<const std::uint8_t> glb, const Model::GlbImportOptions& options,
                                 ModelModules& out, std::vector<AnimationModules>* out_animations,
                                 ArxAnimationConversionReport* report,
                                 std::vector<std::string>* texture_source_paths = nullptr,
                                 std::vector<AnimationSoundSourceReference>* sound_sources = nullptr);
ArxReturnCode exportModelToGlb(const ModelModules& model, const Model::GlbExportOptions& options,
                               std::span<const AnimationModules* const> animations,
                               ArxAnimationConversionReport* report, std::vector<std::uint8_t>& out,
                               std::vector<AnimationSoundFile>* sound_files = nullptr);
ArxReturnCode exportModelLevelPreviewToGlb(const ModelModules& model, const Model::LevelPreviewGlbOptions& options,
                                           std::vector<std::uint8_t>& out);

}  // namespace pistoris
