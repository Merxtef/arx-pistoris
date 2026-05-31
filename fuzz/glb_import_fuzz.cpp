// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/arx_pistoris.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr uint32_t kGlbMagic      = 0x46546C67u;  // 'glTF'
constexpr uint32_t kChunkTypeJson = 0x4E4F534Au;  // 'JSON'
constexpr uint32_t kChunkTypeBin  = 0x004E4942u;  // 'BIN\0'

void push32(std::vector<uint8_t>& buf, uint32_t v) {
  buf.push_back(static_cast<uint8_t>(v & 0xffu));
  buf.push_back(static_cast<uint8_t>((v >> 8) & 0xffu));
  buf.push_back(static_cast<uint8_t>((v >> 16) & 0xffu));
  buf.push_back(static_cast<uint8_t>((v >> 24) & 0xffu));
}

}  // namespace

// NOLINTNEXTLINE(readability-identifier-naming) -- libFuzzer entry point
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  if (size < 4) return 0;

  uint32_t bin_len_req = 0;
  std::memcpy(&bin_len_req, data, 4);
  data += 4;
  size -= 4;

  // Split remaining data between JSON and BIN
  std::size_t bin_len  = (size > 0) ? (bin_len_req % size) : 0;
  std::size_t json_len = size - bin_len;

  const uint8_t* json_data = data;
  const uint8_t* bin_data  = data + json_len;

  // glTF requires 4-byte alignment for chunks
  std::size_t json_pad = (4 - (json_len % 4)) % 4;
  std::size_t bin_pad  = (4 - (bin_len % 4)) % 4;

  uint32_t total_len = 12 + 8 + (uint32_t)json_len + (uint32_t)json_pad;
  if (bin_len > 0) {
    total_len += 8 + (uint32_t)bin_len + (uint32_t)bin_pad;
  }

  std::vector<uint8_t> glb;
  glb.reserve(total_len);

  push32(glb, kGlbMagic);
  push32(glb, 2);  // version
  push32(glb, total_len);

  // JSON chunk
  push32(glb, (uint32_t)(json_len + json_pad));
  push32(glb, kChunkTypeJson);
  glb.insert(glb.end(), json_data, json_data + json_len);
  for (std::size_t i = 0; i < json_pad; ++i) glb.push_back(' ');

  // BIN chunk
  if (bin_len > 0) {
    push32(glb, (uint32_t)(bin_len + bin_pad));
    push32(glb, kChunkTypeBin);
    glb.insert(glb.end(), bin_data, bin_data + bin_len);
    for (std::size_t i = 0; i < bin_pad; ++i) glb.push_back(0);
  }

  ArxFtlHandle ftl      = nullptr;
  ArxTeaHandle* teas    = nullptr;
  std::size_t tea_count = 0;
  ArxReturnCode rc      = arx_pistoris_from_glb(glb.data(), glb.size(), "fuzz.glb", &ftl, &teas, &tea_count);

  if (rc == ARX_OK) {
    if (ftl) arx_pistoris_ftl_free(ftl);
    if (teas) arx_pistoris_free_tea_array(teas, tea_count);
  }

  return 0;
}
