// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "fuzz_common.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  ArxFtl* raw_native = nullptr;
  const ArxReturnCode native_rc = arx_pistoris_ftl_read(data, size, &raw_native);
  arx_fuzz::FtlHandle native(raw_native);
  if (native_rc != ARX_OK) {
    if (native.get()) std::abort();
    return 0;
  }
  if (!native.get()) std::abort();

  ArxModel* raw_model = nullptr;
  ArxTextureSourcePaths* raw_sources = nullptr;
  const ArxReturnCode rc =
      arx_pistoris_model_import_native(native.get(), &raw_model, &raw_sources, ARX_NATIVE_TEXT_AUTO);
  arx_fuzz::ModelHandle model(raw_model);
  arx_fuzz::TextureSourcePathsHandle sources(raw_sources);
  if (rc != ARX_OK) {
    if (model.get() || sources.get()) std::abort();
    return 0;
  }
  if (!model.get() || !sources.get()) std::abort();
  if (arx_pistoris_model_validate(model.get()) != ARX_OK) std::abort();
  std::size_t texture_count = 0;
  if (arx_pistoris_model_texture_count(model.get(), &texture_count) != ARX_OK) std::abort();
  arx_fuzz::validateTextureSourcePaths(sources.get(), texture_count);
  return 0;
}
