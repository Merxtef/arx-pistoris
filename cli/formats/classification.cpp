// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "formats/classification.h"

#include "arx_pistoris/native/tea.hpp"

#include "formats/format.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

namespace cli {
namespace {

FileFacts classified(Format format, PayloadKind kind) { return {.format = format, .kind = kind}; }

}  // namespace

static PayloadKind detectJsonPayloadKind(const std::vector<std::uint8_t>& buf) {
  std::string_view text(reinterpret_cast<const char*>(buf.data()), buf.size());
  if (text.find("https://arx-tools.github.io/schemas/ftl.schema.json") != std::string_view::npos) {
    return PayloadKind::kFtl;
  }
  if (text.find("https://arx-tools.github.io/schemas/tea.schema.json") != std::string_view::npos) {
    return PayloadKind::kTea;
  }
  if (text.find("https://arx-tools.github.io/schemas/fts.schema.json") != std::string_view::npos) {
    return PayloadKind::kFts;
  }
  if (text.find("https://arx-tools.github.io/schemas/dlf.schema.json") != std::string_view::npos) {
    return PayloadKind::kDlf;
  }
  if (text.find("https://arx-tools.github.io/schemas/llf.schema.json") != std::string_view::npos) {
    return PayloadKind::kLlf;
  }
  if (text.find("\"keyframes\"") != std::string_view::npos &&
      text.find("\"totalNumberOfFrames\"") != std::string_view::npos) {
    return PayloadKind::kTea;
  }
  if (text.find("\"polygons\"") != std::string_view::npos && text.find("\"roomDistances\"") != std::string_view::npos &&
      text.find("\"textureContainers\"") != std::string_view::npos) {
    return PayloadKind::kFts;
  }
  if (text.find("\"interactiveObjects\"") != std::string_view::npos &&
      text.find("\"fogs\"") != std::string_view::npos && text.find("\"zones\"") != std::string_view::npos) {
    return PayloadKind::kDlf;
  }
  if (text.find("\"lights\"") != std::string_view::npos && text.find("\"colors\"") != std::string_view::npos &&
      text.find("\"numberOfPolygonsInFTS\"") != std::string_view::npos) {
    return PayloadKind::kLlf;
  }
  if (text.find("\"vertices\"") != std::string_view::npos &&
      text.find("\"textureContainers\"") != std::string_view::npos) {
    return PayloadKind::kFtl;
  }
  return PayloadKind::kUnknown;
}

static bool hasIdentity(const std::vector<std::uint8_t>& buf, std::size_t offset, const char* identity,
                        std::size_t size) {
  return buf.size() >= offset + size && std::memcmp(buf.data() + offset, identity, size) == 0;
}

static bool isTea(const std::vector<std::uint8_t>& buf) {
  constexpr std::size_t kMagicSize = sizeof(pistoris::kTeaMagic);
  return buf.size() >= kMagicSize && std::memcmp(buf.data(), pistoris::kTeaMagic, kMagicSize) == 0;
}

static bool isGlb(const std::vector<std::uint8_t>& buf) {
  return buf.size() >= 4 && std::memcmp(buf.data(), "glTF", 4) == 0;
}

static bool isDlf(const std::vector<std::uint8_t>& buf) {
  constexpr char kIdentity[] = "DANAE_FILE";
  return hasIdentity(buf, sizeof(float), kIdentity, sizeof(kIdentity));
}

FileFacts classifyInput(const std::vector<std::uint8_t>& buf, const char* path) {
  Format extension_format = formatFromPath(path);
  if (isTea(buf)) return classified(Format::kTea, PayloadKind::kTea);
  if (isGlb(buf)) return classified(Format::kGlb, PayloadKind::kGlb);
  if (isDlf(buf)) return classified(Format::kDlf, PayloadKind::kDlf);

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
      facts.kind = detectJsonPayloadKind(buf);
      break;
    default:
      facts.kind = PayloadKind::kUnknown;
      break;
  }
  return facts;
}

}  // namespace cli
