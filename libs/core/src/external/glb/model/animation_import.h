// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/sound.hpp"

#include "external/glb/accessor.h"
#include "external/glb/container.h"
#include "external/glb/node_graph.h"

#include <cstddef>
#include <span>
#include <vector>

namespace pistoris {

struct AnimationModules;
struct ModelModules;

namespace glb_model {

struct ModelDiscovery;

ArxReturnCode importAnimations(const glb::Asset& asset, glb::AccessorCache& accessors, const glb::NodeGraph& graph,
                               const ModelDiscovery& discovery, float units, const ModelModules& model,
                               const std::vector<BoneIndex>& node_bones, std::vector<AnimationModules>& out,
                               ArxAnimationConversionReport* report,
                               std::vector<AnimationSoundSourceReference>* sound_sources);

}  // namespace glb_model
}  // namespace pistoris
