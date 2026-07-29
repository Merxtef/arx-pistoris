// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "native_fuzz_common.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  arx_fuzz::silenceLogs();
  std::vector<std::uint8_t> glb = arx_fuzz::buildGlbFromFuzzInput(data, size);
  if (glb.empty()) return 0;

  ArxFtlHandle raw_ftl = nullptr;
  ArxTeaHandle* raw_teas = nullptr;
  std::size_t tea_count = 0;
  const ArxReturnCode rc = arx_pistoris_from_glb(glb.data(), glb.size(), "fuzz.glb", &raw_ftl, &raw_teas, &tea_count);

  if (rc == ARX_OK) {
    arx_fuzz::FtlHandle ftl(raw_ftl);
    arx_fuzz::TeaArray teas(raw_teas, tea_count);
    if (!ftl.get()) std::abort();
    if (teas.size() > 0 && !teas.get()) std::abort();
    if (arx_pistoris_ftl_validate(ftl.get()) != ARX_OK) std::abort();
    for (std::size_t i = 0; i < teas.size(); ++i) {
      if (!teas.get()[i]) std::abort();
      if (arx_pistoris_tea_validate(teas.get()[i]) != ARX_OK) std::abort();
    }
  }

  return 0;
}
