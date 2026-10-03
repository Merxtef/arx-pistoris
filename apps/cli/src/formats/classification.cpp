// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "formats/classification.h"

#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/tea.hpp"

#include "base/ascii.h"
#include "formats/format.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <string_view>

namespace cli {
namespace {

FileFacts classified(Format format, PayloadKind kind) { return {.format = format, .kind = kind}; }

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

PayloadKind payloadKindFromJsonSuffix(std::string_view path) noexcept {
  if (endsWithAsciiInsensitive(path, ".ftl.json")) return PayloadKind::kFtl;
  if (endsWithAsciiInsensitive(path, ".tea.json")) return PayloadKind::kTea;
  if (endsWithAsciiInsensitive(path, ".fts.json")) return PayloadKind::kFts;
  if (endsWithAsciiInsensitive(path, ".dlf.json")) return PayloadKind::kDlf;
  if (endsWithAsciiInsensitive(path, ".llf.json")) return PayloadKind::kLlf;
  if (endsWithAsciiInsensitive(path, ".amb.json")) return PayloadKind::kAmb;
  return PayloadKind::kUnknown;
}

PayloadKind payloadKindFromSchema(std::string_view schema) noexcept {
  if (schema == "https://arx-tools.github.io/schemas/ftl.schema.json") return PayloadKind::kFtl;
  if (schema == "https://arx-tools.github.io/schemas/tea.schema.json") return PayloadKind::kTea;
  if (schema == "https://arx-tools.github.io/schemas/fts.schema.json") return PayloadKind::kFts;
  if (schema == "https://arx-tools.github.io/schemas/dlf.schema.json") return PayloadKind::kDlf;
  if (schema == "https://arx-tools.github.io/schemas/llf.schema.json") return PayloadKind::kLlf;
  if (schema == "https://arx-tools.github.io/schemas/amb.schema.json") return PayloadKind::kAmb;
  return PayloadKind::kUnknown;
}

bool hasKeys(std::uint32_t actual, std::uint32_t expected) noexcept { return (actual & expected) == expected; }

PayloadKind payloadKindFromRootKeys(std::uint32_t keys) noexcept {
  if (hasKeys(keys, kKeyKeyframes)) return PayloadKind::kTea;
  if (hasKeys(keys, kKeyPolygons | kKeyRoomDistances | kKeyTextureContainers)) return PayloadKind::kFts;
  if (hasKeys(keys, kKeyInteractiveObjects | kKeyFogs | kKeyZones)) return PayloadKind::kDlf;
  if (hasKeys(keys, kKeyLights | kKeyColors)) return PayloadKind::kLlf;
  if (hasKeys(keys, kKeyVertices | kKeyTextureContainers)) return PayloadKind::kFtl;
  if (hasKeys(keys, kKeyTracks)) return PayloadKind::kAmb;
  return PayloadKind::kUnknown;
}

PayloadKind detectJsonPayloadKind(std::span<const std::uint8_t> buffer, std::string_view path) {
  const PayloadKind suffix_kind = payloadKindFromJsonSuffix(path);
  if (suffix_kind != PayloadKind::kUnknown) return suffix_kind;

  JsonHintReader hints;
  if (!nlohmann::json::sax_parse(buffer.begin(), buffer.end(), &hints)) return PayloadKind::kUnknown;
  const PayloadKind schema_kind = payloadKindFromSchema(hints.schema);
  return schema_kind == PayloadKind::kUnknown ? payloadKindFromRootKeys(hints.keys) : schema_kind;
}

bool hasIdentity(std::span<const std::uint8_t> buffer, std::size_t offset, const char* identity,
                 std::size_t size) noexcept {
  return buffer.size() >= offset + size && std::memcmp(buffer.data() + offset, identity, size) == 0;
}

bool isTea(std::span<const std::uint8_t> buffer) noexcept {
  constexpr std::size_t kMagicSize = sizeof(pistoris::kTeaMagic);
  return buffer.size() >= kMagicSize && std::memcmp(buffer.data(), pistoris::kTeaMagic, kMagicSize) == 0;
}

bool isGlb(std::span<const std::uint8_t> buffer) noexcept {
  return buffer.size() >= 4 && std::memcmp(buffer.data(), "glTF", 4) == 0;
}

bool isCin(std::span<const std::uint8_t> buffer) noexcept {
  constexpr std::size_t kMagicSize = pistoris::kCinMagic.size();
  return buffer.size() >= kMagicSize && std::memcmp(buffer.data(), pistoris::kCinMagic.data(), kMagicSize) == 0;
}

bool isAmb(std::span<const std::uint8_t> buffer) noexcept {
  std::uint32_t magic = 0;
  if (buffer.size() < sizeof(magic)) return false;
  std::memcpy(&magic, buffer.data(), sizeof(magic));
  return magic == pistoris::kAmbMagic;
}

bool isDlf(std::span<const std::uint8_t> buffer) noexcept {
  constexpr char kIdentity[] = "DANAE_FILE";
  return hasIdentity(buffer, sizeof(float), kIdentity, sizeof(kIdentity));
}

}  // namespace

FileFacts classifyInput(std::span<const std::uint8_t> buffer, std::string_view path) {
  const Format extension_format = formatFromPath(path);
  if (isTea(buffer)) return classified(Format::kTea, PayloadKind::kTea);
  if (isCin(buffer)) return classified(Format::kCin, PayloadKind::kCin);
  if (isGlb(buffer)) return classified(Format::kGlb, PayloadKind::kGlb);
  if (isAmb(buffer)) return classified(Format::kAmb, PayloadKind::kAmb);
  if (isDlf(buffer)) return classified(Format::kDlf, PayloadKind::kDlf);

  FileFacts facts{.format = extension_format};
  switch (facts.format) {
    case Format::kFtl:
      facts.kind = PayloadKind::kFtl;
      break;
    case Format::kFts:
      facts.kind = PayloadKind::kFts;
      break;
    case Format::kLlf:
      facts.kind = PayloadKind::kLlf;
      break;
    case Format::kObj:
      facts.kind = PayloadKind::kObj;
      break;
    case Format::kJson:
      facts.kind = detectJsonPayloadKind(buffer, path);
      break;
    default:
      facts.kind = PayloadKind::kUnknown;
      break;
  }
  return facts;
}

}  // namespace cli
