// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/sound.hpp"

#include <span>
#include <vector>

namespace pistoris {

struct AnimationModules;
struct ModelModules;

namespace glb {
class Builder;
}

namespace glb_model {

ArxReturnCode addAnimationsToGlb(const ModelModules& model, std::span<const AnimationModules* const> animations,
                                 ArxAnimationConversionReport* report, int motion_node, std::span<const int> bone_nodes,
                                 float units, glb::Builder& builder, std::vector<AnimationSoundFile>* sound_files);

}  // namespace glb_model
}  // namespace pistoris
