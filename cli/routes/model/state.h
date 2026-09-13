// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/sound.hpp"
#include "arx_pistoris/texture.hpp"

#include "resources/sidecar_io.h"

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace cli::model {

struct NativeAnimationFile {
  pistoris::Tea tea;
  std::string resource_path;
  std::vector<pistoris::SoundFile> sound_files;
  std::size_t input = 0;
};

struct NativeModelFiles {
  pistoris::Ftl ftl;
  std::vector<std::uint8_t> inventory_icon;
  ArxImageFormat inventory_icon_format = ARX_IMAGE_FORMAT_UNKNOWN;
  std::vector<NativeAnimationFile> animations;
};

struct AnimationSource {
  std::size_t input = 0;
  SidecarEndpoint endpoint;
};

struct IntermediateModel {
  pistoris::Model model;
  std::vector<std::string> texture_source_paths;
  std::vector<std::unique_ptr<pistoris::Animation>> animations;
  std::vector<std::vector<pistoris::SoundSourceReference>> sound_sources;
  std::vector<AnimationSource> animation_sources;
  std::unique_ptr<pistoris::Model> reference;
};

using ModelInput = std::variant<std::monostate, NativeModelFiles, IntermediateModel>;

}  // namespace cli::model
