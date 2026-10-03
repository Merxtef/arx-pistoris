// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"

#include <string_view>

namespace pistoris {
struct ModelModules;
namespace glb {
class Builder;
}
}  // namespace pistoris

namespace pistoris::glb_model {

struct ModelMeshExportOptions {
  float position_scale = 1.0f;
  bool level_basis = false;
  bool include_authoring_attributes = false;
  std::string_view context = "Model -> GLB";
  std::string_view mesh_name = "model";
};

ArxReturnCode addModelMesh(const ModelModules& model, const ModelMeshExportOptions& options, glb::Builder& builder,
                           int& out_mesh);

}  // namespace pistoris::glb_model
