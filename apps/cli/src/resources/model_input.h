// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.hpp"

#include "console/diagnostics.h"
#include "formats/classification.h"

#include <string>
#include <string_view>
#include <vector>

namespace cli {

class IoService;

struct ConvertedModelInput {
  pistoris::Model model;
  std::vector<pistoris::Animation> animations;
  std::vector<pistoris::AnimationSoundSourceReference> sound_sources;
  std::vector<std::string> texture_source_paths;
};

struct ModelInputConversionOptions {
  pistoris::Model::GlbImportOptions glb;
  pistoris::NativeTextMode native_text_mode = pistoris::NativeTextMode::kAuto;
};

bool isModelInput(FileFacts facts) noexcept;
bool convertModelInput(const ClassifiedPath& input, IoService& io, const ModelInputConversionOptions& options,
                       DiagnosticCode failure_code, std::string_view description, ConvertedModelInput& out);

}  // namespace cli
