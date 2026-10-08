// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation.h"
#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/bake.hpp"
#include "arx_pistoris/model/glb.hpp"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/native.h"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/text.h"
#include "arx_pistoris/sound.h"
#include "arx_pistoris/texture.h"

#include "api/c/animation/internal.h"
#include "api/c/internal.h"
#include "api/c/model/internal.h"
#include "api/c/native/internal.h"
#include "api/c/native/text_internal.h"
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

ArxReturnCode arx_pistoris_model_create(ArxModel** out_model, ArxError* error) noexcept {
  if (!out_model) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_model = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxModel>();
    *out_model = result.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_model_clone(const ArxModel* model, ArxModel** out_model, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_model) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_model = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxModel>();
    result->value = model->value;
    *out_model = result.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

void arx_pistoris_model_destroy(ArxModel* model) noexcept { delete model; }

ArxReturnCode arx_pistoris_model_reset(ArxModel* model, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  return pistoris::c_api::publish(model->value.reset(), error);
}

ArxReturnCode arx_pistoris_model_import_native(const ArxFtl* native, ArxModel** out_model,
                                               ArxTextureSourcePaths** out_texture_source_paths,
                                               ArxNativeTextMode text_mode, ArxError* error) noexcept {
  if (!native) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::validNativeTextMode(text_mode)) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if (!out_model) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_model = nullptr;
  if (out_texture_source_paths) *out_texture_source_paths = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxModel>();
    std::unique_ptr<ArxTextureSourcePaths> paths;
    if (out_texture_source_paths) paths = std::make_unique<ArxTextureSourcePaths>();
    auto imported = pistoris::Model::importNative(
        native->value, paths ? &paths->value : nullptr, pistoris::c_api::nativeTextMode(text_mode));
    if (!imported) return pistoris::c_api::publish(imported, error);
    result->value = std::move(*imported);
    *out_model = result.release();
    if (out_texture_source_paths) *out_texture_source_paths = paths.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_obj_material_library_paths(const uint8_t* obj_data, size_t obj_size,
                                                      ArxObjMaterialLibraryPaths** out_paths,
                                                      ArxError* error) noexcept {
  if (!obj_data || !out_paths) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_paths = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxObjMaterialLibraryPaths>();
    const std::string_view obj(reinterpret_cast<const char*>(obj_data), obj_size);
    auto paths = pistoris::objMaterialLibraryPaths(obj);
    if (!paths) return pistoris::c_api::publish(paths, error);
    result->value = std::move(*paths);
    *out_paths = result.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_obj_material_library_paths_count(const ArxObjMaterialLibraryPaths* paths, size_t* out_count,
                                                            ArxError* error) noexcept {
  if (!paths) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_count = paths->value.size();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_obj_material_library_paths_get(const ArxObjMaterialLibraryPaths* paths, size_t index,
                                                          ArxStringView* out_path, ArxError* error) noexcept {
  if (!paths) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_path) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_path = {};
  if (index >= paths->value.size()) return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  *out_path = pistoris::c_api::view(paths->value[index]);
  return pistoris::c_api::publishCode(ARX_OK, error);
}

void arx_pistoris_obj_material_library_paths_destroy(ArxObjMaterialLibraryPaths* paths) noexcept { delete paths; }

ArxReturnCode arx_pistoris_model_import_obj(const uint8_t* obj_data, size_t obj_size,
                                            const ArxObjMaterialLibraryView* material_libraries,
                                            size_t material_library_count, ArxModel** out_model,
                                            ArxTextureSourcePaths** out_texture_source_paths,
                                            ArxError* error) noexcept {
  if (!obj_data || !validMaterialLibraries(material_libraries, material_library_count) || !out_model)
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_model = nullptr;
  if (out_texture_source_paths) *out_texture_source_paths = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    const std::vector<pistoris::ObjMaterialLibraryView> libraries =
        materialLibraryViews(material_libraries, material_library_count);
    auto result = std::make_unique<ArxModel>();
    std::unique_ptr<ArxTextureSourcePaths> paths;
    if (out_texture_source_paths) paths = std::make_unique<ArxTextureSourcePaths>();
    const std::string_view obj(reinterpret_cast<const char*>(obj_data), obj_size);
    auto imported = pistoris::Model::importObj(obj, libraries, paths ? &paths->value : nullptr);
    if (!imported) return pistoris::c_api::publish(imported, error);
    result->value = std::move(*imported);
    *out_model = result.release();
    if (out_texture_source_paths) *out_texture_source_paths = paths.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_model_export_obj(const ArxModel* model, ArxStringView stem,
                                            const ArxObjExportOptions* options, char** out_obj, char** out_mtl,
                                            ArxObjTextureFiles** out_textures, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(stem) || !out_obj || !out_mtl)
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_obj = nullptr;
  *out_mtl = nullptr;
  if (out_textures) *out_textures = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    const pistoris::ObjExportOptions cpp_options{.include_files = out_textures != nullptr &&
                                                                  (!options || options->include_files != 0)};
    auto exported = model->value.exportObj(pistoris::c_api::stringView(stem), cpp_options);
    if (!exported) return pistoris::c_api::publish(exported, error);
    pistoris::ObjBundle result = std::move(*exported);
    std::unique_ptr<ArxObjTextureFiles> textures;
    if (out_textures) {
      textures = std::make_unique<ArxObjTextureFiles>();
      textures->value = std::move(result.texture_files);
    }
    const ArxReturnCode publish_rc = publishObjText(result, out_obj, out_mtl);
    if (publish_rc != ARX_OK) return publish_rc;
    if (out_textures) *out_textures = textures.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_obj_texture_files_count(const ArxObjTextureFiles* files, size_t* out_count,
                                                   ArxError* error) noexcept {
  if (!files) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_count) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_count = files->value.size();
  return pistoris::c_api::publishCode(ARX_OK, error);
}

ArxReturnCode arx_pistoris_obj_texture_files_get(const ArxObjTextureFiles* files, size_t index,
                                                 ArxObjTextureFile* out_file, ArxError* error) noexcept {
  if (!files) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_file) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_file = {};
  if (index >= files->value.size()) return pistoris::c_api::publishCode(ARX_INDEX_OUT_OF_RANGE, error);
  const pistoris::ObjTextureFile& file = files->value[index];
  out_file->source_texture = file.source_texture;
  out_file->path = pistoris::c_api::view(file.path);
  out_file->encoded_image = pistoris::c_api::view(file.encoded_image);
  return pistoris::c_api::publishCode(ARX_OK, error);
}

void arx_pistoris_obj_texture_files_destroy(ArxObjTextureFiles* files) noexcept { delete files; }

ArxReturnCode arx_pistoris_model_import_glb(const uint8_t* data, size_t size, const ArxModelGlbImportOptions* options,
                                            ArxModel** out_model, ArxAnimationList** out_animations,
                                            ArxAnimationConversionReport* report,
                                            ArxTextureSourcePaths** out_texture_source_paths,
                                            ArxAnimationSoundSourceReferences** out_sound_sources,
                                            ArxError* error) noexcept {
  if (!data || !out_model) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  if ((report || out_sound_sources) && !out_animations) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  *out_model = nullptr;
  if (out_animations) *out_animations = nullptr;
  if (report) *report = {};
  if (out_texture_source_paths) *out_texture_source_paths = nullptr;
  if (out_sound_sources) *out_sound_sources = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    auto result = std::make_unique<ArxModel>();
    std::unique_ptr<ArxTextureSourcePaths> texture_paths;
    if (out_texture_source_paths) texture_paths = std::make_unique<ArxTextureSourcePaths>();
    pistoris::Model::GlbImportOptions cpp_options;
    if (options) cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
    if (!out_animations) {
      auto imported = pistoris::Model::importGlb(
          std::span<const std::uint8_t>(data, size), cpp_options, texture_paths ? &texture_paths->value : nullptr);
      if (!imported) return pistoris::c_api::publish(imported, error);
      result->value = std::move(*imported);
      *out_model = result.release();
      if (out_texture_source_paths) *out_texture_source_paths = texture_paths.release();
      return pistoris::c_api::publishCode(ARX_OK, error);
    }

    std::unique_ptr<ArxAnimationSoundSourceReferences> sound_sources;
    if (out_sound_sources) sound_sources = std::make_unique<ArxAnimationSoundSourceReferences>();
    ArxAnimationConversionReport local_report{};
    auto imported = pistoris::Model::importGlbWithAnimations(std::span<const std::uint8_t>(data, size),
                                                             cpp_options,
                                                             report ? &local_report : nullptr,
                                                             texture_paths ? &texture_paths->value : nullptr,
                                                             sound_sources ? &sound_sources->value : nullptr);
    if (!imported) return pistoris::c_api::publish(imported, error);

    result->value = std::move(imported->model);
    std::unique_ptr<ArxAnimationList> animations = pistoris::c_api::makeAnimationList(std::move(imported->animations));
    *out_model = result.release();
    *out_animations = animations.release();
    if (report) *report = local_report;
    if (out_texture_source_paths) *out_texture_source_paths = texture_paths.release();
    if (out_sound_sources) *out_sound_sources = sound_sources.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_model_export_level_preview_glb(const ArxModel* model,
                                                          const ArxModelLevelPreviewGlbOptions* options,
                                                          uint8_t** out_data, size_t* out_size,
                                                          ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!out_data || !out_size) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  if (options && (!pistoris::c_api::valid(options->class_path) || !pistoris::c_api::valid(options->asset_name)))
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_data = nullptr;
  *out_size = 0;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    pistoris::Model::LevelPreviewGlbOptions cpp_options;
    if (options) {
      cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
      cpp_options.class_path = pistoris::c_api::stringView(options->class_path);
      cpp_options.asset_name = pistoris::c_api::stringView(options->asset_name);
    }
    auto result = model->value.exportLevelPreviewGlb(cpp_options);
    if (!result) return pistoris::c_api::publish(result, error);
    return pistoris::c_api::publishBytes(std::move(*result), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_model_export_glb(const ArxModel* model, const ArxAnimation* const* animations,
                                            size_t animation_count, const ArxModelGlbExportOptions* options,
                                            ArxAnimationConversionReport* report, uint8_t** out_data, size_t* out_size,
                                            ArxAnimationSoundFiles** out_sounds, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!pistoris::c_api::valid(animations, animation_count) || !out_data || !out_size)
    return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  for (std::size_t index = 0; index < animation_count; ++index)
    if (!animations[index]) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  *out_data = nullptr;
  *out_size = 0;
  if (out_sounds) *out_sounds = nullptr;
  if (report) *report = {};
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    std::vector<const pistoris::Animation*> animation_views;
    animation_views.reserve(animation_count);
    for (std::size_t index = 0; index < animation_count; ++index) animation_views.push_back(&animations[index]->value);
    pistoris::Model::GlbExportOptions cpp_options;
    if (options) cpp_options.arx_units_per_glb_unit = options->arx_units_per_glb_unit;
    ArxAnimationConversionReport local_report{};
    if (out_sounds) {
      auto exported = model->value.exportGlbBundle(animation_views, cpp_options, report ? &local_report : nullptr);
      if (!exported) return pistoris::c_api::publish(exported, error);
      pistoris::ModelGlbBundle bundle = std::move(*exported);
      auto sounds = std::make_unique<ArxAnimationSoundFiles>();
      sounds->value = std::move(bundle.sound_files);
      const ArxReturnCode publish_rc = pistoris::c_api::publishBytes(std::move(bundle.glb), out_data, out_size);
      if (publish_rc != ARX_OK) return publish_rc;
      *out_sounds = sounds.release();
    } else {
      auto result = model->value.exportGlb(animation_views, cpp_options, report ? &local_report : nullptr);
      if (!result) return pistoris::c_api::publish(result, error);
      const ArxReturnCode publish_rc = pistoris::c_api::publishBytes(std::move(*result), out_data, out_size);
      if (publish_rc != ARX_OK) return publish_rc;
    }
    if (report) *report = local_report;
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_model_bake_native(const ArxModel* model, const ArxNativeModelBakeOptions* options,
                                             ArxFtl** out_native, ArxNativeTextureFiles** out_textures,
                                             ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  if (!options) return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
  if (!out_native) return pistoris::c_api::publishCode(ARX_INVALID_DATA_POINTER, error);
  *out_native = nullptr;
  if (out_textures) *out_textures = nullptr;
  return pistoris::c_api::guard(error, [&]() -> ArxReturnCode {
    if (!pistoris::c_api::validNativeTextMode(options->text_mode))
      return pistoris::c_api::publishCode(ARX_INVALID_OPTIONS, error);
    const pistoris::NativeModelBakeOptions cpp_options{
        .include_texture_files = out_textures && options->include_texture_files != 0,
        .text_mode = pistoris::c_api::nativeTextMode(options->text_mode)};
    auto baked = model->value.bakeNativeBundle(cpp_options);
    if (!baked) return pistoris::c_api::publish(baked, error);
    pistoris::NativeModelBundle bundle = std::move(*baked);
    auto native = std::make_unique<ArxFtl>();
    native->value = std::move(bundle.ftl);
    std::unique_ptr<ArxNativeTextureFiles> textures;
    if (out_textures) {
      textures = std::make_unique<ArxNativeTextureFiles>();
      textures->value = std::move(bundle.texture_files);
    }
    *out_native = native.release();
    if (out_textures) *out_textures = textures.release();
    return pistoris::c_api::publishCode(ARX_OK, error);
  });
}

ArxReturnCode arx_pistoris_model_validate(const ArxModel* model, ArxError* error) noexcept {
  if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);
  auto result = model->value.validate();
  return pistoris::c_api::publish(result, error);
}

#define ARX_MODEL_VALIDATE(name, method)                                                              \
  ArxReturnCode arx_pistoris_model_validate_##name(const ArxModel* model, ArxError* error) noexcept { \
    if (!model) return pistoris::c_api::publishCode(ARX_INVALID_HANDLE, error);                       \
    auto result = model->value.method();                                                              \
    return pistoris::c_api::publish(result, error);                                                   \
  }

ARX_MODEL_VALIDATE(geometry, validateGeometry)
ARX_MODEL_VALIDATE(skeleton, validateSkeleton)
ARX_MODEL_VALIDATE(action_points, validateActionPoints)
ARX_MODEL_VALIDATE(selections, validateSelections)

#undef ARX_MODEL_VALIDATE

// NOLINTEND(readability-identifier-naming)
