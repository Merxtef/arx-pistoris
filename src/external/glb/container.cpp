// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "container.h"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/runtime/types.h"

#include "utils/log.h"

#include <cstdint>
#include <cstring>
#include <format>
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

void decode(char* value) {
  if (value != nullptr) cgltf_decode_string(value);
}

void decodeExtensions(cgltf_extension* extensions, cgltf_size count) {
  for (cgltf_size index = 0; index < count; ++index) decode(extensions[index].name);
}

template <typename T>
void decodeItem(T& item) {
  if constexpr (requires { item.name; }) decode(item.name);
  if constexpr (requires { item.uri; }) decode(item.uri);
  if constexpr (requires { item.mime_type; }) decode(item.mime_type);
  if constexpr (requires { item.extensions; }) decodeExtensions(item.extensions, item.extensions_count);
}

template <typename T>
void decodeItems(T* items, cgltf_size count) {
  for (cgltf_size index = 0; index < count; ++index) decodeItem(items[index]);
}

void decodeAttributes(cgltf_attribute* attributes, cgltf_size count) {
  for (cgltf_size index = 0; index < count; ++index) decode(attributes[index].name);
}

void decodeStrings(cgltf_data& data) {
  decode(data.asset.copyright);
  decode(data.asset.generator);
  decode(data.asset.version);
  decode(data.asset.min_version);
  decodeExtensions(data.asset.extensions, data.asset.extensions_count);

  decodeItems(data.buffers, data.buffers_count);
  decodeItems(data.buffer_views, data.buffer_views_count);
  decodeItems(data.accessors, data.accessors_count);
  decodeItems(data.images, data.images_count);
  decodeItems(data.samplers, data.samplers_count);
  decodeItems(data.textures, data.textures_count);
  decodeItems(data.materials, data.materials_count);
  decodeItems(data.skins, data.skins_count);
  decodeItems(data.cameras, data.cameras_count);
  decodeItems(data.lights, data.lights_count);
  decodeItems(data.scenes, data.scenes_count);
  decodeItems(data.variants, data.variants_count);

  for (cgltf_size animation_index = 0; animation_index < data.animations_count; ++animation_index) {
    cgltf_animation& animation = data.animations[animation_index];
    decodeItem(animation);
    decodeItems(animation.samplers, animation.samplers_count);
    decodeItems(animation.channels, animation.channels_count);
  }

  for (cgltf_size mesh_index = 0; mesh_index < data.meshes_count; ++mesh_index) {
    cgltf_mesh& mesh = data.meshes[mesh_index];
    decodeItem(mesh);
    for (cgltf_size index = 0; index < mesh.target_names_count; ++index) decode(mesh.target_names[index]);
    for (cgltf_size primitive_index = 0; primitive_index < mesh.primitives_count; ++primitive_index) {
      cgltf_primitive& primitive = mesh.primitives[primitive_index];
      decodeAttributes(primitive.attributes, primitive.attributes_count);
      decodeAttributes(primitive.draco_mesh_compression.attributes, primitive.draco_mesh_compression.attributes_count);
      for (cgltf_size target_index = 0; target_index < primitive.targets_count; ++target_index)
        decodeAttributes(primitive.targets[target_index].attributes, primitive.targets[target_index].attributes_count);
      decodeExtensions(primitive.extensions, primitive.extensions_count);
    }
  }

  for (cgltf_size node_index = 0; node_index < data.nodes_count; ++node_index) {
    cgltf_node& node = data.nodes[node_index];
    decodeItem(node);
    decodeAttributes(node.mesh_gpu_instancing.attributes, node.mesh_gpu_instancing.attributes_count);
  }

  decodeExtensions(data.data_extensions, data.data_extensions_count);
  for (cgltf_size index = 0; index < data.extensions_used_count; ++index) decode(data.extensions_used[index]);
  for (cgltf_size index = 0; index < data.extensions_required_count; ++index) decode(data.extensions_required[index]);
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
  if (rc != ARX_OK) {
    log(ARX_LOG_DEBUG, "GLB container failure: preflight returned code {}", rc);
    return rc;
  }
  if (bytes.size() > std::numeric_limits<cgltf_size>::max()) {
    log(ARX_LOG_DEBUG, "GLB container failure: input exceeds cgltf size range");
    return ARX_GLB_BAD_FORMAT;
  }

  cgltf_options options{};
  options.type = cgltf_file_type_glb;
  cgltf_data* parsed = nullptr;
  cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &parsed);
  if (result != cgltf_result_success) {
    rc = mapParseResult(result);
    log(ARX_LOG_DEBUG, "GLB container failure: cgltf parse result {} mapped to code {}", static_cast<int>(result), rc);
    return rc;
  }

  Asset tmp;
  tmp.data_ = parsed;
  decodeStrings(*parsed);
  for (cgltf_size i = 0; i < parsed->buffers_count; ++i) {
    if (parsed->buffers[i].uri != nullptr) {
      log(ARX_LOG_DEBUG, "GLB container failure: buffer {} references an external URI", i);
      return ARX_GLB_UNSUPPORTED_FEATURE;
    }
  }
  result = cgltf_load_buffers(&options, parsed, nullptr);
  if (result != cgltf_result_success) {
    rc = mapParseResult(result);
    log(ARX_LOG_DEBUG,
        "GLB container failure: buffer loading result {} mapped to code {}",
        static_cast<int>(result),
        rc);
    return rc;
  }
  result = cgltf_validate(parsed);
  if (result != cgltf_result_success) {
    rc = mapParseResult(result);
    log(ARX_LOG_DEBUG,
        "GLB container failure: cgltf validation result {} mapped to code {}",
        static_cast<int>(result),
        rc);
    return rc;
  }

  out = std::move(tmp);
  return ARX_OK;
}

}  // namespace pistoris::glb
