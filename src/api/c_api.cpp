// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/tea.hpp"

#include "api/api_helpers.h"
#include "api/c_api_internal.h"
#include "arx/ftl.h"
#include "arx/native_storage.h"
#include "arx/tea.h"
#include "external/glb.h"
#include "external/json.h"
#include "external/obj.h"
#include "layout_hash_gen.h"
#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/math/xform.h"
#include "version.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
ArxLogFn log_fn = nullptr;
void* log_ud = nullptr;

const char* buildTime();
}  // namespace pistoris

using pistoris::c_api::guard;

// NOLINTBEGIN(readability-identifier-naming)

const char* arx_pistoris_version(void) noexcept { return pistoris::kVersion; }

const char* arx_pistoris_build_time(void) noexcept { return pistoris::buildTime(); }

const char* arx_pistoris_get_layout_hash(void) noexcept { return ARX_PISTORIS_LAYOUT_HASH; }

void arx_pistoris_set_log_callback(ArxLogFn fn, void* userdata) noexcept {
  pistoris::log_fn = fn;
  pistoris::log_ud = userdata;
}

ArxReturnCode arx_pistoris_ftl_parse(const uint8_t* data, size_t size, ArxFtlHandle* out) noexcept {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;

  return guard("arx_pistoris_ftl_parse", [&]() -> ArxReturnCode {
    auto d = std::make_unique<pistoris::ftl::Data>();
    ArxReturnCode rc = pistoris::loadFtlStorage(d.get(), std::span<const std::uint8_t>(data, size));
    if (rc != ARX_OK) {
      *out = nullptr;
      return rc;
    }

    *out = reinterpret_cast<ArxFtlHandle>(d.release());
    return ARX_OK;
  });
}

void arx_pistoris_ftl_free(ArxFtlHandle h) noexcept { delete reinterpret_cast<pistoris::ftl::Data*>(h); }

ArxReturnCode arx_pistoris_tea_parse(const uint8_t* data, size_t size, ArxTeaHandle* out) noexcept {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;

  return guard("arx_pistoris_tea_parse", [&]() -> ArxReturnCode {
    auto d = std::make_unique<pistoris::tea::Data>();
    pistoris::ReadCursor c(data, size);
    ArxReturnCode rc = pistoris::loadTea(d.get(), c);
    if (rc != ARX_OK) {
      *out = nullptr;
      return rc;
    }

    *out = reinterpret_cast<ArxTeaHandle>(d.release());
    return ARX_OK;
  });
}

void arx_pistoris_tea_free(ArxTeaHandle h) noexcept { delete reinterpret_cast<pistoris::tea::Data*>(h); }

const char* arx_pistoris_tea_name(ArxTeaHandle h) noexcept {
  if (!h) return nullptr;
  return reinterpret_cast<const pistoris::tea::Data*>(h)->name;
}

ArxReturnCode arx_pistoris_tea_write(ArxTeaHandle h, uint8_t** out, size_t* out_size) noexcept {
  if (!h) return ARX_INVALID_HANDLE;
  if (!out) return ARX_INVALID_DATA_POINTER;
  if (!out_size) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;
  *out_size = 0;

  return guard("arx_pistoris_tea_write", [&]() -> ArxReturnCode {
    const auto* d = reinterpret_cast<const pistoris::tea::Data*>(h);
    pistoris::WriteCursor c;
    ArxReturnCode rc = pistoris::saveTea(d, c);
    if (rc != ARX_OK) return rc;

    return pistoris::c_api::publishBytes(c.take(), out, out_size);
  });
}

ArxReturnCode arx_pistoris_ftl_to_obj(ArxFtlHandle h, const char* obj_stem, char** out) noexcept {
  if (!h) return ARX_INVALID_HANDLE;
  if (!obj_stem) return ARX_INVALID_DATA_POINTER;
  if (!out) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;

  return guard("arx_pistoris_ftl_to_obj", [&]() -> ArxReturnCode {
    const auto* d = reinterpret_cast<const pistoris::ftl::Data*>(h);
    std::string result;
    ArxReturnCode rc = pistoris::exportFtlToObj(*d, obj_stem, result);
    if (rc != ARX_OK) return rc;

    return pistoris::c_api::publishString(result, out);
  });
}

ArxReturnCode arx_pistoris_ftl_to_mtl(ArxFtlHandle h, char** out) noexcept {
  if (!h) return ARX_INVALID_HANDLE;
  if (!out) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;

  return guard("arx_pistoris_ftl_to_mtl", [&]() -> ArxReturnCode {
    const auto* d = reinterpret_cast<const pistoris::ftl::Data*>(h);
    std::string result;
    ArxReturnCode rc = pistoris::exportFtlToMtl(*d, result);
    if (rc != ARX_OK) return rc;

    return pistoris::c_api::publishString(result, out);
  });
}

void arx_pistoris_free_string(char* s) noexcept { delete[] s; }

ArxReturnCode arx_pistoris_ftl_to_json(ArxFtlHandle h, uint32_t pretty, char** out) noexcept {
  if (!h) return ARX_INVALID_HANDLE;
  if (!out) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;

  return guard("arx_pistoris_ftl_to_json", [&]() -> ArxReturnCode {
    const auto* d = reinterpret_cast<const pistoris::ftl::Data*>(h);
    std::string result;
    ArxReturnCode rc = pistoris::exportFtlToJson(*d, pretty != 0, result);
    if (rc != ARX_OK) return rc;

    return pistoris::c_api::publishString(result, out);
  });
}

ArxReturnCode arx_pistoris_ftl_from_json(const uint8_t* data, size_t size, ArxFtlHandle* out) noexcept {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;

  return guard("arx_pistoris_ftl_from_json", [&]() -> ArxReturnCode {
    auto d = std::make_unique<pistoris::ftl::Data>();
    std::string_view text(reinterpret_cast<const char*>(data), size);
    ArxReturnCode rc = pistoris::importJsonToFtl(text, d.get());
    if (rc != ARX_OK) {
      *out = nullptr;
      return rc;
    }

    *out = reinterpret_cast<ArxFtlHandle>(d.release());
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_tea_to_json(ArxTeaHandle h, uint32_t pretty, char** out) noexcept {
  if (!h) return ARX_INVALID_HANDLE;
  if (!out) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;

  return guard("arx_pistoris_tea_to_json", [&]() -> ArxReturnCode {
    const auto* d = reinterpret_cast<const pistoris::tea::Data*>(h);
    std::string result;
    ArxReturnCode rc = pistoris::exportTeaToJson(*d, pretty != 0, result);
    if (rc != ARX_OK) return rc;

    return pistoris::c_api::publishString(result, out);
  });
}

ArxReturnCode arx_pistoris_tea_from_json(const uint8_t* data, size_t size, ArxTeaHandle* out) noexcept {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;

  return guard("arx_pistoris_tea_from_json", [&]() -> ArxReturnCode {
    auto d = std::make_unique<pistoris::tea::Data>();
    std::string_view text(reinterpret_cast<const char*>(data), size);
    ArxReturnCode rc = pistoris::importJsonToTea(text, d.get());
    if (rc != ARX_OK) {
      *out = nullptr;
      return rc;
    }

    *out = reinterpret_cast<ArxTeaHandle>(d.release());
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_ftl_write(ArxFtlHandle h, uint32_t compress, uint8_t** out, size_t* out_size) noexcept {
  if (!h) return ARX_INVALID_HANDLE;
  if (!out) return ARX_INVALID_DATA_POINTER;
  if (!out_size) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;
  *out_size = 0;

  return guard("arx_pistoris_ftl_write", [&]() -> ArxReturnCode {
    const auto* d = reinterpret_cast<const pistoris::ftl::Data*>(h);
    std::vector<std::uint8_t> buf;
    ArxReturnCode rc = pistoris::saveFtlStorage(d, buf, compress != 0);
    if (rc != ARX_OK) return rc;

    return pistoris::c_api::publishBytes(std::move(buf), out, out_size);
  });
}

void arx_pistoris_free_bytes(uint8_t* p) noexcept { delete[] p; }

ArxReturnCode arx_pistoris_to_glb(ArxFtlHandle ftl_h, const ArxTeaHandle* teas, size_t tea_count, uint8_t** out,
                                  size_t* out_size) noexcept {
  if (!ftl_h) return ARX_INVALID_HANDLE;
  if (!out) return ARX_INVALID_DATA_POINTER;
  if (!out_size) return ARX_INVALID_DATA_POINTER;
  if (tea_count > 0 && !teas) return ARX_INVALID_DATA_POINTER;
  for (size_t i = 0; i < tea_count; ++i) {
    if (!teas[i]) return ARX_INVALID_HANDLE;
  }

  *out = nullptr;
  *out_size = 0;

  return guard("arx_pistoris_to_glb", [&]() -> ArxReturnCode {
    const auto* d = reinterpret_cast<const pistoris::ftl::Data*>(ftl_h);

    std::vector<const pistoris::tea::Data*> tea_ptrs;
    tea_ptrs.reserve(tea_count);
    for (size_t i = 0; i < tea_count; ++i) tea_ptrs.push_back(reinterpret_cast<const pistoris::tea::Data*>(teas[i]));

    std::vector<uint8_t> result;
    ArxReturnCode rc = pistoris::exportFtlTeaToGlb(*d, tea_ptrs, result);
    if (rc != ARX_OK) return rc;

    return pistoris::c_api::publishBytes(std::move(result), out, out_size);
  });
}

ArxReturnCode arx_pistoris_from_glb(const uint8_t* data, size_t size, const char* glb_filename, ArxFtlHandle* out_ftl,
                                    ArxTeaHandle** out_teas, size_t* out_tea_count) noexcept {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out_ftl) return ARX_INVALID_DATA_POINTER;
  if (!out_teas || !out_tea_count) return ARX_INVALID_DATA_POINTER;

  *out_ftl = nullptr;
  *out_teas = nullptr;
  *out_tea_count = 0;

  return guard("arx_pistoris_from_glb", [&]() -> ArxReturnCode {
    auto fd = std::make_unique<pistoris::ftl::Data>();
    std::string_view fn(glb_filename ? glb_filename : "");
    std::vector<pistoris::tea::Data> teas_buf;
    ArxReturnCode rc = pistoris::importGlbToFtlTea(std::span<const uint8_t>(data, size), fn, *fd, teas_buf);
    if (rc != ARX_OK) return rc;

    std::vector<std::unique_ptr<pistoris::tea::Data>> tea_owned;
    tea_owned.reserve(teas_buf.size());
    for (size_t i = 0; i < teas_buf.size(); ++i) {
      tea_owned.push_back(std::make_unique<pistoris::tea::Data>(std::move(teas_buf[i])));
    }

    std::unique_ptr<ArxTeaHandle[]> arr;
    if (!tea_owned.empty()) arr = std::make_unique<ArxTeaHandle[]>(tea_owned.size());
    for (size_t i = 0; i < tea_owned.size(); ++i) arr[i] = reinterpret_cast<ArxTeaHandle>(tea_owned[i].release());

    *out_ftl = reinterpret_cast<ArxFtlHandle>(fd.release());
    *out_teas = arr.release();
    *out_tea_count = tea_owned.size();
    return ARX_OK;
  });
}

void arx_pistoris_free_tea_array(ArxTeaHandle* teas, size_t count) noexcept {
  if (!teas) return;
  for (size_t i = 0; i < count; ++i) arx_pistoris_tea_free(teas[i]);
  delete[] teas;
}

ArxReturnCode arx_pistoris_ftl_overwrite_texture_paths(ArxFtlHandle ftl_h, const char* path) noexcept {
  if (!ftl_h) return ARX_INVALID_HANDLE;
  if (!path) return ARX_INVALID_DATA_POINTER;

  auto* d = reinterpret_cast<pistoris::ftl::Data*>(ftl_h);
  return guard("arx_pistoris_ftl_overwrite_texture_paths", [&] {
    return pistoris::api::overwriteTexturePaths(*d, path, "arx_pistoris_ftl_overwrite_texture_paths");
  });
}

ArxReturnCode arx_pistoris_ftl_validate(ArxFtlHandle ftl_h) noexcept {
  if (!ftl_h) return ARX_INVALID_HANDLE;

  const auto* d = reinterpret_cast<const pistoris::ftl::Data*>(ftl_h);
  return guard("arx_pistoris_ftl_validate", [&] { return pistoris::validateFtl(d); });
}

ArxReturnCode arx_pistoris_tea_validate(ArxTeaHandle tea_h) noexcept {
  if (!tea_h) return ARX_INVALID_HANDLE;

  const auto* d = reinterpret_cast<const pistoris::tea::Data*>(tea_h);
  return guard("arx_pistoris_tea_validate", [&] { return pistoris::validateTea(d); });
}

ArxReturnCode arx_pistoris_ftl_apply_xform(ArxFtlHandle ftl_h, float rx, float ry, float rz, float sx, float sy,
                                           float sz, float tx, float ty, float tz) noexcept {
  if (!ftl_h) return ARX_INVALID_HANDLE;

  auto* d = reinterpret_cast<pistoris::ftl::Data*>(ftl_h);
  return guard("arx_pistoris_ftl_apply_xform", [&] {
    pistoris::AffineXform xform = pistoris::makeAffineXform(rx, ry, rz, sx, sy, sz, tx, ty, tz);
    return pistoris::api::applyTransform(*d, xform);
  });
}

ArxReturnCode arx_pistoris_tea_apply_xform(ArxTeaHandle tea_h, float rx, float ry, float rz, float sx, float sy,
                                           float sz, float tx, float ty, float tz) noexcept {
  if (!tea_h) return ARX_INVALID_HANDLE;

  auto* d = reinterpret_cast<pistoris::tea::Data*>(tea_h);
  return guard("arx_pistoris_tea_apply_xform", [&] {
    pistoris::AffineXform xform = pistoris::makeAffineXform(rx, ry, rz, sx, sy, sz, tx, ty, tz);
    return pistoris::api::applyTransform(*d, xform);
  });
}

ArxReturnCode arx_pistoris_ftl_snap_bone_origins_to_reference(ArxFtlHandle target, ArxFtlHandle reference) noexcept {
  if (!target || !reference) return ARX_INVALID_HANDLE;

  auto* target_data = reinterpret_cast<pistoris::ftl::Data*>(target);
  const auto* reference_data = reinterpret_cast<const pistoris::ftl::Data*>(reference);
  return guard("arx_pistoris_ftl_snap_bone_origins_to_reference",
               [&] { return pistoris::api::snapFtlBoneOriginsToReference(*target_data, *reference_data); });
}

ArxReturnCode arx_pistoris_ftl_snap_action_points_to_reference(ArxFtlHandle target, ArxFtlHandle reference) noexcept {
  if (!target || !reference) return ARX_INVALID_HANDLE;

  auto* target_data = reinterpret_cast<pistoris::ftl::Data*>(target);
  const auto* reference_data = reinterpret_cast<const pistoris::ftl::Data*>(reference);
  return guard("arx_pistoris_ftl_snap_action_points_to_reference",
               [&] { return pistoris::api::snapFtlActionPointsToReference(*target_data, *reference_data); });
}

ArxReturnCode arx_pistoris_ftl_copy_synthetic_selection_affiliations(ArxFtlHandle target,
                                                                     ArxFtlHandle reference) noexcept {
  if (!target || !reference) return ARX_INVALID_HANDLE;

  auto* target_data = reinterpret_cast<pistoris::ftl::Data*>(target);
  const auto* reference_data = reinterpret_cast<const pistoris::ftl::Data*>(reference);
  return guard("arx_pistoris_ftl_copy_synthetic_selection_affiliations",
               [&] { return pistoris::api::copyFtlSyntheticSelectionAffiliations(*target_data, *reference_data); });
}

ArxReturnCode arx_pistoris_obj_parse(const uint8_t* obj_data, size_t obj_size, const uint8_t* mtl_data, size_t mtl_size,
                                     const char* obj_filename, ArxFtlHandle* out) noexcept {
  if (!obj_data) return ARX_INVALID_DATA_POINTER;
  if (!mtl_data && mtl_size > 0) return ARX_INVALID_DATA_POINTER;
  if (!out) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;

  return guard("arx_pistoris_obj_parse", [&]() -> ArxReturnCode {
    auto d = std::make_unique<pistoris::ftl::Data>();
    std::string_view obj(reinterpret_cast<const char*>(obj_data), obj_size);
    std::string_view mtl;
    if (mtl_data) mtl = {reinterpret_cast<const char*>(mtl_data), mtl_size};
    std::string_view fn(obj_filename ? obj_filename : "");

    ArxReturnCode rc = pistoris::importObjToFtl(obj, mtl, fn, d.get());
    if (rc != ARX_OK) {
      *out = nullptr;
      return rc;
    }

    *out = reinterpret_cast<ArxFtlHandle>(d.release());
    return ARX_OK;
  });
}

// NOLINTEND(readability-identifier-naming)
