// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/tea_data.hpp"

#include "api/api_helpers.h"
#include "arx/ftl.h"
#include "arx/tea.h"
#include "external/glb.h"
#include "external/json.h"
#include "external/obj.h"
#include "layout_hash_gen.h"
#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/math/xform.h"
#include "version.h"

#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {
ArxLogFn log_fn = nullptr;
void* log_ud    = nullptr;

const char* buildTime();
}  // namespace pistoris

namespace {

template <class Fn>
ArxReturnCode guard(const char* where, Fn&& fn) {
  try {
    return std::forward<Fn>(fn)();
  } catch (const std::bad_alloc&) {
    return ARX_BAD_ALLOC;
  } catch (const std::exception& e) {
    pistoris::log(ARX_LOG_ERROR, std::string(where) + ": unexpected exception: " + e.what());
    return ARX_UNKNOWN_ERROR;
  } catch (...) {
    pistoris::log(ARX_LOG_ERROR, std::string(where) + ": unexpected exception");
    return ARX_UNKNOWN_ERROR;
  }
}

}  // namespace

// NOLINTBEGIN(readability-identifier-naming)

const char* arx_pistoris_version(void) { return pistoris::kVersion; }

const char* arx_pistoris_build_time(void) { return pistoris::buildTime(); }

const char* arx_pistoris_get_layout_hash(void) { return ARX_PISTORIS_LAYOUT_HASH; }

void arx_pistoris_set_log_callback(ArxLogFn fn, void* userdata) {
  pistoris::log_fn = fn;
  pistoris::log_ud = userdata;
}

ArxReturnCode arx_pistoris_ftl_parse(const uint8_t* data, size_t size, ArxFtlHandle* out) {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out) return ARX_INVALID_HANDLE;

  *out = nullptr;

  return guard("arx_pistoris_ftl_parse", [&] {
    auto d = std::make_unique<pistoris::ftl::Data>();
    pistoris::ReadCursor c(data, size);
    ArxReturnCode rc = pistoris::loadFtl(d.get(), c);
    if (rc != ARX_OK) {
      *out = nullptr;
      return rc;
    }

    *out = reinterpret_cast<ArxFtlHandle>(d.release());
    return ARX_OK;
  });
}

void arx_pistoris_ftl_free(ArxFtlHandle h) { delete reinterpret_cast<pistoris::ftl::Data*>(h); }

ArxReturnCode arx_pistoris_tea_parse(const uint8_t* data, size_t size, ArxTeaHandle* out) {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out) return ARX_INVALID_HANDLE;

  *out = nullptr;

  return guard("arx_pistoris_tea_parse", [&] {
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

void arx_pistoris_tea_free(ArxTeaHandle h) { delete reinterpret_cast<pistoris::tea::Data*>(h); }

const char* arx_pistoris_tea_name(ArxTeaHandle h) {
  if (!h) return nullptr;
  return reinterpret_cast<const pistoris::tea::Data*>(h)->name;
}

ArxReturnCode arx_pistoris_tea_write(ArxTeaHandle h, uint8_t** out, size_t* out_size) {
  if (!h) return ARX_INVALID_HANDLE;
  if (!out) return ARX_INVALID_DATA_POINTER;
  if (!out_size) return ARX_INVALID_DATA_POINTER;

  *out      = nullptr;
  *out_size = 0;

  return guard("arx_pistoris_tea_write", [&] {
    const auto* d = reinterpret_cast<const pistoris::tea::Data*>(h);
    pistoris::WriteCursor c;
    ArxReturnCode rc = pistoris::saveTea(d, c);
    if (rc != ARX_OK) return rc;

    auto buf  = c.take();
    auto* p   = new uint8_t[buf.size()];
    *out      = p;
    *out_size = buf.size();
    std::memcpy(p, buf.data(), buf.size());
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_ftl_to_obj(ArxFtlHandle h, const char* obj_stem, char** out) {
  if (!h) return ARX_INVALID_HANDLE;
  if (!obj_stem) return ARX_INVALID_DATA_POINTER;
  if (!out) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;

  return guard("arx_pistoris_ftl_to_obj", [&] {
    const auto* d = reinterpret_cast<const pistoris::ftl::Data*>(h);
    std::string result;
    ArxReturnCode rc = pistoris::exportFtlToObj(*d, obj_stem, result);
    if (rc != ARX_OK) return rc;

    char* buf = new char[result.size() + 1];
    std::memcpy(buf, result.c_str(), result.size() + 1);
    *out = buf;
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_ftl_to_mtl(ArxFtlHandle h, char** out) {
  if (!h) return ARX_INVALID_HANDLE;
  if (!out) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;

  return guard("arx_pistoris_ftl_to_mtl", [&] {
    const auto* d = reinterpret_cast<const pistoris::ftl::Data*>(h);
    std::string result;
    ArxReturnCode rc = pistoris::exportFtlToMtl(*d, result);
    if (rc != ARX_OK) return rc;

    char* buf = new char[result.size() + 1];
    std::memcpy(buf, result.c_str(), result.size() + 1);
    *out = buf;
    return ARX_OK;
  });
}

void arx_pistoris_free_string(char* s) { delete[] s; }

ArxReturnCode arx_pistoris_ftl_to_json(ArxFtlHandle h, int pretty, char** out) {
  if (!h) return ARX_INVALID_HANDLE;
  if (!out) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;

  return guard("arx_pistoris_ftl_to_json", [&] {
    const auto* d = reinterpret_cast<const pistoris::ftl::Data*>(h);
    std::string result;
    ArxReturnCode rc = pistoris::exportFtlToJson(*d, pretty != 0, result);
    if (rc != ARX_OK) return rc;

    char* buf = new char[result.size() + 1];
    std::memcpy(buf, result.c_str(), result.size() + 1);
    *out = buf;
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_ftl_from_json(const uint8_t* data, size_t size, ArxFtlHandle* out) {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out) return ARX_INVALID_HANDLE;

  *out = nullptr;

  return guard("arx_pistoris_ftl_from_json", [&] {
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

ArxReturnCode arx_pistoris_tea_to_json(ArxTeaHandle h, int pretty, char** out) {
  if (!h) return ARX_INVALID_HANDLE;
  if (!out) return ARX_INVALID_DATA_POINTER;

  *out = nullptr;

  return guard("arx_pistoris_tea_to_json", [&] {
    const auto* d = reinterpret_cast<const pistoris::tea::Data*>(h);
    std::string result;
    ArxReturnCode rc = pistoris::exportTeaToJson(*d, pretty != 0, result);
    if (rc != ARX_OK) return rc;

    char* buf = new char[result.size() + 1];
    std::memcpy(buf, result.c_str(), result.size() + 1);
    *out = buf;
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_tea_from_json(const uint8_t* data, size_t size, ArxTeaHandle* out) {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out) return ARX_INVALID_HANDLE;

  *out = nullptr;

  return guard("arx_pistoris_tea_from_json", [&] {
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

ArxReturnCode arx_pistoris_ftl_write(ArxFtlHandle h, uint8_t** out, size_t* out_size) {
  if (!h) return ARX_INVALID_HANDLE;
  if (!out) return ARX_INVALID_DATA_POINTER;
  if (!out_size) return ARX_INVALID_DATA_POINTER;

  *out      = nullptr;
  *out_size = 0;

  return guard("arx_pistoris_ftl_write", [&] {
    const auto* d = reinterpret_cast<const pistoris::ftl::Data*>(h);
    pistoris::WriteCursor c;
    ArxReturnCode rc = pistoris::saveFtl(d, c);
    if (rc != ARX_OK) return rc;

    auto buf   = c.take();
    uint8_t* p = new uint8_t[buf.size()];
    std::memcpy(p, buf.data(), buf.size());
    *out      = p;
    *out_size = buf.size();
    return ARX_OK;
  });
}

void arx_pistoris_free_bytes(uint8_t* p) { delete[] p; }

ArxReturnCode arx_pistoris_to_glb(ArxFtlHandle ftl_h, const ArxTeaHandle* teas, size_t tea_count, uint8_t** out,
                                  size_t* out_size) {
  if (!ftl_h) return ARX_INVALID_HANDLE;
  if (!out) return ARX_INVALID_DATA_POINTER;
  if (!out_size) return ARX_INVALID_DATA_POINTER;
  if (tea_count > 0 && !teas) return ARX_INVALID_DATA_POINTER;
  for (size_t i = 0; i < tea_count; ++i) {
    if (!teas[i]) return ARX_INVALID_HANDLE;
  }

  *out      = nullptr;
  *out_size = 0;

  return guard("arx_pistoris_to_glb", [&] {
    const auto* d = reinterpret_cast<const pistoris::ftl::Data*>(ftl_h);

    std::vector<const pistoris::tea::Data*> tea_ptrs;
    tea_ptrs.reserve(tea_count);
    for (size_t i = 0; i < tea_count; ++i) tea_ptrs.push_back(reinterpret_cast<const pistoris::tea::Data*>(teas[i]));

    std::vector<uint8_t> result;
    ArxReturnCode rc = pistoris::exportFtlTeaToGlb(*d, tea_ptrs, result);
    if (rc != ARX_OK) return rc;

    auto* p   = new uint8_t[result.size()];
    *out      = p;
    *out_size = result.size();
    std::memcpy(p, result.data(), result.size());
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_from_glb(const uint8_t* data, size_t size, const char* glb_filename, ArxFtlHandle* out_ftl,
                                    ArxTeaHandle** out_teas, size_t* out_tea_count) {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out_ftl) return ARX_INVALID_HANDLE;
  if (!out_teas || !out_tea_count) return ARX_INVALID_DATA_POINTER;

  *out_ftl       = nullptr;
  *out_teas      = nullptr;
  *out_tea_count = 0;

  return guard("arx_pistoris_from_glb", [&] {
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

    ArxTeaHandle* arr = nullptr;
    if (!tea_owned.empty()) arr = new ArxTeaHandle[tea_owned.size()]();
    for (size_t i = 0; i < tea_owned.size(); ++i) arr[i] = reinterpret_cast<ArxTeaHandle>(tea_owned[i].release());

    *out_ftl       = reinterpret_cast<ArxFtlHandle>(fd.release());
    *out_teas      = arr;
    *out_tea_count = tea_owned.size();
    return ARX_OK;
  });
}

void arx_pistoris_free_tea_array(ArxTeaHandle* teas, size_t count) {
  if (!teas) return;
  for (size_t i = 0; i < count; ++i) arx_pistoris_tea_free(teas[i]);
  delete[] teas;
}

ArxReturnCode arx_pistoris_ftl_overwrite_texture_paths(ArxFtlHandle ftl_h, const char* path) {
  if (!ftl_h) return ARX_INVALID_HANDLE;
  if (!path) return ARX_INVALID_DATA_POINTER;

  auto* d = reinterpret_cast<pistoris::ftl::Data*>(ftl_h);
  return guard("arx_pistoris_ftl_overwrite_texture_paths", [&] {
    return pistoris::api::overwriteTexturePaths(*d, path, "arx_pistoris_ftl_overwrite_texture_paths");
  });
}

ArxReturnCode arx_pistoris_ftl_validate(ArxFtlHandle ftl_h) {
  if (!ftl_h) return ARX_INVALID_HANDLE;

  const auto* d = reinterpret_cast<const pistoris::ftl::Data*>(ftl_h);
  return guard("arx_pistoris_ftl_validate", [&] { return pistoris::validateFtl(d); });
}

ArxReturnCode arx_pistoris_tea_validate(ArxTeaHandle tea_h) {
  if (!tea_h) return ARX_INVALID_HANDLE;

  const auto* d = reinterpret_cast<const pistoris::tea::Data*>(tea_h);
  return guard("arx_pistoris_tea_validate", [&] { return pistoris::validateTea(d); });
}

ArxReturnCode arx_pistoris_ftl_apply_xform(ArxFtlHandle ftl_h, float rx, float ry, float rz, float sx, float sy,
                                           float sz, float tx, float ty, float tz) {
  if (!ftl_h) return ARX_INVALID_HANDLE;

  auto* d = reinterpret_cast<pistoris::ftl::Data*>(ftl_h);
  return guard("arx_pistoris_ftl_apply_xform", [&] {
    pistoris::AffineXform xform = pistoris::makeAffineXform(rx, ry, rz, sx, sy, sz, tx, ty, tz);
    return pistoris::api::applyTransform(*d, xform);
  });
}

ArxReturnCode arx_pistoris_tea_apply_xform(ArxTeaHandle tea_h, float rx, float ry, float rz, float sx, float sy,
                                           float sz, float tx, float ty, float tz) {
  if (!tea_h) return ARX_INVALID_HANDLE;

  auto* d = reinterpret_cast<pistoris::tea::Data*>(tea_h);
  return guard("arx_pistoris_tea_apply_xform", [&] {
    pistoris::AffineXform xform = pistoris::makeAffineXform(rx, ry, rz, sx, sy, sz, tx, ty, tz);
    return pistoris::api::applyTransform(*d, xform);
  });
}

ArxReturnCode arx_pistoris_ftl_snap_bone_origins_to_reference(ArxFtlHandle target, ArxFtlHandle reference) {
  if (!target || !reference) return ARX_INVALID_HANDLE;

  auto* target_data          = reinterpret_cast<pistoris::ftl::Data*>(target);
  const auto* reference_data = reinterpret_cast<const pistoris::ftl::Data*>(reference);
  return guard("arx_pistoris_ftl_snap_bone_origins_to_reference",
               [&] { return pistoris::api::snapFtlBoneOriginsToReference(*target_data, *reference_data); });
}

ArxReturnCode arx_pistoris_ftl_snap_action_points_to_reference(ArxFtlHandle target, ArxFtlHandle reference) {
  if (!target || !reference) return ARX_INVALID_HANDLE;

  auto* target_data          = reinterpret_cast<pistoris::ftl::Data*>(target);
  const auto* reference_data = reinterpret_cast<const pistoris::ftl::Data*>(reference);
  return guard("arx_pistoris_ftl_snap_action_points_to_reference",
               [&] { return pistoris::api::snapFtlActionPointsToReference(*target_data, *reference_data); });
}

ArxReturnCode arx_pistoris_ftl_copy_synthetic_selection_affiliations(ArxFtlHandle target, ArxFtlHandle reference) {
  if (!target || !reference) return ARX_INVALID_HANDLE;

  auto* target_data          = reinterpret_cast<pistoris::ftl::Data*>(target);
  const auto* reference_data = reinterpret_cast<const pistoris::ftl::Data*>(reference);
  return guard("arx_pistoris_ftl_copy_synthetic_selection_affiliations",
               [&] { return pistoris::api::copyFtlSyntheticSelectionAffiliations(*target_data, *reference_data); });
}

ArxReturnCode arx_pistoris_obj_parse(const uint8_t* obj_data, size_t obj_size, const uint8_t* mtl_data, size_t mtl_size,
                                     const char* obj_filename, ArxFtlHandle* out) {
  if (!obj_data) return ARX_INVALID_DATA_POINTER;
  if (!mtl_data && mtl_size > 0) return ARX_INVALID_DATA_POINTER;
  if (!out) return ARX_INVALID_HANDLE;

  *out = nullptr;

  return guard("arx_pistoris_obj_parse", [&] {
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
