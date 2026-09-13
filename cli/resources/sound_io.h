// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/sound.hpp"

#include "console/diagnostics.h"
#include "io/path_location.h"
#include "resources/layout.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace pistoris {
class Ambiance;
class Animation;
}  // namespace pistoris

namespace cli {

class IoService;
class ResourceOutputPlan;
using ResourceAssetId = std::size_t;

struct SoundInput {
  bool use_format_sources = false;
  PathLocation source_base;
};

struct SoundOutput {
  ResourceLayout layout = ResourceLayout::kGame;
  PathLocation base;
};

bool loadSoundData(pistoris::Ambiance& ambiance, IoService& io, const SoundInput& input,
                   std::span<const pistoris::SoundSourceReference> sources = {});
bool loadSoundData(pistoris::Animation& animation, IoService& io, const SoundInput& input,
                   std::span<const pistoris::SoundSourceReference> sources = {});
void loadNativeSoundFiles(const pistoris::amb::Data& ambiance, IoService& io, const SoundInput& input,
                          std::vector<pistoris::SoundFile>& out);
void loadNativeSoundFiles(const pistoris::tea::Data& animation, IoService& io, const SoundInput& input,
                          std::vector<pistoris::SoundFile>& out);
bool addSoundFileOutputs(ResourceOutputPlan& plan, IoService& io, const SoundOutput& output,
                         std::span<const pistoris::SoundFile> files, ResourceAssetId asset, DiagnosticCode failure_code,
                         std::string_view owner);

}  // namespace cli
