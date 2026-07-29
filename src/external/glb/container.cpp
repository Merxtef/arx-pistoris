// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "container.h"

#include "arx_pistoris/pistoris_types.h"

#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <utility>

namespace pistoris::glb {
namespace {

constexpr std::uint32_t kGlbMagic = 0x46546c67;
constexpr std::uint32_t kGlbVersion = 2;
constexpr std::uint32_t kChunkJson = 0x4e4f534a;

std::uint32_t read32(std::span<const std::uint8_t> data, std::size_t offset) {
  std::uint32_t value = 0;
  std::memcpy(&value, data.data() + offset, sizeof(value));
  return value;
}

ArxReturnCode preflight(std::span<const std::uint8_t> bytes) {
  if (bytes.size() < 4) return ARX_UNEXPECTED_EOF;
  if (read32(bytes, 0) != kGlbMagic) return ARX_INVALID_IDENTIFIER;
  if (bytes.size() < 12) return ARX_UNEXPECTED_EOF;
  if (read32(bytes, 4) != kGlbVersion) return ARX_GLB_BAD_FORMAT;
  std::uint32_t declared = read32(bytes, 8);
  if (declared > bytes.size()) return ARX_UNEXPECTED_EOF;
  if (declared != bytes.size() || declared < 20) return ARX_GLB_BAD_FORMAT;
  if (read32(bytes, 16) != kChunkJson) return ARX_GLB_BAD_FORMAT;
  std::uint32_t json_size = read32(bytes, 12);
  if (json_size > declared - 20) return ARX_UNEXPECTED_EOF;
  return ARX_OK;
}

ArxReturnCode mapParseResult(cgltf_result result) {
  switch (result) {
    case cgltf_result_success:
      return ARX_OK;
    case cgltf_result_data_too_short:
      return ARX_UNEXPECTED_EOF;
    case cgltf_result_out_of_memory:
      return ARX_BAD_ALLOC;
    case cgltf_result_unknown_format:
    case cgltf_result_invalid_json:
    case cgltf_result_invalid_gltf:
    case cgltf_result_invalid_options:
    case cgltf_result_legacy_gltf:
      return ARX_GLB_BAD_FORMAT;
    case cgltf_result_file_not_found:
    case cgltf_result_io_error:
      return ARX_GLB_UNSUPPORTED_FEATURE;
    default:
      return ARX_GLB_BAD_FORMAT;
  }
}

}  // namespace

Asset::~Asset() {
  if (data_ != nullptr) cgltf_free(data_);
}

Asset::Asset(Asset&& other) noexcept : data_(other.data_) { other.data_ = nullptr; }

Asset& Asset::operator=(Asset&& other) noexcept {
  if (this == &other) return *this;
  if (data_ != nullptr) cgltf_free(data_);
  data_ = other.data_;
  other.data_ = nullptr;
  return *this;
}

ArxReturnCode parse(std::span<const std::uint8_t> bytes, Asset& out) {
  ArxReturnCode rc = preflight(bytes);
  if (rc != ARX_OK) return rc;
  if (bytes.size() > std::numeric_limits<cgltf_size>::max()) return ARX_GLB_BAD_FORMAT;

  cgltf_options options{};
  options.type = cgltf_file_type_glb;
  cgltf_data* parsed = nullptr;
  cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &parsed);
  if (result != cgltf_result_success) return mapParseResult(result);

  Asset tmp;
  tmp.data_ = parsed;
  for (cgltf_size i = 0; i < parsed->buffers_count; ++i) {
    if (parsed->buffers[i].uri != nullptr) return ARX_GLB_UNSUPPORTED_FEATURE;
  }
  result = cgltf_load_buffers(&options, parsed, nullptr);
  if (result != cgltf_result_success) return mapParseResult(result);
  result = cgltf_validate(parsed);
  if (result != cgltf_result_success) return mapParseResult(result);

  out = std::move(tmp);
  return ARX_OK;
}

}  // namespace pistoris::glb
