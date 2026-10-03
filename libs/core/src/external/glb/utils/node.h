// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "cgltf/cgltf.h"

namespace pistoris::glb {

inline bool simpleEmptyNode(const cgltf_node& node) noexcept {
  return node.mesh == nullptr && node.camera == nullptr && node.light == nullptr && node.skin == nullptr &&
         node.extensions_count == 0 && !node.has_mesh_gpu_instancing;
}

}  // namespace pistoris::glb
