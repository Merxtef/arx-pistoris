// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation.h"
#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/bake.hpp"
#include "arx_pistoris/model/glb.hpp"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/native.h"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/texture.h"
#include "arx_pistoris/texture.hpp"

#include "api/c/animation/internal.h"
#include "api/c/internal.h"
#include "api/c/model/internal.h"
#include "api/c/native/internal.h"
#include "api/c/sound/internal.h"
#include "api/c/texture/internal.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

namespace {

bool validMaterialLibraries(const ArxObjMaterialLibraryView* libraries, std::size_t count) noexcept {
  if (!pistoris::c_api::valid(libraries, count)) return false;
  for (std::size_t index = 0; index < count; ++index) {
    if (!pistoris::c_api::valid(libraries[index].path) ||
        !pistoris::c_api::valid(libraries[index].data, libraries[index].size))
      return false;
  }
  return true;
}

std::vector<pistoris::ObjMaterialLibraryView> materialLibraryViews(const ArxObjMaterialLibraryView* libraries,
                                                                   std::size_t count) {
  std::vector<pistoris::ObjMaterialLibraryView> result;
  result.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    const char* data = libraries[index].data ? reinterpret_cast<const char*>(libraries[index].data) : "";
    result.push_back({pistoris::c_api::stringView(libraries[index].path), {data, libraries[index].size}});
  }
  return result;
}

ArxReturnCode publishObjText(const pistoris::ObjBundle& result, char** out_obj, char** out_mtl) {
  auto obj = std::make_unique<char[]>(result.text.size() + 1U);
  auto mtl = std::make_unique<char[]>(result.mtl.size() + 1U);
  if (!result.text.empty()) std::memcpy(obj.get(), result.text.data(), result.text.size());
  if (!result.mtl.empty()) std::memcpy(mtl.get(), result.mtl.data(), result.mtl.size());
  obj[result.text.size()] = '\0';
  mtl[result.mtl.size()] = '\0';
  *out_obj = obj.release();
  *out_mtl = mtl.release();
  return ARX_OK;
}

}  // namespace

ArxReturnCode arx_pistoris_model_create(ArxModel** out_model) noexcept {
  if (!out_model) return ARX_INVALID_DATA_POINTER;
  *out_model = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxModel>();
    *out_model = result.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_model_clone(const ArxModel* model, ArxModel** out_model) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!out_model) return ARX_INVALID_DATA_POINTER;
  *out_model = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxModel>();
    result->value = model->value;
    *out_model = result.release();
    return ARX_OK;
  });
}

void arx_pistoris_model_destroy(ArxModel* model) noexcept { delete model; }

ArxReturnCode arx_pistoris_model_reset(ArxModel* model) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    model->value.reset();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_model_import_native(const ArxFtl* native, ArxModel** out_model,
                                               ArxTextureSourcePaths** out_texture_source_paths) noexcept {
  if (!native) return ARX_INVALID_HANDLE;
  if (!out_model) return ARX_INVALID_DATA_POINTER;
  *out_model = nullptr;
  if (out_texture_source_paths) *out_texture_source_paths = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxModel>();
    std::unique_ptr<ArxTextureSourcePaths> paths;
    if (out_texture_source_paths) paths = std::make_unique<ArxTextureSourcePaths>();
    const ArxReturnCode rc =
        pistoris::Model::importNative(result->value, native->value, paths ? &paths->value : nullptr);
    if (rc != ARX_OK) return rc;
    *out_model = result.release();
    if (out_texture_source_paths) *out_texture_source_paths = paths.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_obj_material_library_paths(const uint8_t* obj_data, size_t obj_size,
                                                      ArxObjMaterialLibraryPaths** out_paths) noexcept {
  if (!obj_data || !out_paths) return ARX_INVALID_DATA_POINTER;
  *out_paths = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxObjMaterialLibraryPaths>();
    const std::string_view obj(reinterpret_cast<const char*>(obj_data), obj_size);
    const ArxReturnCode rc = pistoris::objMaterialLibraryPaths(obj, result->value);
    if (rc != ARX_OK) return rc;
    *out_paths = result.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_obj_material_library_paths_count(const ArxObjMaterialLibraryPaths* paths,
                                                            size_t* out_count) noexcept {
  if (!paths) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = paths->value.size();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_obj_material_library_paths_get(const ArxObjMaterialLibraryPaths* paths, size_t index,
                                                          ArxStringView* out_path) noexcept {
  if (!paths) return ARX_INVALID_HANDLE;
  if (!out_path) return ARX_INVALID_DATA_POINTER;
  *out_path = {};
  if (index >= paths->value.size()) return ARX_INDEX_OUT_OF_RANGE;
  *out_path = pistoris::c_api::view(paths->value[index]);
  return ARX_OK;
}

void arx_pistoris_obj_material_library_paths_destroy(ArxObjMaterialLibraryPaths* paths) noexcept { delete paths; }

ArxReturnCode arx_pistoris_model_import_obj(const uint8_t* obj_data, size_t obj_size,
                                            const ArxObjMaterialLibraryView* material_libraries,
                                            size_t material_library_count, ArxModel** out_model,
                                            ArxTextureSourcePaths** out_texture_source_paths) noexcept {
  if (!obj_data || !validMaterialLibraries(material_libraries, material_library_count) || !out_model)
    return ARX_INVALID_DATA_POINTER;
  *out_model = nullptr;
  if (out_texture_source_paths) *out_texture_source_paths = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    const std::vector<pistoris::ObjMaterialLibraryView> libraries =
        materialLibraryViews(material_libraries, material_library_count);
    auto result = std::make_unique<ArxModel>();
    std::unique_ptr<ArxTextureSourcePaths> paths;
    if (out_texture_source_paths) paths = std::make_unique<ArxTextureSourcePaths>();
    const std::string_view obj(reinterpret_cast<const char*>(obj_data), obj_size);
    const ArxReturnCode rc = pistoris::Model::importObj(result->value, obj, libraries, paths ? &paths->value : nullptr);
    if (rc != ARX_OK) return rc;
    *out_model = result.release();
    if (out_texture_source_paths) *out_texture_source_paths = paths.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_model_export_obj(const ArxModel* model, ArxStringView stem,
                                            const ArxObjExportOptions* options, char** out_obj, char** out_mtl,
                                            ArxObjTextureFiles** out_textures) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(stem) || !out_obj || !out_mtl) return ARX_INVALID_DATA_POINTER;
  *out_obj = nullptr;
  *out_mtl = nullptr;
  if (out_textures) *out_textures = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::ObjBundle result;
    const pistoris::ObjExportOptions cpp_options{.include_files = out_textures != nullptr &&
                                                                  (!options || options->include_files != 0)};
    const ArxReturnCode rc = model->value.exportObj(pistoris::c_api::stringView(stem), cpp_options, result);
    if (rc != ARX_OK) return rc;
    std::unique_ptr<ArxObjTextureFiles> textures;
    if (out_textures) {
      textures = std::make_unique<ArxObjTextureFiles>();
      textures->value = std::move(result.texture_files);
    }
    const ArxReturnCode publish_rc = publishObjText(result, out_obj, out_mtl);
    if (publish_rc != ARX_OK) return publish_rc;
    if (out_textures) *out_textures = textures.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_obj_texture_files_count(const ArxObjTextureFiles* files, size_t* out_count) noexcept {
  if (!files) return ARX_INVALID_HANDLE;
  if (!out_count) return ARX_INVALID_DATA_POINTER;
  *out_count = files->value.size();
  return ARX_OK;
}

ArxReturnCode arx_pistoris_obj_texture_files_get(const ArxObjTextureFiles* files, size_t index,
                                                 ArxObjTextureFile* out_file) noexcept {
  if (!files) return ARX_INVALID_HANDLE;
  if (!out_file) return ARX_INVALID_DATA_POINTER;
  *out_file = {};
  if (index >= files->value.size()) return ARX_INDEX_OUT_OF_RANGE;
  const pistoris::ObjTextureFile& file = files->value[index];
  out_file->source_texture = file.source_texture;
  out_file->path = pistoris::c_api::view(file.path);
  out_file->encoded_image = pistoris::c_api::view(file.encoded_image);
  return ARX_OK;
}

void arx_pistoris_obj_texture_files_destroy(ArxObjTextureFiles* files) noexcept { delete files; }

ArxReturnCode arx_pistoris_model_import_glb(const uint8_t* data, size_t size, const ArxModelGlbImportOptions* options,
                                            ArxModel** out_model, ArxAnimationList** out_animations,
                                            ArxAnimationConversionReport* report,
                                            ArxTextureSourcePaths** out_texture_source_paths,
                                            ArxAnimationSoundSourceReferences** out_sound_sources) noexcept {
  if (!data || !out_model) return ARX_INVALID_DATA_POINTER;
  if ((report || out_sound_sources) && !out_animations) return ARX_INVALID_OPTIONS;
  *out_model = nullptr;
  if (out_animations) *out_animations = nullptr;
  if (report) *report = {};
  if (out_texture_source_paths) *out_texture_source_paths = nullptr;
  if (out_sound_sources) *out_sound_sources = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxModel>();
    std::unique_ptr<ArxTextureSourcePaths> texture_paths;
    if (out_texture_source_paths) texture_paths = std::make_unique<ArxTextureSourcePaths>();
    pistoris::Model::GlbImportOptions cpp_options;
    if (options) cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
    if (!out_animations) {
      const ArxReturnCode rc = pistoris::Model::importGlb(result->value,
                                                          std::span<const std::uint8_t>(data, size),
                                                          cpp_options,
                                                          texture_paths ? &texture_paths->value : nullptr);
      if (rc != ARX_OK) return rc;
      *out_model = result.release();
      if (out_texture_source_paths) *out_texture_source_paths = texture_paths.release();
      return ARX_OK;
    }

    std::vector<std::unique_ptr<pistoris::Animation>> imported;
    std::unique_ptr<ArxAnimationSoundSourceReferences> sound_sources;
    if (out_sound_sources) sound_sources = std::make_unique<ArxAnimationSoundSourceReferences>();
    ArxAnimationConversionReport local_report{};
    const ArxReturnCode rc = pistoris::Model::importGlb(result->value,
                                                        imported,
                                                        std::span<const std::uint8_t>(data, size),
                                                        cpp_options,
                                                        report ? &local_report : nullptr,
                                                        texture_paths ? &texture_paths->value : nullptr,
                                                        sound_sources ? &sound_sources->value : nullptr);
    if (rc != ARX_OK) return rc;

    std::unique_ptr<ArxAnimationList> animations = pistoris::c_api::makeAnimationList(std::move(imported));
    *out_model = result.release();
    *out_animations = animations.release();
    if (report) *report = local_report;
    if (out_texture_source_paths) *out_texture_source_paths = texture_paths.release();
    if (out_sound_sources) *out_sound_sources = sound_sources.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_model_export_level_preview_glb(const ArxModel* model,
                                                          const ArxModelLevelPreviewGlbOptions* options,
                                                          uint8_t** out_data, size_t* out_size) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  if (options && (!pistoris::c_api::valid(options->class_path) || !pistoris::c_api::valid(options->asset_name)))
    return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    pistoris::Model::LevelPreviewGlbOptions cpp_options;
    if (options) {
      cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
      cpp_options.class_path = pistoris::c_api::stringView(options->class_path);
      cpp_options.asset_name = pistoris::c_api::stringView(options->asset_name);
    }
    std::vector<std::uint8_t> result;
    const ArxReturnCode rc = model->value.exportLevelPreviewGlb(result, cpp_options);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(result), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_model_export_glb(const ArxModel* model, const ArxAnimation* const* animations,
                                            size_t animation_count, const ArxModelGlbExportOptions* options,
                                            ArxAnimationConversionReport* report, uint8_t** out_data, size_t* out_size,
                                            ArxAnimationSoundFiles** out_sounds) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!pistoris::c_api::valid(animations, animation_count) || !out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  for (std::size_t index = 0; index < animation_count; ++index)
    if (!animations[index]) return ARX_INVALID_HANDLE;
  *out_data = nullptr;
  *out_size = 0;
  if (out_sounds) *out_sounds = nullptr;
  if (report) *report = {};
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    std::vector<const pistoris::Animation*> animation_views;
    animation_views.reserve(animation_count);
    for (std::size_t index = 0; index < animation_count; ++index) animation_views.push_back(&animations[index]->value);
    pistoris::Model::GlbExportOptions cpp_options;
    if (options) cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
    ArxAnimationConversionReport local_report{};
    if (out_sounds) {
      pistoris::ModelGlbBundle bundle;
      const ArxReturnCode rc =
          model->value.exportGlbBundle(animation_views, cpp_options, report ? &local_report : nullptr, bundle);
      if (rc != ARX_OK) return rc;
      auto sounds = std::make_unique<ArxAnimationSoundFiles>();
      sounds->value = std::move(bundle.sound_files);
      const ArxReturnCode publish_rc = pistoris::c_api::publishBytes(std::move(bundle.glb), out_data, out_size);
      if (publish_rc != ARX_OK) return publish_rc;
      *out_sounds = sounds.release();
    } else {
      std::vector<std::uint8_t> result;
      const ArxReturnCode rc =
          model->value.exportGlb(result, animation_views, cpp_options, report ? &local_report : nullptr);
      if (rc != ARX_OK) return rc;
      const ArxReturnCode publish_rc = pistoris::c_api::publishBytes(std::move(result), out_data, out_size);
      if (publish_rc != ARX_OK) return publish_rc;
    }
    if (report) *report = local_report;
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_model_bake_native(const ArxModel* model, const ArxNativeTextureBakeOptions* options,
                                             ArxFtl** out_native, ArxNativeTextureFiles** out_textures) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  if (!options) return ARX_INVALID_OPTIONS;
  if (!out_native) return ARX_INVALID_DATA_POINTER;
  *out_native = nullptr;
  if (out_textures) *out_textures = nullptr;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    const pistoris::NativeTextureBakeOptions cpp_options{out_textures && options->include_files != 0};
    pistoris::NativeModelBundle bundle;
    const ArxReturnCode rc = model->value.bakeNativeBundle(cpp_options, bundle);
    if (rc != ARX_OK) return rc;
    auto native = std::make_unique<ArxFtl>();
    native->value = std::move(bundle.ftl);
    std::unique_ptr<ArxNativeTextureFiles> textures;
    if (out_textures) {
      textures = std::make_unique<ArxNativeTextureFiles>();
      textures->value = std::move(bundle.texture_files);
    }
    *out_native = native.release();
    if (out_textures) *out_textures = textures.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_model_validate(const ArxModel* model) noexcept {
  if (!model) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return model->value.validate(); });
}

#define ARX_MODEL_VALIDATE(name, method)                                             \
  ArxReturnCode arx_pistoris_model_validate_##name(const ArxModel* model) noexcept { \
    if (!model) return ARX_INVALID_HANDLE;                                           \
    return pistoris::c_api::guard([&] { return model->value.method(); });            \
  }

ARX_MODEL_VALIDATE(mesh, validateMesh)
ARX_MODEL_VALIDATE(skeleton, validateSkeleton)
ARX_MODEL_VALIDATE(action_points, validateActionPoints)
ARX_MODEL_VALIDATE(selections, validateSelections)

#undef ARX_MODEL_VALIDATE

// NOLINTEND(readability-identifier-naming)
