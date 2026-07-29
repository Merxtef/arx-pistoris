// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native_fuzz_common.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  if (size < sizeof(std::uint32_t)) return 0;

  std::uint32_t split_request = 0;
  std::memcpy(&split_request, data, sizeof(split_request));
  data += sizeof(split_request);
  size -= sizeof(split_request);

  const std::size_t split = split_request % (size + 1);
  const std::uint8_t* obj_data = data;
  const std::uint8_t* mtl_data = data + split;
  const std::size_t obj_size = split;
  const std::size_t mtl_size = size - split;
  const std::uint8_t* mtl_or_nil = (mtl_size > 0) ? mtl_data : nullptr;

  ArxFtlHandle raw_ftl = nullptr;
  const ArxReturnCode rc = arx_pistoris_obj_parse(obj_data, obj_size, mtl_or_nil, mtl_size, nullptr, &raw_ftl);
  if (rc == ARX_OK) {
    arx_fuzz::FtlHandle ftl(raw_ftl);
    if (!ftl.get()) std::abort();
  }
  return 0;
}
