// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

bool takeLe32(const std::uint8_t*& data, std::size_t& size, std::uint32_t& value) {
  constexpr std::size_t kEncodedSize = 4;
  if (size < kEncodedSize) return false;
  value = static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8U) |
          (static_cast<std::uint32_t>(data[2]) << 16U) | (static_cast<std::uint32_t>(data[3]) << 24U);
  data += kEncodedSize;
  size -= kEncodedSize;
  return true;
}

void validateImportedModel(ArxReturnCode rc, ArxModel* raw_model, ArxTextureSourcePaths* raw_sources) {
  arx_fuzz::ModelHandle model(raw_model);
  arx_fuzz::TextureSourcePathsHandle sources(raw_sources);
  if (rc != ARX_OK) {
    if (model.get() || sources.get()) std::abort();
    return;
  }
  if (!model.get() || !sources.get()) std::abort();
  if (arx_pistoris_model_validate(model.get()) != ARX_OK) std::abort();
  std::size_t texture_count = 0;
  if (arx_pistoris_model_texture_count(model.get(), &texture_count) != ARX_OK) std::abort();
  arx_fuzz::validateTextureSourcePaths(sources.get(), texture_count);
}

}  // namespace

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  std::uint32_t obj_size = 0;
  if (!takeLe32(data, size, obj_size) || obj_size > size) return 0;
  const std::uint8_t* obj_data = data;
  data += obj_size;
  size -= obj_size;

  ArxObjMaterialLibraryPaths* raw_paths = nullptr;
  const ArxReturnCode paths_rc = arx_pistoris_obj_material_library_paths(obj_data, obj_size, &raw_paths);
  arx_fuzz::ObjMaterialLibraryPathsHandle paths(raw_paths);
  if (paths_rc != ARX_OK) {
    if (paths.get()) std::abort();
    return 0;
  }
  if (!paths.get()) std::abort();

  std::size_t library_count = 0;
  if (arx_pistoris_obj_material_library_paths_count(paths.get(), &library_count) != ARX_OK) std::abort();
  if (library_count == 0) return 0;

  std::vector<ArxObjMaterialLibraryView> libraries;
  libraries.reserve(library_count);
  for (std::size_t index = 0; index < library_count; ++index) {
    std::uint32_t library_size = 0;
    if (!takeLe32(data, size, library_size) || library_size > size) return 0;
    ArxStringView path{};
    if (arx_pistoris_obj_material_library_paths_get(paths.get(), index, &path) != ARX_OK) std::abort();
    libraries.push_back({path, data, library_size});
    data += library_size;
    size -= library_size;
  }

  ArxModel* raw_model = nullptr;
  ArxTextureSourcePaths* raw_sources = nullptr;
  const ArxReturnCode model_rc =
      arx_pistoris_model_import_obj(obj_data, obj_size, libraries.data(), libraries.size(), &raw_model, &raw_sources);
  validateImportedModel(model_rc, raw_model, raw_sources);
  return 0;
}
