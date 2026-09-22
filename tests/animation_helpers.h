// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/tea.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

inline pistoris::tea::Data makeAnimationTea() {
  pistoris::tea::Data result;
  std::strcpy(result.name, "walk");
  result.num_frames = 9;
  result.num_groups = 2;
  result.keyframes.resize(3);
  for (std::size_t index = 0; index < result.keyframes.size(); ++index) {
    pistoris::tea::Keyframe& keyframe = result.keyframes[index];
    keyframe.num_frame = static_cast<std::int32_t>(index * 4U);
    keyframe.flag_frame = index == 1U ? pistoris::kTeaFlagFrameStep : pistoris::kTeaFlagFrameNone;
    keyframe.translate = ArxVector3{static_cast<float>(index), 0.0f, 0.0f};
    keyframe.quat = ArxQuat{};
    keyframe.groups.resize(2);
    for (std::size_t group = 0; group < keyframe.groups.size(); ++group) {
      keyframe.groups[group].translate = {static_cast<float>(index), static_cast<float>(group), 0.0f};
      keyframe.groups[group].zoom = {0.1f * static_cast<float>(group), 0.0f, 0.0f};
    }
  }
  result.keyframes[1].sample.emplace();
  std::strcpy(result.keyframes[1].sample->name, "step");
  return result;
}
