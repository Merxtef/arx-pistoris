// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/sound.hpp"

#include <variant>
#include <vector>

namespace cli::ambiance {

struct NativeAmbiance {
  NativeAmbiance() = default;
  NativeAmbiance(const NativeAmbiance&) = delete;
  NativeAmbiance& operator=(const NativeAmbiance&) = delete;
  NativeAmbiance(NativeAmbiance&&) = delete;
  NativeAmbiance& operator=(NativeAmbiance&&) = delete;

  pistoris::Amb ambiance;
  pistoris::NativeTextMode text_mode = pistoris::NativeTextMode::kUtf8;
};

struct IntermediateAmbiance {
  IntermediateAmbiance() = default;
  IntermediateAmbiance(const IntermediateAmbiance&) = delete;
  IntermediateAmbiance& operator=(const IntermediateAmbiance&) = delete;
  IntermediateAmbiance(IntermediateAmbiance&&) = delete;
  IntermediateAmbiance& operator=(IntermediateAmbiance&&) = delete;

  pistoris::Ambiance ambiance;
  std::vector<pistoris::SoundSourceReference> sound_sources;
};

using AmbianceInput = std::variant<std::monostate, NativeAmbiance, IntermediateAmbiance>;

}  // namespace cli::ambiance
