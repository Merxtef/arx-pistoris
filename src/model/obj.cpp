// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/model/obj.hpp"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/status_boundary.h"
#include "external/obj/model.h"
#include "model/data.h"  // NOLINT(misc-include-cleaner)
#include "utils/log.h"

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {

ArxReturnCode Model::importObj(Model& out, std::string_view obj, std::string_view mtl,
                               std::vector<std::string>* texture_source_paths) noexcept {
  if (mtl.empty()) return importObj(out, obj, std::span<const ObjMaterialLibraryView>{}, texture_source_paths);
  const std::array libraries = {ObjMaterialLibraryView{.path = "<inline>", .text = mtl}};
  return importObj(out, obj, libraries, texture_source_paths);
}

ArxReturnCode Model::importObj(Model& out, std::string_view obj,
                               std::span<const ObjMaterialLibraryView> material_libraries,
                               std::vector<std::string>* texture_source_paths) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    log(ARX_LOG_INFO, "=== OBJ -> Model conversion started ===");
    Model result;
    std::vector<std::string> source_paths;
    ArxReturnCode rc =
        importObjToModel(obj, material_libraries, *result.data_, texture_source_paths ? &source_paths : nullptr);
    if (rc != ARX_OK) {
      log(ARX_LOG_ERROR, "=== OBJ -> Model conversion failed (code {}) ===", static_cast<int>(rc));
      return rc;
    }
    rc = result.validate();
    if (rc != ARX_OK) {
      log(ARX_LOG_ERROR,
          "OBJ -> Model: constructed data failed validation (code {}); possible importer bug",
          static_cast<int>(rc));
      log(ARX_LOG_ERROR, "=== OBJ -> Model conversion failed (code {}) ===", static_cast<int>(rc));
      return rc;
    }
    out.swap(result);
    if (texture_source_paths) *texture_source_paths = std::move(source_paths);
    log(ARX_LOG_INFO,
        "=== OBJ -> Model conversion completed: {} vertices, {} faces, {} textures, {} action points ===",
        out.vertexCount(),
        out.faceCount(),
        out.textureCount(),
        out.actionPointCount());
    return ARX_OK;
  });
}

ArxReturnCode Model::exportObj(std::string_view stem, ObjBundle& out) const noexcept {
  return exportObj(stem, ObjExportOptions{}, out);
}

ArxReturnCode Model::exportObj(std::string_view stem, const ObjExportOptions& options, ObjBundle& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    log(ARX_LOG_INFO, "=== Model -> OBJ conversion started ===");
    const ArxReturnCode rc = validate();
    if (rc != ARX_OK) {
      log(ARX_LOG_ERROR, "=== Model -> OBJ conversion failed (code {}) ===", static_cast<int>(rc));
      return rc;
    }
    const ArxReturnCode export_rc = exportModelToObj(*data_, stem, options, out);
    if (export_rc != ARX_OK) {
      log(ARX_LOG_ERROR, "=== Model -> OBJ conversion failed (code {}) ===", static_cast<int>(export_rc));
      return export_rc;
    }
    log(ARX_LOG_INFO,
        "=== Model -> OBJ conversion completed: {} vertices, {} faces, {} texture files ===",
        vertexCount(),
        faceCount(),
        out.texture_files.size());
    return ARX_OK;
  });
}

}  // namespace pistoris
