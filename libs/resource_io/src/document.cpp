// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/resource_io/document.hpp"

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/resource_io/location.hpp"

#include "result_failure.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <string_view>

namespace {

using pistoris::resource_io::ResourceClassification;
using pistoris::resource_io::ResourceFormat;
using pistoris::resource_io::ResourcePayload;

char lowerAscii(char value) noexcept {
  return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

bool endsWithAsciiInsensitive(std::string_view value, std::string_view suffix) noexcept {
  if (value.size() < suffix.size()) return false;
  value.remove_prefix(value.size() - suffix.size());
  for (std::size_t index = 0; index < value.size(); ++index)
    if (lowerAscii(value[index]) != lowerAscii(suffix[index])) return false;
  return true;
}

ResourceFormat formatFromPath(std::string_view path) noexcept {
  if (endsWithAsciiInsensitive(path, ".ftl")) return ResourceFormat::kFtl;
  if (endsWithAsciiInsensitive(path, ".tea")) return ResourceFormat::kTea;
  if (endsWithAsciiInsensitive(path, ".fts")) return ResourceFormat::kFts;
  if (endsWithAsciiInsensitive(path, ".dlf")) return ResourceFormat::kDlf;
  if (endsWithAsciiInsensitive(path, ".llf")) return ResourceFormat::kLlf;
  if (endsWithAsciiInsensitive(path, ".amb")) return ResourceFormat::kAmb;
  if (endsWithAsciiInsensitive(path, ".cin")) return ResourceFormat::kCin;
  if (endsWithAsciiInsensitive(path, ".obj")) return ResourceFormat::kObj;
  if (endsWithAsciiInsensitive(path, ".mtl")) return ResourceFormat::kMtl;
  if (endsWithAsciiInsensitive(path, ".json")) return ResourceFormat::kJson;
  if (endsWithAsciiInsensitive(path, ".glb")) return ResourceFormat::kGlb;
  return ResourceFormat::kUnknown;
}

ResourcePayload payloadFromFormat(ResourceFormat format) noexcept {
  switch (format) {
    case ResourceFormat::kFtl:
      return ResourcePayload::kModel;
    case ResourceFormat::kTea:
      return ResourcePayload::kAnimation;
    case ResourceFormat::kFts:
      return ResourcePayload::kLevelGeometry;
    case ResourceFormat::kDlf:
      return ResourcePayload::kLevelScene;
    case ResourceFormat::kLlf:
      return ResourcePayload::kLevelLighting;
    case ResourceFormat::kAmb:
      return ResourcePayload::kAmbiance;
    case ResourceFormat::kCin:
      return ResourcePayload::kCinematic;
    case ResourceFormat::kObj:
      return ResourcePayload::kObj;
    case ResourceFormat::kGlb:
      return ResourcePayload::kGlb;
    default:
      return ResourcePayload::kUnknown;
  }
}

enum JsonRootKey : std::uint16_t {
  kKeyNone = 0,
  kKeyKeyframes = 1U << 0U,
  kKeyPolygons = 1U << 1U,
  kKeyRoomDistances = 1U << 2U,
  kKeyTextureContainers = 1U << 3U,
  kKeyInteractiveObjects = 1U << 4U,
  kKeyFogs = 1U << 5U,
  kKeyZones = 1U << 6U,
  kKeyLights = 1U << 7U,
  kKeyColors = 1U << 8U,
  kKeyVertices = 1U << 9U,
  kKeyTracks = 1U << 10U,
};

std::uint32_t rootKey(std::string_view key) noexcept {
  if (key == "keyframes") return kKeyKeyframes;
  if (key == "polygons") return kKeyPolygons;
  if (key == "roomDistances") return kKeyRoomDistances;
  if (key == "textureContainers") return kKeyTextureContainers;
  if (key == "interactiveObjects") return kKeyInteractiveObjects;
  if (key == "fogs") return kKeyFogs;
  if (key == "zones") return kKeyZones;
  if (key == "lights") return kKeyLights;
  if (key == "colors") return kKeyColors;
  if (key == "vertices") return kKeyVertices;
  if (key == "tracks") return kKeyTracks;
  return kKeyNone;
}

class JsonHintReader final : public nlohmann::json_sax<nlohmann::json> {
 public:
  bool null() override { return scalar(); }
  bool boolean(bool) override { return scalar(); }
  bool number_integer(number_integer_t) override { return scalar(); }
  bool number_unsigned(number_unsigned_t) override { return scalar(); }
  bool number_float(number_float_t, const string_t&) override { return scalar(); }
  bool string(string_t& value) override {
    if (capture_schema_) schema = value;
    return scalar();
  }
  bool binary(binary_t&) override { return scalar(); }
  bool start_object(std::size_t) override {
    beginContainer();
    if (depth_ == 1U) root_object_ = true;
    return true;
  }
  bool key(string_t& value) override {
    capture_schema_ = root_object_ && depth_ == 1U && value == "$schema";
    if (root_object_ && depth_ == 1U) keys |= rootKey(value);
    return true;
  }
  bool end_object() override {
    capture_schema_ = false;
    --depth_;
    return true;
  }
  bool start_array(std::size_t) override {
    beginContainer();
    return true;
  }
  bool end_array() override {
    capture_schema_ = false;
    --depth_;
    return true;
  }
  bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception&) override { return false; }

  std::string schema;
  std::uint32_t keys = kKeyNone;

 private:
  bool scalar() noexcept {
    capture_schema_ = false;
    return true;
  }
  void beginContainer() noexcept {
    capture_schema_ = false;
    ++depth_;
  }

  std::size_t depth_ = 0;
  bool root_object_ = false;
  bool capture_schema_ = false;
};

ResourcePayload payloadFromJsonSuffix(std::string_view path) noexcept {
  if (endsWithAsciiInsensitive(path, ".ftl.json")) return ResourcePayload::kModel;
  if (endsWithAsciiInsensitive(path, ".tea.json")) return ResourcePayload::kAnimation;
  if (endsWithAsciiInsensitive(path, ".fts.json")) return ResourcePayload::kLevelGeometry;
  if (endsWithAsciiInsensitive(path, ".dlf.json")) return ResourcePayload::kLevelScene;
  if (endsWithAsciiInsensitive(path, ".llf.json")) return ResourcePayload::kLevelLighting;
  if (endsWithAsciiInsensitive(path, ".amb.json")) return ResourcePayload::kAmbiance;
  return ResourcePayload::kUnknown;
}

ResourcePayload payloadFromSchema(std::string_view schema) noexcept {
  if (schema == "https://arx-tools.github.io/schemas/ftl.schema.json") return ResourcePayload::kModel;
  if (schema == "https://arx-tools.github.io/schemas/tea.schema.json") return ResourcePayload::kAnimation;
  if (schema == "https://arx-tools.github.io/schemas/fts.schema.json") return ResourcePayload::kLevelGeometry;
  if (schema == "https://arx-tools.github.io/schemas/dlf.schema.json") return ResourcePayload::kLevelScene;
  if (schema == "https://arx-tools.github.io/schemas/llf.schema.json") return ResourcePayload::kLevelLighting;
  if (schema == "https://arx-tools.github.io/schemas/amb.schema.json") return ResourcePayload::kAmbiance;
  return ResourcePayload::kUnknown;
}

bool hasKeys(std::uint32_t actual, std::uint32_t expected) noexcept { return (actual & expected) == expected; }

ResourcePayload payloadFromRootKeys(std::uint32_t keys) noexcept {
  if (hasKeys(keys, kKeyKeyframes)) return ResourcePayload::kAnimation;
  if (hasKeys(keys, kKeyPolygons | kKeyRoomDistances | kKeyTextureContainers)) return ResourcePayload::kLevelGeometry;
  if (hasKeys(keys, kKeyInteractiveObjects | kKeyFogs | kKeyZones)) return ResourcePayload::kLevelScene;
  if (hasKeys(keys, kKeyLights | kKeyColors)) return ResourcePayload::kLevelLighting;
  if (hasKeys(keys, kKeyVertices | kKeyTextureContainers)) return ResourcePayload::kModel;
  if (hasKeys(keys, kKeyTracks)) return ResourcePayload::kAmbiance;
  return ResourcePayload::kUnknown;
}

ResourcePayload jsonPayload(std::span<const std::uint8_t> data, std::string_view path) {
  const ResourcePayload suffix = payloadFromJsonSuffix(path);
  if (suffix != ResourcePayload::kUnknown) return suffix;
  JsonHintReader hints;
  if (!nlohmann::json::sax_parse(data.begin(), data.end(), &hints)) return ResourcePayload::kUnknown;
  const ResourcePayload schema = payloadFromSchema(hints.schema);
  return schema == ResourcePayload::kUnknown ? payloadFromRootKeys(hints.keys) : schema;
}

bool hasIdentity(std::span<const std::uint8_t> data, std::size_t offset, const char* identity,
                 std::size_t size) noexcept {
  return data.size() >= offset + size && std::memcmp(data.data() + offset, identity, size) == 0;
}

bool isTea(std::span<const std::uint8_t> data) noexcept {
  return data.size() >= sizeof(pistoris::kTeaMagic) &&
         std::memcmp(data.data(), pistoris::kTeaMagic, sizeof(pistoris::kTeaMagic)) == 0;
}

bool isCin(std::span<const std::uint8_t> data) noexcept {
  return data.size() >= pistoris::kCinMagic.size() &&
         std::memcmp(data.data(), pistoris::kCinMagic.data(), pistoris::kCinMagic.size()) == 0;
}

bool isGlb(std::span<const std::uint8_t> data) noexcept {
  return data.size() >= 4U && std::memcmp(data.data(), "glTF", 4U) == 0;
}

bool isAmb(std::span<const std::uint8_t> data) noexcept {
  std::uint32_t magic = 0;
  if (data.size() < sizeof(magic)) return false;
  std::memcpy(&magic, data.data(), sizeof(magic));
  return magic == pistoris::kAmbMagic;
}

bool isDlf(std::span<const std::uint8_t> data) noexcept {
  constexpr char kIdentity[] = "DANAE_FILE";
  return hasIdentity(data, sizeof(float), kIdentity, sizeof(kIdentity));
}

}  // namespace

namespace pistoris::resource_io {

ResourceIoResult<ResourceClassification> classifyResource(std::span<const std::uint8_t> data,
                                                          std::string_view path) noexcept {
  try {
    if (isTea(data))
      return ResourceIoResult<ResourceClassification>::success({ResourceFormat::kTea, ResourcePayload::kAnimation});
    if (isCin(data))
      return ResourceIoResult<ResourceClassification>::success({ResourceFormat::kCin, ResourcePayload::kCinematic});
    if (isGlb(data))
      return ResourceIoResult<ResourceClassification>::success({ResourceFormat::kGlb, ResourcePayload::kGlb});
    if (isAmb(data))
      return ResourceIoResult<ResourceClassification>::success({ResourceFormat::kAmb, ResourcePayload::kAmbiance});
    if (isDlf(data))
      return ResourceIoResult<ResourceClassification>::success({ResourceFormat::kDlf, ResourcePayload::kLevelScene});
    const ResourceFormat format = formatFromPath(path);
    return ResourceIoResult<ResourceClassification>::success(
        {format, format == ResourceFormat::kJson ? jsonPayload(data, path) : payloadFromFormat(format)});
  } catch (const std::bad_alloc&) {
    return detail::resourceIoFailure<ResourceClassification>(ARX_BAD_ALLOC, ResourceIoOperation::kClassify, {}, {}, 0);
  } catch (...) {
    return detail::resourceIoFailure<ResourceClassification>(
        ARX_INTERNAL_ERROR, ResourceIoOperation::kClassify, {}, {}, 0);
  }
}

const char* resourceFormatName(ResourceFormat format) noexcept {
  switch (format) {
    case ResourceFormat::kFtl:
      return "FTL";
    case ResourceFormat::kTea:
      return "TEA";
    case ResourceFormat::kFts:
      return "FTS";
    case ResourceFormat::kDlf:
      return "DLF";
    case ResourceFormat::kLlf:
      return "LLF";
    case ResourceFormat::kAmb:
      return "AMB";
    case ResourceFormat::kCin:
      return "CIN";
    case ResourceFormat::kObj:
      return "OBJ";
    case ResourceFormat::kMtl:
      return "MTL";
    case ResourceFormat::kJson:
      return "JSON";
    case ResourceFormat::kGlb:
      return "GLB";
    case ResourceFormat::kUnknown:
      break;
  }
  return "unknown";
}

}  // namespace pistoris::resource_io
