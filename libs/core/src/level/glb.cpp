// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/location.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/location.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/result_failure.h"
#include "api/status_boundary.h"
#include "external/glb/failure.h"
#include "external/glb/level/api.h"
#include "level/data.h"
#include "model/data.h"
#include "utils/log.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

void logGlbStart(std::string_view direction) { log(ARX_LOG_INFO, "=== {} conversion started ===", direction); }

}  // namespace

GlbResult<Level> Level::importGlb(std::span<const std::uint8_t> data) noexcept {
  return importGlb(data, GlbImportOptions{}, nullptr, nullptr);
}

GlbResult<Level> Level::importGlb(std::span<const std::uint8_t> data, const GlbImportOptions& options,
                                  ArxLevelGlbImportInfo* info,
                                  std::vector<std::string>* texture_source_paths) noexcept {
  constexpr std::string_view kOperation = "GLB -> Level conversion";
  return api_detail::glbBoundary(
      [&]() -> GlbResult<Level> {
        constexpr std::string_view kDirection = "GLB -> Level";
        logGlbStart(kDirection);
        Level result;
        ArxLevelGlbImportInfo import_info{};
        std::vector<std::string> source_paths;
        glb::Failure failure;
        ArxReturnCode rc = importLevelFromGlb(data,
                                              static_cast<LevelModules&>(*result.data_),
                                              options,
                                              &result.data_->validation,
                                              &import_info,
                                              texture_source_paths ? &source_paths : nullptr,
                                              &failure);
        if (rc != ARX_OK) {
          return api_detail::glbFailure<Level>(rc, std::move(failure.location), std::move(failure.detail), kOperation);
        }
        log(ARX_LOG_INFO,
            "=== {} conversion completed: {} vertices, {} faces, {} rooms, {} entities ===",
            kDirection,
            result.data_->geometry.vertices.size(),
            result.data_->geometry.faces.size(),
            result.data_->rooms.definitions.size(),
            result.data_->scene.entities.size());
        if (info) *info = import_info;
        if (texture_source_paths) *texture_source_paths = std::move(source_paths);
        return result;
      },
      kOperation);
}

LevelGlbExportResult<std::vector<std::uint8_t>> Level::exportGlb() const noexcept {
  return exportGlb(std::span<const Model* const>{}, GlbExportOptions{}, nullptr);
}

LevelGlbExportResult<std::vector<std::uint8_t>> Level::exportGlb(const GlbExportOptions& options) const noexcept {
  return exportGlb(std::span<const Model* const>{}, options, nullptr);
}

LevelGlbExportResult<std::vector<std::uint8_t>> Level::exportGlb(std::span<const Model* const> model_previews,
                                                                 ArxLevelModelPreviewReport* report) const noexcept {
  return exportGlb(model_previews, GlbExportOptions{}, report);
}

LevelGlbExportResult<std::vector<std::uint8_t>> Level::exportGlb(std::span<const Model* const> model_previews,
                                                                 const GlbExportOptions& options,
                                                                 ArxLevelModelPreviewReport* report) const noexcept {
  if (!data_)
    return api_detail::levelGlbExportFailure<std::vector<std::uint8_t>>(
        ARX_INVALID_STATE,
        LevelGlbExportLocation{api_detail::resourceLocation(resourcePath(), LevelElement::kResource)});
  return api_detail::levelGlbExportBoundary(
      LevelGlbExportLocation{api_detail::resourceLocation(resourcePath(), LevelElement::kResource)},
      [&]() -> LevelGlbExportResult<std::vector<std::uint8_t>> {
        constexpr std::string_view kDirection = "Level -> GLB";
        logGlbStart(kDirection);
        auto validation = validate();
        if (!validation)
          return api_detail::remapFailure<std::vector<std::uint8_t>, LevelGlbExportLocation>(
              std::move(validation), [](const LevelLocation& location) { return LevelGlbExportLocation{location}; });
        if (!data_->validation.derived.referenced_bounds)
          return api_detail::levelGlbExportFailure<std::vector<std::uint8_t>>(
              ARX_LEVEL_NO_GEOMETRY,
              LevelGlbExportLocation{api_detail::resourceLocation(resourcePath(), LevelElement::kFace)});

        ArxLevelModelPreviewReport local_report{};
        std::vector<const ModelModules*> preview_data;
        preview_data.reserve(model_previews.size());
        for (std::size_t input_index = 0; input_index < model_previews.size(); ++input_index) {
          const Model* model = model_previews[input_index];
          if (model == nullptr)
            return api_detail::levelGlbExportFailure<std::vector<std::uint8_t>>(
                ARX_INVALID_DATA_POINTER,
                LevelGlbExportLocation{api_detail::resourceLocation(
                    {}, ModelElement::kResource, kNoElementIndex, kNoElementIndex, input_index)});
          ArxReturnCode rc = model->validate().code();
          if (rc != ARX_OK) {
            ++local_report.skipped_invalid_models;
            continue;
          }
          preview_data.push_back(static_cast<const ModelModules*>(model->data_.get()));
        }
        std::vector<std::uint8_t> out;
        ArxLevelModelPreviewReport export_report{};
        const ArxReturnCode rc = exportLevelToGlb(static_cast<const LevelModules&>(*data_),
                                                  data_->validation.derived.effective_bounds.value_or(ArxAabb{}),
                                                  options,
                                                  preview_data,
                                                  &export_report,
                                                  out);
        local_report.mapped_models += export_report.mapped_models;
        local_report.previewed_entities += export_report.previewed_entities;
        local_report.skipped_anonymous_models += export_report.skipped_anonymous_models;
        local_report.skipped_unmappable_models += export_report.skipped_unmappable_models;
        local_report.skipped_duplicate_models += export_report.skipped_duplicate_models;
        if (rc != ARX_OK)
          return api_detail::levelGlbExportFailure<std::vector<std::uint8_t>>(
              rc, LevelGlbExportLocation{api_detail::resourceLocation(resourcePath(), LevelElement::kResource)});
        if (report) *report = local_report;
        const std::size_t skipped = local_report.skipped_anonymous_models + local_report.skipped_unmappable_models +
                                    local_report.skipped_duplicate_models + local_report.skipped_invalid_models;
        if (skipped != 0) {
          log(ARX_LOG_WARN,
              "Level -> GLB skipped {} Model preview(s): {} anonymous, {} unmappable, {} duplicate, {} invalid",
              skipped,
              local_report.skipped_anonymous_models,
              local_report.skipped_unmappable_models,
              local_report.skipped_duplicate_models,
              local_report.skipped_invalid_models);
        }
        log(ARX_LOG_INFO,
            "=== {} conversion completed: {} vertices, {} faces, {} rooms, {} entities ===",
            kDirection,
            data_->geometry.vertices.size(),
            data_->geometry.faces.size(),
            data_->rooms.definitions.size(),
            data_->scene.entities.size());
        return out;
      });
}

}  // namespace pistoris
