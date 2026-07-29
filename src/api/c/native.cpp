// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/native.h"

#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "api/c_api_internal.h"
#include "arx/dlf.h"
#include "arx/fts.h"
#include "arx/llf.h"
#include "arx/native_storage.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

// NOLINTBEGIN(readability-identifier-naming)

ArxReturnCode arx_pistoris_dlf_parse(const uint8_t* data, size_t size, ArxDlf** out_dlf,
                                     ArxLlf** out_embedded_llf) noexcept {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out_dlf || !out_embedded_llf) return ARX_INVALID_DATA_POINTER;
  *out_dlf = nullptr;
  *out_embedded_llf = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto dlf = std::make_unique<ArxDlf>();
    std::optional<pistoris::llf::Data> embedded;
    ArxReturnCode rc = pistoris::loadDlfStorage(&dlf->value, &embedded, std::span<const std::uint8_t>(data, size));
    if (rc != ARX_OK) return rc;

    std::unique_ptr<ArxLlf> llf;
    if (embedded) {
      llf = std::make_unique<ArxLlf>();
      llf->value = std::move(*embedded);
    }
    *out_dlf = dlf.release();
    *out_embedded_llf = llf.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_fts_parse(const uint8_t* data, size_t size, ArxFts** out_fts) noexcept {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out_fts) return ARX_INVALID_DATA_POINTER;
  *out_fts = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto fts = std::make_unique<ArxFts>();
    ArxReturnCode rc = pistoris::loadFtsStorage(&fts->value, std::span<const std::uint8_t>(data, size));
    if (rc != ARX_OK) return rc;
    *out_fts = fts.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_llf_parse(const uint8_t* data, size_t size, ArxLlf** out_llf) noexcept {
  if (!data) return ARX_INVALID_DATA_POINTER;
  if (!out_llf) return ARX_INVALID_DATA_POINTER;
  *out_llf = nullptr;

  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    auto llf = std::make_unique<ArxLlf>();
    ArxReturnCode rc = pistoris::loadLlfStorage(&llf->value, std::span<const std::uint8_t>(data, size));
    if (rc != ARX_OK) return rc;
    *out_llf = llf.release();
    return ARX_OK;
  });
}

ArxReturnCode arx_pistoris_dlf_write(const ArxDlf* dlf, const ArxDlfWriteOptions* options, uint32_t compress,
                                     uint8_t** out_data, size_t* out_size) noexcept {
  if (!dlf) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  if (options && !pistoris::c_api::valid(options->signer)) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;

  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> result;
    const pistoris::llf::Data* lighting = options && options->embedded_llf ? &options->embedded_llf->value : nullptr;
    const std::string_view signer = options ? pistoris::c_api::stringView(options->signer) : std::string_view{};
    ArxReturnCode rc = pistoris::saveDlfStorage(&dlf->value, lighting, signer, result, compress != 0);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(result), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_fts_write(const ArxFts* fts, uint32_t compress, uint8_t** out_data,
                                     size_t* out_size) noexcept {
  if (!fts) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;

  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> result;
    ArxReturnCode rc = pistoris::saveFtsStorage(&fts->value, result, compress != 0);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(result), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_llf_write(const ArxLlf* llf, const ArxLlfWriteOptions* options, uint32_t compress,
                                     uint8_t** out_data, size_t* out_size) noexcept {
  if (!llf) return ARX_INVALID_HANDLE;
  if (!out_data || !out_size) return ARX_INVALID_DATA_POINTER;
  if (options && !pistoris::c_api::valid(options->signer)) return ARX_INVALID_DATA_POINTER;
  *out_data = nullptr;
  *out_size = 0;

  return pistoris::c_api::guard([&] {
    std::vector<std::uint8_t> result;
    const std::string_view signer = options ? pistoris::c_api::stringView(options->signer) : std::string_view{};
    ArxReturnCode rc = pistoris::saveLlfStorage(&llf->value, signer, result, compress != 0);
    if (rc != ARX_OK) return rc;
    return pistoris::c_api::publishBytes(std::move(result), out_data, out_size);
  });
}

ArxReturnCode arx_pistoris_dlf_validate(const ArxDlf* dlf) noexcept {
  if (!dlf) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return pistoris::validateDlf(&dlf->value); });
}

ArxReturnCode arx_pistoris_fts_validate(const ArxFts* fts) noexcept {
  if (!fts) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return pistoris::validateFts(&fts->value); });
}

ArxReturnCode arx_pistoris_llf_validate(const ArxLlf* llf) noexcept {
  if (!llf) return ARX_INVALID_HANDLE;
  return pistoris::c_api::guard([&] { return pistoris::validateLlf(&llf->value); });
}

void arx_pistoris_dlf_destroy(ArxDlf* dlf) noexcept { delete dlf; }

void arx_pistoris_fts_destroy(ArxFts* fts) noexcept { delete fts; }

void arx_pistoris_llf_destroy(ArxLlf* llf) noexcept { delete llf; }

// NOLINTEND(readability-identifier-naming)
