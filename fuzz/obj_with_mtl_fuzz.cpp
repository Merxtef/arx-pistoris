// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_pistoris.h"

#include <cstddef>
#include <cstdint>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  // first half treated as OBJ, second half as MTL
  std::size_t split       = size / 2;
  const uint8_t* obj_data = data;
  const uint8_t* mtl_data = data + split;

  ArxFtlHandle h   = nullptr;
  ArxReturnCode rc = arx_pistoris_obj_parse(obj_data, split, mtl_data, size - split, nullptr, &h);
  if (rc == ARX_OK) arx_pistoris_ftl_free(h);
  return 0;
}
