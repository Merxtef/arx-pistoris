// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/runtime/types.h"

#include "api/status_boundary.h"
#include "external/glb/level/api.h"
#include "level/data.h"
#include "model/data.h"
#include "utils/log.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
namespace {

void logGlbStart(std::string_view direction) { log(ARX_LOG_INFO, "=== {} conversion started ===", direction); }

ArxReturnCode logGlbFailure(std::string_view direction, ArxReturnCode rc) {
  log(ARX_LOG_INFO, "=== {} conversion failed with code {} ===", direction, rc);
  return rc;
}

}  // namespace

ArxReturnCode Level::importGlb(Level& out, std::span<const std::uint8_t> data) noexcept {
  return importGlb(out, data, GlbImportOptions{}, nullptr, nullptr);
}

ArxReturnCode Level::importGlb(Level& out, std::span<const std::uint8_t> data, const GlbImportOptions& options,
                               ArxLevelGlbImportInfo* info, std::vector<std::string>* texture_source_paths) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    constexpr std::string_view kDirection = "GLB -> Level";
    logGlbStart(kDirection);
    Level tmp;
    ArxLevelGlbImportInfo import_info{};
    std::vector<std::string> source_paths;
    ArxReturnCode rc = importLevelFromGlb(data,
                                          static_cast<LevelModules&>(*tmp.data_),
                                          options,
                                          &tmp.data_->validation,
                                          &import_info,
                                          texture_source_paths ? &source_paths : nullptr);
    if (rc != ARX_OK) return logGlbFailure(kDirection, rc);
    log(ARX_LOG_INFO,
        "=== {} conversion completed: {} vertices, {} faces, {} rooms, {} entities ===",
        kDirection,
        tmp.data_->geometry.vertices.size(),
        tmp.data_->geometry.faces.size(),
        tmp.data_->rooms.definitions.size(),
        tmp.data_->scene.entities.size());
    out.swap(tmp);
    if (info) *info = import_info;
    if (texture_source_paths) *texture_source_paths = std::move(source_paths);
    return ARX_OK;
  });
}

ArxReturnCode Level::exportGlb(std::vector<std::uint8_t>& out) const noexcept {
  return exportGlb(out, std::span<const Model* const>{}, GlbExportOptions{}, nullptr);
}

ArxReturnCode Level::exportGlb(std::vector<std::uint8_t>& out, const GlbExportOptions& options) const noexcept {
  return exportGlb(out, std::span<const Model* const>{}, options, nullptr);
}

ArxReturnCode Level::exportGlb(std::vector<std::uint8_t>& out, std::span<const Model* const> model_previews,
                               ArxLevelModelPreviewReport* report) const noexcept {
  return exportGlb(out, model_previews, GlbExportOptions{}, report);
}

ArxReturnCode Level::exportGlb(std::vector<std::uint8_t>& out, std::span<const Model* const> model_previews,
                               const GlbExportOptions& options, ArxLevelModelPreviewReport* report) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    constexpr std::string_view kDirection = "Level -> GLB";
    logGlbStart(kDirection);
    ArxReturnCode rc = validate();
    if (rc != ARX_OK) return logGlbFailure(kDirection, rc);
    if (!data_->validation.derived.referenced_bounds) return logGlbFailure(kDirection, ARX_LEVEL_NO_GEOMETRY);

    ArxLevelModelPreviewReport local_report{};
    std::vector<const ModelModules*> preview_data;
    preview_data.reserve(model_previews.size());
    for (const Model* model : model_previews) {
      if (model == nullptr) return logGlbFailure(kDirection, ARX_INVALID_DATA_POINTER);
      rc = model->validate();
      if (rc != ARX_OK) {
        ++local_report.skipped_invalid_models;
        continue;
      }
      preview_data.push_back(static_cast<const ModelModules*>(model->data_.get()));
    }
    std::vector<std::uint8_t> encoded;
    ArxLevelModelPreviewReport export_report{};
    rc = exportLevelToGlb(static_cast<const LevelModules&>(*data_),
                          *data_->validation.derived.referenced_bounds,
                          options,
                          preview_data,
                          &export_report,
                          encoded);
    local_report.mapped_models += export_report.mapped_models;
    local_report.previewed_entities += export_report.previewed_entities;
    local_report.skipped_anonymous_models += export_report.skipped_anonymous_models;
    local_report.skipped_unmappable_models += export_report.skipped_unmappable_models;
    local_report.skipped_duplicate_models += export_report.skipped_duplicate_models;
    if (rc != ARX_OK) return logGlbFailure(kDirection, rc);
    out = std::move(encoded);
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
    return ARX_OK;
  });
}

}  // namespace pistoris
