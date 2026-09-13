// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.hpp"

#include "console/diagnostics.h"
#include "formats/classification.h"
#include "resources/model_input_io.h"  // IWYU pragma: export

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace cli {

struct ConvertedModelInput {
  pistoris::Model model;
  std::vector<std::unique_ptr<pistoris::Animation>> animations;
  std::vector<pistoris::AnimationSoundSourceReference> sound_sources;
  std::vector<std::string> texture_source_paths;
};

bool isModelInput(FileFacts facts) noexcept;
bool convertModelInput(const ClassifiedPath& input, std::span<const ModelMaterialLibraryInput> material_libraries,
                       const pistoris::Model::GlbImportOptions& glb_options, DiagnosticCode failure_code,
                       std::string_view description, ConvertedModelInput& out);

}  // namespace cli
