// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/sound.hpp"

#include "console/diagnostics.h"
#include "io/path_location.h"
#include "media/encoded.h"
#include "resources/layout.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace pistoris {
class Ambiance;
class Animation;
struct CinematicSoundFile;
}  // namespace pistoris

namespace cli {

enum class AudioLookupMode : std::uint8_t;
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

enum class AudioCandidateResult : std::uint8_t {
  kLoaded,
  kNotFound,
  kFailed,
};

AudioCandidateResult loadAudioCandidate(IoService& io, const PathLocation* base, std::string_view path,
                                        std::string_view owner, media::PreparedAudio& out);
AudioCandidateResult loadAudioCandidate(IoService& io, const PathLocation* base, std::string_view path,
                                        AudioLookupMode mode, std::string_view owner, media::PreparedAudio& out);
AudioCandidateResult loadAudioCandidate(IoService& io, const PathLocation& location, std::string_view display_path,
                                        std::string_view owner, media::PreparedAudio& out);

bool loadSoundData(pistoris::Ambiance& ambiance, IoService& io, const SoundInput& input,
                   std::span<const pistoris::SoundSourceReference> sources = {});
bool loadSoundData(pistoris::Animation& animation, IoService& io, const SoundInput& input,
                   std::span<const pistoris::SoundSourceReference> sources = {});
void loadNativeSoundFiles(const pistoris::amb::Data& ambiance, pistoris::NativeTextMode text_mode, IoService& io,
                          const SoundInput& input, std::vector<pistoris::SoundFile>& out);
void loadNativeSoundFiles(const pistoris::tea::Data& animation, pistoris::NativeTextMode text_mode, IoService& io,
                          const SoundInput& input, std::vector<pistoris::SoundFile>& out);
bool addSoundFileOutputs(ResourceOutputPlan& plan, IoService& io, const SoundOutput& output,
                         std::span<const pistoris::SoundFile> files, ResourceAssetId asset, DiagnosticCode failure_code,
                         std::string_view owner);
bool addSoundFileOutputs(ResourceOutputPlan& plan, IoService& io, const SoundOutput& output,
                         std::span<const pistoris::CinematicSoundFile> files, ResourceAssetId asset,
                         DiagnosticCode failure_code, std::string_view owner);

}  // namespace cli
