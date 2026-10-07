// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "formats/classification.h"

#include "arx_pistoris/resource_io/document.hpp"

#include "formats/format.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace cli {
namespace {

Format cliFormat(pistoris::resource_io::ResourceFormat format) noexcept {
  using pistoris::resource_io::ResourceFormat;
  switch (format) {
    case ResourceFormat::kFtl:
      return Format::kFtl;
    case ResourceFormat::kTea:
      return Format::kTea;
    case ResourceFormat::kFts:
      return Format::kFts;
    case ResourceFormat::kDlf:
      return Format::kDlf;
    case ResourceFormat::kLlf:
      return Format::kLlf;
    case ResourceFormat::kAmb:
      return Format::kAmb;
    case ResourceFormat::kCin:
      return Format::kCin;
    case ResourceFormat::kObj:
      return Format::kObj;
    case ResourceFormat::kJson:
      return Format::kJson;
    case ResourceFormat::kGlb:
      return Format::kGlb;
    case ResourceFormat::kMtl:
    case ResourceFormat::kUnknown:
      return Format::kUnknown;
  }
  return Format::kUnknown;
}

PayloadKind cliPayload(pistoris::resource_io::ResourcePayload payload) noexcept {
  using pistoris::resource_io::ResourcePayload;
  switch (payload) {
    case ResourcePayload::kModel:
      return PayloadKind::kFtl;
    case ResourcePayload::kAnimation:
      return PayloadKind::kTea;
    case ResourcePayload::kLevelGeometry:
      return PayloadKind::kFts;
    case ResourcePayload::kLevelScene:
      return PayloadKind::kDlf;
    case ResourcePayload::kLevelLighting:
      return PayloadKind::kLlf;
    case ResourcePayload::kAmbiance:
      return PayloadKind::kAmb;
    case ResourcePayload::kCinematic:
      return PayloadKind::kCin;
    case ResourcePayload::kObj:
      return PayloadKind::kObj;
    case ResourcePayload::kGlb:
      return PayloadKind::kGlb;
    case ResourcePayload::kUnknown:
      return PayloadKind::kUnknown;
  }
  return PayloadKind::kUnknown;
}

}  // namespace

FileFacts classifyInput(std::span<const std::uint8_t> buffer, std::string_view path) {
  const auto classified = pistoris::resource_io::classifyResource(buffer, path);
  if (!classified) return {};
  return {.format = cliFormat(classified->format), .kind = cliPayload(classified->payload)};
}

}  // namespace cli
