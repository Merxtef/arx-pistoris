// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/model/obj.hpp"

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/location.hpp"
#include "arx_pistoris/model/obj_location.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/result_failure.h"
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

ObjResult<Model> Model::importObj(std::string_view obj, std::string_view mtl,
                                  std::vector<std::string>* texture_source_paths) noexcept {
  if (mtl.empty()) return importObj(obj, std::span<const ObjMaterialLibraryView>{}, texture_source_paths);
  const std::array libraries = {ObjMaterialLibraryView{.path = "<inline>", .text = mtl}};
  return importObj(obj, libraries, texture_source_paths);
}

ObjResult<Model> Model::importObj(std::string_view obj, std::span<const ObjMaterialLibraryView> material_libraries,
                                  std::vector<std::string>* texture_source_paths) noexcept {
  constexpr std::string_view kOperation = "OBJ -> Model conversion";
  return api_detail::objBoundary(
      [&]() -> ObjResult<Model> {
        log(ARX_LOG_INFO, "=== OBJ -> Model conversion started ===");
        Model result;
        std::vector<std::string> source_paths;
        ObjImportFailure failure;
        ArxReturnCode rc = importObjToModel(
            obj, material_libraries, *result.data_, texture_source_paths ? &source_paths : nullptr, &failure);
        if (rc != ARX_OK)
          return api_detail::objFailure<Model>(rc, std::move(failure.location), std::move(failure.detail), kOperation);
        auto validation = result.validate();
        if (!validation)
          return api_detail::remapFailure<Model, ObjLocation>(
              std::move(validation),
              [](const ModelLocation&) {
                return ObjLocation{
                    .source = ObjSource::kObj,
                    .source_index = kNoInputIndex,
                    .line = 0,
                    .source_path = {},
                };
              },
              "constructed Model failed validation; possible importer bug");
        if (texture_source_paths) *texture_source_paths = std::move(source_paths);
        log(ARX_LOG_INFO,
            "=== OBJ -> Model conversion completed: {} vertices, {} faces, {} textures, {} action points ===",
            result.vertexCount(),
            result.faceCount(),
            result.textureCount(),
            result.actionPointCount());
        return ObjResult<Model>::success(std::move(result));
      },
      kOperation);
}

ModelResult<ObjBundle> Model::exportObj(std::string_view stem) const noexcept {
  return exportObj(stem, ObjExportOptions{});
}

ModelResult<ObjBundle> Model::exportObj(std::string_view stem, const ObjExportOptions& options) const noexcept {
  constexpr std::string_view kOperation = "Model -> OBJ conversion";
  if (!data_)
    return api_detail::modelFailure<ObjBundle>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), ModelElement::kResource), {}, kOperation);
  return api_detail::modelBoundary(
      resourcePath(),
      [&]() -> ModelResult<ObjBundle> {
        log(ARX_LOG_INFO, "=== Model -> OBJ conversion started ===");
        auto validation = validate();
        if (!validation) return std::move(validation).propagate<ObjBundle>();
        ObjBundle out;
        const ArxReturnCode export_rc = exportModelToObj(*data_, stem, options, out);
        if (export_rc != ARX_OK)
          return api_detail::modelFailure<ObjBundle>(
              export_rc, api_detail::resourceLocation(resourcePath(), ModelElement::kResource), {}, kOperation);
        log(ARX_LOG_INFO,
            "=== Model -> OBJ conversion completed: {} vertices, {} faces, {} texture files ===",
            vertexCount(),
            faceCount(),
            out.texture_files.size());
        return ModelResult<ObjBundle>::success(std::move(out));
      },
      kOperation);
}

}  // namespace pistoris
