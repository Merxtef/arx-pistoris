// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"
#include "arx_pistoris/runtime/types.h"

#include "external/glb/level/coordinates.h"
#include "external/glb/level/entities.h"
#include "external/glb/model/api.h"
#include "external/glb/model/mesh_export.h"
#include "external/glb/object_coordinates.h"
#include "external/glb/writer.h"
#include "model/data.h"
#include "paths/entity_class.h"
#include "utils/identifier.h"
#include "utils/log.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {
namespace {

bool previewClassPath(std::string_view requested, std::string& out) {
  if (paths::resourceSelectorKind(requested) != ARX_RESOURCE_KIND_NONE) {
    paths::ModelPathView model;
    if (!paths::modelFromSelector(requested, model)) return false;
    return paths::baseEntityClassFromModel(model, out);
  }
  std::string_view removed_extension;
  return normalizeEntityClassPath(requested, out, removed_extension) && removed_extension.empty();
}

}  // namespace

ArxReturnCode exportModelLevelPreviewToGlb(const ModelModules& model, const Model::LevelPreviewGlbOptions& options,
                                           std::vector<std::uint8_t>& out) {
  if (!glb_object::validUnits(options.arx_units_per_glb_unit)) return ARX_INVALID_OPTIONS;

  std::string class_path;
  if (options.class_path.empty()) {
    class_path = "<class-path>";
    log(ARX_LOG_WARN, "Model Level preview has no class path; exported as <class-path>");
  } else if (!previewClassPath(options.class_path, class_path)) {
    return ARX_INVALID_IDENTIFIER;
  }

  const std::string_view requested_asset = options.asset_name.empty() ? std::string_view("asset") : options.asset_name;
  IdentifierNormalization normalized = normalizeIdentifier(requested_asset);
  if (normalized.repair != IdentifierRepair::kNone)
    log(ARX_LOG_INFO, "Model Level preview asset name '{}' normalized to '{}'", requested_asset, normalized.value);

  glb::Builder builder;
  builder.setContentBasisRotation(glb_level::kLevelGlbBasisRotation);
  int mesh = -1;
  const glb_model::ModelMeshExportOptions mesh_options{
      .position_scale = 1.0f / options.arx_units_per_glb_unit,
      .level_basis = true,
      .context = "Model Level preview -> GLB",
      .mesh_name = normalized.value,
  };
  ArxReturnCode rc = glb_model::addModelMesh(model, mesh_options, builder, mesh);
  if (rc != ARX_OK) return rc;

  const int entity = builder.addNode(glb_level::entityNodeName(normalized.value, 0), mesh);
  builder.addChild(entity, builder.addNode(glb_level::entityClassHelperName(class_path, normalized.value)));
  builder.addRoot(entity);
  return builder.write(out);
}

}  // namespace pistoris
