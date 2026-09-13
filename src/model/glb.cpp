// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/model/glb.hpp"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "animation/data.h"
#include "api/status_boundary.h"
#include "external/glb/model/api.h"
#include "model/data.h"
#include "utils/log.h"

#include <cstddef>
#include <cstdint>
#include <memory>
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

ArxReturnCode Model::importGlb(Model& out, std::span<const std::uint8_t> data) noexcept {
  return importGlb(out, data, GlbImportOptions{}, nullptr);
}

ArxReturnCode Model::importGlb(Model& out, std::span<const std::uint8_t> data, const GlbImportOptions& options,
                               std::vector<std::string>* texture_source_paths) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    constexpr std::string_view kDirection = "GLB -> Model";
    logGlbStart(kDirection);
    Model result;
    std::vector<std::string> source_paths;
    ArxReturnCode rc = importModelFromGlb(data,
                                          options,
                                          static_cast<ModelModules&>(*result.data_),
                                          nullptr,
                                          nullptr,
                                          texture_source_paths ? &source_paths : nullptr);
    if (rc != ARX_OK) return logGlbFailure(kDirection, rc);
    log(ARX_LOG_INFO,
        "=== {} conversion completed: {} vertices, {} faces, {} bones ===",
        kDirection,
        result.data_->geometry.vertices.size(),
        result.data_->geometry.faces.size(),
        result.data_->skeleton.bones.size());
    out.swap(result);
    if (texture_source_paths) *texture_source_paths = std::move(source_paths);
    return ARX_OK;
  });
}

ArxReturnCode Model::importGlb(Model& out, std::vector<std::unique_ptr<Animation>>& out_animations,
                               std::span<const std::uint8_t> data, ArxAnimationConversionReport* report) noexcept {
  return importGlb(out, out_animations, data, GlbImportOptions{}, report, nullptr);
}

ArxReturnCode Model::importGlb(Model& out, std::vector<std::unique_ptr<Animation>>& out_animations,
                               std::span<const std::uint8_t> data, const GlbImportOptions& options,
                               ArxAnimationConversionReport* report, std::vector<std::string>* texture_source_paths,
                               std::vector<AnimationSoundSourceReference>* sound_sources) noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    constexpr std::string_view kDirection = "GLB -> Model + Animation";
    logGlbStart(kDirection);
    Model result;
    std::vector<AnimationModules> imported;
    std::vector<std::string> source_paths;
    std::vector<AnimationSoundSourceReference> animation_sources;
    ArxAnimationConversionReport local_report{};
    ArxReturnCode rc = importModelFromGlb(data,
                                          options,
                                          static_cast<ModelModules&>(*result.data_),
                                          &imported,
                                          &local_report,
                                          texture_source_paths ? &source_paths : nullptr,
                                          sound_sources ? &animation_sources : nullptr);
    if (rc != ARX_OK) return logGlbFailure(kDirection, rc);
    std::vector<std::unique_ptr<Animation>> animations;
    animations.reserve(imported.size());
    for (AnimationModules& modules : imported) {
      auto animation = std::make_unique<Animation>();
      static_cast<AnimationModules&>(*animation->data_) = std::move(modules);
      animations.push_back(std::move(animation));
    }
    log(ARX_LOG_INFO,
        "=== {} conversion completed: {} vertices, {} faces, {} bones, {} animation(s) ===",
        kDirection,
        result.data_->geometry.vertices.size(),
        result.data_->geometry.faces.size(),
        result.data_->skeleton.bones.size(),
        animations.size());
    out.swap(result);
    out_animations = std::move(animations);
    if (report) *report = local_report;
    if (texture_source_paths) *texture_source_paths = std::move(source_paths);
    if (sound_sources) *sound_sources = std::move(animation_sources);
    return ARX_OK;
  });
}

ArxReturnCode Model::exportGlb(std::vector<std::uint8_t>& out) const noexcept {
  return exportGlb(out, GlbExportOptions{});
}

ArxReturnCode Model::exportGlb(std::vector<std::uint8_t>& out, const GlbExportOptions& options) const noexcept {
  return exportGlb(out, std::span<const Animation* const>{}, options, nullptr);
}

ArxReturnCode Model::exportGlb(std::vector<std::uint8_t>& out, std::span<const Animation* const> animations,
                               ArxAnimationConversionReport* report) const noexcept {
  return exportGlb(out, animations, GlbExportOptions{}, report);
}

ArxReturnCode Model::exportGlb(std::vector<std::uint8_t>& out, std::span<const Animation* const> animations,
                               const GlbExportOptions& options, ArxAnimationConversionReport* report) const noexcept {
  return api_detail::statusBoundary(
      [&]() -> ArxReturnCode { return exportGlbInternal(animations, options, report, out, nullptr); });
}

ArxReturnCode Model::exportGlbBundle(std::span<const Animation* const> animations, const GlbExportOptions& options,
                                     ArxAnimationConversionReport* report, ModelGlbBundle& out) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    ModelGlbBundle result;
    const ArxReturnCode rc = exportGlbInternal(animations, options, report, result.glb, &result.sound_files);
    if (rc != ARX_OK) return rc;
    out = std::move(result);
    return ARX_OK;
  });
}

ArxReturnCode Model::exportGlbInternal(std::span<const Animation* const> animations, const GlbExportOptions& options,
                                       ArxAnimationConversionReport* report, std::vector<std::uint8_t>& out,
                                       std::vector<AnimationSoundFile>* sound_files) const {
  constexpr std::string_view kDirection = "Model -> GLB";
  logGlbStart(kDirection);
  ArxReturnCode rc = validate();
  if (rc != ARX_OK) return logGlbFailure(kDirection, rc);
  std::vector<const AnimationModules*> animation_data;
  std::vector<std::size_t> source_indices;
  animation_data.reserve(animations.size());
  source_indices.reserve(animations.size());
  ArxAnimationConversionReport local_report{};
  for (std::size_t index = 0; index < animations.size(); ++index) {
    const Animation* animation = animations[index];
    if (animation == nullptr) return logGlbFailure(kDirection, ARX_INVALID_DATA_POINTER);
    rc = animation->validate();
    if (rc != ARX_OK) {
      ++local_report.skipped;
      log(ARX_LOG_WARN, "Model -> GLB: animation '{}' skipped with code {}", animation->name(), rc);
      continue;
    }
    animation_data.push_back(animation->data_.get());
    source_indices.push_back(index);
  }
  std::vector<std::uint8_t> encoded;
  std::vector<AnimationSoundFile> files;
  ArxAnimationConversionReport conversion_report{};
  rc = exportModelToGlb(static_cast<const ModelModules&>(*data_),
                        options,
                        animation_data,
                        &conversion_report,
                        encoded,
                        sound_files ? &files : nullptr);
  local_report.converted += conversion_report.converted;
  local_report.skipped += conversion_report.skipped;
  if (rc != ARX_OK) return logGlbFailure(kDirection, rc);
  log(ARX_LOG_INFO,
      "=== {} conversion completed: {} vertices, {} faces, {} animation(s) converted, {} skipped ===",
      kDirection,
      data_->geometry.vertices.size(),
      data_->geometry.faces.size(),
      local_report.converted,
      local_report.skipped);
  if (sound_files) {
    for (AnimationSoundFile& file : files) {
      if (file.animation_index >= source_indices.size()) return logGlbFailure(kDirection, ARX_INTERNAL_ERROR);
      file.animation_index = source_indices[file.animation_index];
    }
    *sound_files = std::move(files);
  }
  out = std::move(encoded);
  if (report) *report = local_report;
  return ARX_OK;
}

ArxReturnCode Model::exportLevelPreviewGlb(std::vector<std::uint8_t>& out) const noexcept {
  return exportLevelPreviewGlb(out, LevelPreviewGlbOptions{});
}

ArxReturnCode Model::exportLevelPreviewGlb(std::vector<std::uint8_t>& out,
                                           const LevelPreviewGlbOptions& options) const noexcept {
  return api_detail::statusBoundary([&]() -> ArxReturnCode {
    constexpr std::string_view kDirection = "Model -> Level preview GLB";
    logGlbStart(kDirection);
    ArxReturnCode rc = validate();
    if (rc != ARX_OK) return logGlbFailure(kDirection, rc);
    std::vector<std::uint8_t> encoded;
    rc = exportModelLevelPreviewToGlb(static_cast<const ModelModules&>(*data_), options, encoded);
    if (rc != ARX_OK) return logGlbFailure(kDirection, rc);
    log(ARX_LOG_INFO,
        "=== {} conversion completed: {} vertices, {} faces ===",
        kDirection,
        data_->geometry.vertices.size(),
        data_->geometry.faces.size());
    out = std::move(encoded);
    return ARX_OK;
  });
}

}  // namespace pistoris
