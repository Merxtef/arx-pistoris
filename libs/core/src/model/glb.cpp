// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/model/glb.hpp"

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/location.hpp"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/location.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "animation/data.h"
#include "api/result_failure.h"
#include "api/status_boundary.h"
#include "external/glb/failure.h"
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

}  // namespace

GlbResult<Model> Model::importGlb(std::span<const std::uint8_t> data) noexcept {
  return importGlb(data, GlbImportOptions{}, nullptr);
}

GlbResult<Model> Model::importGlb(std::span<const std::uint8_t> data, const GlbImportOptions& options,
                                  std::vector<std::string>* texture_source_paths) noexcept {
  constexpr std::string_view kOperation = "GLB -> Model conversion";
  return api_detail::glbBoundary(
      [&]() -> GlbResult<Model> {
        constexpr std::string_view kDirection = "GLB -> Model";
        logGlbStart(kDirection);
        Model result;
        std::vector<std::string> source_paths;
        glb::Failure failure;
        ArxReturnCode rc = importModelFromGlb(data,
                                              options,
                                              static_cast<ModelModules&>(*result.data_),
                                              nullptr,
                                              nullptr,
                                              texture_source_paths ? &source_paths : nullptr,
                                              nullptr,
                                              &failure);
        if (rc != ARX_OK) {
          return api_detail::glbFailure<Model>(rc, std::move(failure.location), std::move(failure.detail), kOperation);
        }
        log(ARX_LOG_INFO,
            "=== {} conversion completed: {} vertices, {} faces, {} bones ===",
            kDirection,
            result.data_->geometry.vertices.size(),
            result.data_->geometry.faces.size(),
            result.data_->skeleton.bones.size());
        if (texture_source_paths) *texture_source_paths = std::move(source_paths);
        return GlbResult<Model>::success(std::move(result));
      },
      kOperation);
}

GlbResult<ModelGlbImport> Model::importGlbWithAnimations(std::span<const std::uint8_t> data,
                                                         ArxAnimationConversionReport* report) noexcept {
  return importGlbWithAnimations(data, GlbImportOptions{}, report, nullptr);
}

GlbResult<ModelGlbImport> Model::importGlbWithAnimations(
    std::span<const std::uint8_t> data, const GlbImportOptions& options, ArxAnimationConversionReport* report,
    std::vector<std::string>* texture_source_paths,
    std::vector<AnimationSoundSourceReference>* sound_sources) noexcept {
  constexpr std::string_view kOperation = "GLB -> Model + Animation conversion";
  return api_detail::glbBoundary(
      [&]() -> GlbResult<ModelGlbImport> {
        constexpr std::string_view kDirection = "GLB -> Model + Animation";
        logGlbStart(kDirection);
        Model result;
        std::vector<AnimationModules> imported;
        std::vector<std::string> source_paths;
        std::vector<AnimationSoundSourceReference> animation_sources;
        ArxAnimationConversionReport local_report{};
        glb::Failure failure;
        ArxReturnCode rc = importModelFromGlb(data,
                                              options,
                                              static_cast<ModelModules&>(*result.data_),
                                              &imported,
                                              &local_report,
                                              texture_source_paths ? &source_paths : nullptr,
                                              sound_sources ? &animation_sources : nullptr,
                                              &failure);
        if (rc != ARX_OK) {
          return api_detail::glbFailure<ModelGlbImport>(
              rc, std::move(failure.location), std::move(failure.detail), kOperation);
        }
        std::vector<Animation> animations;
        animations.reserve(imported.size());
        for (AnimationModules& modules : imported) {
          Animation animation;
          static_cast<AnimationModules&>(*animation.data_) = std::move(modules);
          animations.push_back(std::move(animation));
        }
        log(ARX_LOG_INFO,
            "=== {} conversion completed: {} vertices, {} faces, {} bones, {} animation(s) ===",
            kDirection,
            result.data_->geometry.vertices.size(),
            result.data_->geometry.faces.size(),
            result.data_->skeleton.bones.size(),
            animations.size());
        if (report) *report = local_report;
        if (texture_source_paths) *texture_source_paths = std::move(source_paths);
        if (sound_sources) *sound_sources = std::move(animation_sources);
        return GlbResult<ModelGlbImport>::success(
            ModelGlbImport{.model = std::move(result), .animations = std::move(animations)});
      },
      kOperation);
}

ModelGlbExportResult<std::vector<std::uint8_t>> Model::exportGlb() const noexcept {
  return exportGlb(GlbExportOptions{});
}

ModelGlbExportResult<std::vector<std::uint8_t>> Model::exportGlb(const GlbExportOptions& options) const noexcept {
  return exportGlb(std::span<const Animation* const>{}, options, nullptr);
}

ModelGlbExportResult<std::vector<std::uint8_t>> Model::exportGlb(std::span<const Animation* const> animations,
                                                                 ArxAnimationConversionReport* report) const noexcept {
  return exportGlb(animations, GlbExportOptions{}, report);
}

ModelGlbExportResult<std::vector<std::uint8_t>> Model::exportGlb(std::span<const Animation* const> animations,
                                                                 const GlbExportOptions& options,
                                                                 ArxAnimationConversionReport* report) const noexcept {
  const std::string_view operation =
      animations.empty() ? "Model -> GLB conversion" : "Model + Animation -> GLB conversion";
  if (!data_)
    return api_detail::modelGlbExportFailure<std::vector<std::uint8_t>>(
        ARX_INVALID_STATE,
        ModelGlbExportLocation{api_detail::resourceLocation(resourcePath(), ModelElement::kResource)},
        {},
        operation);
  return api_detail::modelGlbExportBoundary(
      ModelGlbExportLocation{api_detail::resourceLocation(resourcePath(), ModelElement::kResource)},
      [&]() -> ModelGlbExportResult<std::vector<std::uint8_t>> {
        auto validation = validate();
        if (!validation)
          return api_detail::remapFailure<std::vector<std::uint8_t>, ModelGlbExportLocation>(
              std::move(validation), [](const ModelLocation& location) { return ModelGlbExportLocation{location}; });
        std::vector<std::uint8_t> out;
        ModelGlbExportLocation failure_location{api_detail::resourceLocation(resourcePath(), ModelElement::kResource)};
        const ArxReturnCode rc = exportGlbInternal(animations, options, report, out, nullptr, &failure_location);
        if (rc != ARX_OK)
          return api_detail::modelGlbExportFailure<std::vector<std::uint8_t>>(
              rc, std::move(failure_location), {}, operation);
        return ModelGlbExportResult<std::vector<std::uint8_t>>::success(std::move(out));
      },
      operation);
}

ModelGlbExportResult<ModelGlbBundle> Model::exportGlbBundle(std::span<const Animation* const> animations,
                                                            const GlbExportOptions& options,
                                                            ArxAnimationConversionReport* report) const noexcept {
  const std::string_view operation =
      animations.empty() ? "Model -> GLB conversion" : "Model + Animation -> GLB conversion";
  if (!data_)
    return api_detail::modelGlbExportFailure<ModelGlbBundle>(
        ARX_INVALID_STATE,
        ModelGlbExportLocation{api_detail::resourceLocation(resourcePath(), ModelElement::kResource)},
        {},
        operation);
  return api_detail::modelGlbExportBoundary(
      ModelGlbExportLocation{api_detail::resourceLocation(resourcePath(), ModelElement::kResource)},
      [&]() -> ModelGlbExportResult<ModelGlbBundle> {
        auto validation = validate();
        if (!validation)
          return api_detail::remapFailure<ModelGlbBundle, ModelGlbExportLocation>(
              std::move(validation), [](const ModelLocation& location) { return ModelGlbExportLocation{location}; });
        ModelGlbBundle result;
        ModelGlbExportLocation failure_location{api_detail::resourceLocation(resourcePath(), ModelElement::kResource)};
        const ArxReturnCode rc =
            exportGlbInternal(animations, options, report, result.glb, &result.sound_files, &failure_location);
        if (rc != ARX_OK)
          return api_detail::modelGlbExportFailure<ModelGlbBundle>(rc, std::move(failure_location), {}, operation);
        return ModelGlbExportResult<ModelGlbBundle>::success(std::move(result));
      },
      operation);
}

ArxReturnCode Model::exportGlbInternal(std::span<const Animation* const> animations, const GlbExportOptions& options,
                                       ArxAnimationConversionReport* report, std::vector<std::uint8_t>& out,
                                       std::vector<AnimationSoundFile>* sound_files,
                                       ModelGlbExportLocation* failure_location) const {
  constexpr std::string_view kDirection = "Model -> GLB";
  logGlbStart(kDirection);
  ArxReturnCode rc = ARX_OK;
  std::vector<const AnimationModules*> animation_data;
  std::vector<std::size_t> source_indices;
  animation_data.reserve(animations.size());
  source_indices.reserve(animations.size());
  ArxAnimationConversionReport local_report{};
  for (std::size_t index = 0; index < animations.size(); ++index) {
    const Animation* animation = animations[index];
    if (animation == nullptr) {
      if (failure_location)
        *failure_location = AnimationLocation{
            api_detail::resourceLocation({}, AnimationElement::kResource, kNoElementIndex, kNoElementIndex, index)};
      return ARX_INVALID_DATA_POINTER;
    }
    rc = animation->validate().code();
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
  if (rc != ARX_OK) return rc;
  log(ARX_LOG_INFO,
      "=== {} conversion completed: {} vertices, {} faces, {} animation(s) converted, {} skipped ===",
      kDirection,
      data_->geometry.vertices.size(),
      data_->geometry.faces.size(),
      local_report.converted,
      local_report.skipped);
  if (sound_files) {
    for (AnimationSoundFile& file : files) {
      if (file.animation_index >= source_indices.size()) return ARX_INTERNAL_ERROR;
      file.animation_index = source_indices[file.animation_index];
    }
    *sound_files = std::move(files);
  }
  out = std::move(encoded);
  if (report) *report = local_report;
  return ARX_OK;
}

ModelResult<std::vector<std::uint8_t>> Model::exportLevelPreviewGlb() const noexcept {
  return exportLevelPreviewGlb(LevelPreviewGlbOptions{});
}

ModelResult<std::vector<std::uint8_t>> Model::exportLevelPreviewGlb(
    const LevelPreviewGlbOptions& options) const noexcept {
  constexpr std::string_view kOperation = "Model -> Level preview GLB conversion";
  if (!data_)
    return api_detail::modelFailure<std::vector<std::uint8_t>>(
        ARX_INVALID_STATE, api_detail::resourceLocation(resourcePath(), ModelElement::kResource), {}, kOperation);
  return api_detail::modelBoundary(
      resourcePath(),
      [&]() -> ModelResult<std::vector<std::uint8_t>> {
        constexpr std::string_view kDirection = "Model -> Level preview GLB";
        logGlbStart(kDirection);
        auto validation = validate();
        if (!validation) return std::move(validation).propagate<std::vector<std::uint8_t>>();
        std::vector<std::uint8_t> encoded;
        const ArxReturnCode rc =
            exportModelLevelPreviewToGlb(static_cast<const ModelModules&>(*data_), options, encoded);
        if (rc != ARX_OK)
          return api_detail::modelFailure<std::vector<std::uint8_t>>(
              rc, api_detail::resourceLocation(resourcePath(), ModelElement::kResource), {}, kOperation);
        log(ARX_LOG_INFO,
            "=== {} conversion completed: {} vertices, {} faces ===",
            kDirection,
            data_->geometry.vertices.size(),
            data_->geometry.faces.size());
        return ModelResult<std::vector<std::uint8_t>>::success(std::move(encoded));
      },
      kOperation);
}

}  // namespace pistoris
