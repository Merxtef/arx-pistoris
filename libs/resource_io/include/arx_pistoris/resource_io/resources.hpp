// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/resource_io/catalog.hpp"
#include "arx_pistoris/resource_io/document.hpp"
#include "arx_pistoris/resource_io/native_bundle.hpp"
#include "arx_pistoris/resource_io/output.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::resource_io {

struct LoadedModel {
  Model model;
  std::vector<Animation> animations;
};

struct ModelLoadOptions {
  Model::GlbImportOptions glb;
  NativeTextMode native_text_mode = NativeTextMode::kAuto;
};

struct AnimationLoadOptions {
  NativeTextMode native_text_mode = NativeTextMode::kAuto;
};

struct LevelLoadOptions {
  Level::GlbImportOptions glb;
  NativeTextMode native_text_mode = NativeTextMode::kAuto;
};

struct AmbianceLoadOptions {
  Ambiance::GlbImportOptions glb;
  NativeTextMode native_text_mode = NativeTextMode::kAuto;
};

struct CinematicLoadOptions {
  NativeTextMode native_text_mode = NativeTextMode::kAuto;
};

struct ModelWriteOptions {
  ResourceOutputOptions resource;
  std::optional<Model::GlbExportOptions> glb;
  std::optional<NativeTextMode> native_text_mode;
  std::optional<bool> compress;
};

struct AnimationWriteOptions {
  ResourceOutputOptions resource;
  NativeTextMode native_text_mode = NativeTextMode::kAuto;
};

struct LevelWriteOptions {
  ResourceOutputOptions resource;
  std::optional<Level::GlbExportOptions> glb;
  std::optional<NativeTextMode> native_text_mode;
  std::optional<bool> reconstruct_quads;
  std::optional<bool> compress;
  std::optional<bool> embed_lighting;
  std::optional<std::string> signer;
};

struct AmbianceWriteOptions {
  ResourceOutputOptions resource;
  std::optional<Ambiance::GlbExportOptions> glb;
  std::optional<NativeTextMode> native_text_mode;
};

struct CinematicWriteOptions {
  ResourceOutputOptions resource;
  std::optional<NativeTextMode> native_text_mode;
  std::optional<ArxImageFormat> illustration_format;
};

class Resources {
 public:
  Resources() = default;
  explicit Resources(ResourceMounts mounts) : mounts_(std::move(mounts)) {}

  [[nodiscard]] ResourceMounts& mounts() noexcept { return mounts_; }
  [[nodiscard]] const ResourceMounts& mounts() const noexcept { return mounts_; }
  [[nodiscard]] ResourceIoResult<ResourceCatalog> scanCatalog(const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceDocument> readDocument(
      const paths::ResourceSelector& resource, const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceDocument> readDocument(
      std::string_view logical_path, const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceDocument> readDocumentFile(const std::filesystem::path& path) const noexcept;
  [[nodiscard]] ResourceIoResult<ModelNativeBundle> loadModelNativeBundle(
      const ResourceDocument& model, std::span<const ResourceDocument> animations = {},
      const NativeBundleLoadOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<AnimationNativeBundle> loadAnimationNativeBundle(
      const ResourceDocument& animation, const NativeBundleLoadOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<LevelNativeBundle> loadLevelNativeBundle(
      const ResourceDocument& primary, const ResourceDocument* lighting = nullptr,
      const ResourceDocument* scene = nullptr, const NativeBundleLoadOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<AmbianceNativeBundle> loadAmbianceNativeBundle(
      const ResourceDocument& ambiance, const NativeBundleLoadOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<CinematicNativeBundle> loadCinematicNativeBundle(
      const ResourceDocument& cinematic, const NativeBundleLoadOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<LoadedModel> loadModel(const ResourceDocument& document,
                                                        const ModelLoadOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<LoadedModel> loadModel(const paths::ResourceSelector& resource,
                                                        const ModelLoadOptions& options = {},
                                                        const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<LoadedModel> loadModel(paths::ModelPathView resource,
                                                        const ModelLoadOptions& options = {},
                                                        const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<LoadedModel> loadModel(std::string_view logical_path,
                                                        const ModelLoadOptions& options = {},
                                                        const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<LoadedModel> loadModelFile(const std::filesystem::path& path,
                                                            const ModelLoadOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Animation> loadAnimation(const paths::ResourceSelector& resource,
                                                          const AnimationLoadOptions& options = {},
                                                          const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Animation> loadAnimation(paths::AnimationPathView resource,
                                                          const AnimationLoadOptions& options = {},
                                                          const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Animation> loadAnimation(std::string_view logical_path,
                                                          const AnimationLoadOptions& options = {},
                                                          const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Animation> loadAnimationFile(const std::filesystem::path& path,
                                                              const AnimationLoadOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Animation> loadAnimation(const ResourceDocument& document,
                                                          const AnimationLoadOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Level> loadLevel(std::uint32_t level, const LevelLoadOptions& options = {},
                                                  const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Level> loadLevel(const paths::ResourceSelector& resource,
                                                  const LevelLoadOptions& options = {},
                                                  const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Level> loadLevel(std::string_view logical_path, const LevelLoadOptions& options = {},
                                                  const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Level> loadLevelFile(
      const std::filesystem::path& fts_path, const std::optional<std::filesystem::path>& llf_path = std::nullopt,
      const std::optional<std::filesystem::path>& dlf_path = std::nullopt,
      const LevelLoadOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Level> loadLevel(const ResourceDocument& primary,
                                                  const ResourceDocument* lighting = nullptr,
                                                  const ResourceDocument* scene = nullptr,
                                                  const LevelLoadOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Ambiance> loadAmbiance(const paths::ResourceSelector& resource,
                                                        const AmbianceLoadOptions& options = {},
                                                        const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Ambiance> loadAmbiance(paths::AmbiancePathView resource,
                                                        const AmbianceLoadOptions& options = {},
                                                        const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Ambiance> loadAmbiance(std::string_view logical_path,
                                                        const AmbianceLoadOptions& options = {},
                                                        const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Ambiance> loadAmbianceFile(const std::filesystem::path& path,
                                                            const AmbianceLoadOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Ambiance> loadAmbiance(const ResourceDocument& document,
                                                        const AmbianceLoadOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Cinematic> loadCinematic(const paths::ResourceSelector& resource,
                                                          const CinematicLoadOptions& options = {},
                                                          const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Cinematic> loadCinematic(paths::CinematicPathView resource,
                                                          const CinematicLoadOptions& options = {},
                                                          const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Cinematic> loadCinematic(std::string_view logical_path,
                                                          const CinematicLoadOptions& options = {},
                                                          const ResourceLookupOptions& lookup = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Cinematic> loadCinematicFile(const std::filesystem::path& path,
                                                              const CinematicLoadOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<Cinematic> loadCinematic(const ResourceDocument& document,
                                                          const CinematicLoadOptions& options = {}) const noexcept;

  [[nodiscard]] ResourceIoResult<ResourceWritePlan> prepareWrite(
      ResourceOutputs outputs, const ResourceWriteOptions& options = {}) const noexcept;

  [[nodiscard]] ResourceIoResult<ResourceOutputs> prepareModelOutputs(
      const Model& model, std::string_view logical_path, const ModelWriteOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceOutputs> prepareModelOutputs(
      const LoadedModel& loaded, std::string_view logical_path, const ModelWriteOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceOutputs> prepareModelFileOutputs(
      const Model& model, const std::filesystem::path& path, const ModelWriteOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceOutputs> prepareModelFileOutputs(
      const LoadedModel& loaded, const std::filesystem::path& path,
      const ModelWriteOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> writeModel(
      const Model& model, std::string_view logical_path, const ModelWriteOptions& options = {},
      const ResourceWriteOptions& write_options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> writeModel(
      const LoadedModel& loaded, std::string_view logical_path, const ModelWriteOptions& options = {},
      const ResourceWriteOptions& write_options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> writeModelFile(
      const Model& model, const std::filesystem::path& path, const ModelWriteOptions& options = {},
      const ResourceWriteOptions& write_options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> writeModelFile(
      const LoadedModel& loaded, const std::filesystem::path& path, const ModelWriteOptions& options = {},
      const ResourceWriteOptions& write_options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceOutputs> prepareAnimationOutputs(
      const Animation& animation, std::string_view logical_path,
      const AnimationWriteOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceOutputs> prepareAnimationFileOutputs(
      const Animation& animation, const std::filesystem::path& path,
      const AnimationWriteOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> writeAnimation(
      const Animation& animation, std::string_view logical_path, const AnimationWriteOptions& options = {},
      const ResourceWriteOptions& write_options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> writeAnimationFile(
      const Animation& animation, const std::filesystem::path& path, const AnimationWriteOptions& options = {},
      const ResourceWriteOptions& write_options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceOutputs> prepareLevelOutputs(
      const Level& level, std::string_view logical_path, const LevelWriteOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceOutputs> prepareLevelOutputs(
      const Level& level, std::string_view logical_path, std::optional<std::string_view> llf_path,
      std::optional<std::string_view> dlf_path, std::optional<std::uint32_t> level_index = std::nullopt,
      const LevelWriteOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceOutputs> prepareLevelFileOutputs(
      const Level& level, const std::filesystem::path& primary_path,
      const std::optional<std::filesystem::path>& llf_path = std::nullopt,
      const std::optional<std::filesystem::path>& dlf_path = std::nullopt,
      std::optional<std::uint32_t> level_index = std::nullopt, const LevelWriteOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> writeLevel(
      const Level& level, std::string_view logical_path, const LevelWriteOptions& options = {},
      const ResourceWriteOptions& write_options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> writeLevel(
      const Level& level, std::string_view logical_path, std::optional<std::string_view> llf_path,
      std::optional<std::string_view> dlf_path, std::optional<std::uint32_t> level_index = std::nullopt,
      const LevelWriteOptions& options = {}, const ResourceWriteOptions& write_options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> writeLevelFile(
      const Level& level, const std::filesystem::path& primary_path,
      const std::optional<std::filesystem::path>& llf_path = std::nullopt,
      const std::optional<std::filesystem::path>& dlf_path = std::nullopt,
      std::optional<std::uint32_t> level_index = std::nullopt, const LevelWriteOptions& options = {},
      const ResourceWriteOptions& write_options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceOutputs> prepareAmbianceOutputs(
      const Ambiance& ambiance, std::string_view logical_path, const AmbianceWriteOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceOutputs> prepareAmbianceFileOutputs(
      const Ambiance& ambiance, const std::filesystem::path& path,
      const AmbianceWriteOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> writeAmbiance(
      const Ambiance& ambiance, std::string_view logical_path, const AmbianceWriteOptions& options = {},
      const ResourceWriteOptions& write_options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> writeAmbianceFile(
      const Ambiance& ambiance, const std::filesystem::path& path, const AmbianceWriteOptions& options = {},
      const ResourceWriteOptions& write_options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceOutputs> prepareCinematicOutputs(
      const Cinematic& cinematic, std::string_view logical_path,
      const CinematicWriteOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceOutputs> prepareCinematicFileOutputs(
      const Cinematic& cinematic, const std::filesystem::path& path,
      const CinematicWriteOptions& options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> writeCinematic(
      const Cinematic& cinematic, std::string_view logical_path, const CinematicWriteOptions& options = {},
      const ResourceWriteOptions& write_options = {}) const noexcept;
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> writeCinematicFile(
      const Cinematic& cinematic, const std::filesystem::path& path, const CinematicWriteOptions& options = {},
      const ResourceWriteOptions& write_options = {}) const noexcept;

 private:
  [[nodiscard]] ResourceIoResult<ResourceWriteReport> writeOutputs(ResourceOutputs outputs,
                                                                   const ResourceWriteOptions& options) const noexcept;

  ResourceMounts mounts_;
};

}  // namespace pistoris::resource_io
