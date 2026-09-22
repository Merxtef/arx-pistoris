// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "formats/format.h"

#include "base/ascii.h"
#include "base/resource_path.h"

#include <cstddef>
#include <string>
#include <string_view>

namespace cli {

const char* formatName(Format format) {
  switch (format) {
    case Format::kUnset:
      return "unset";
    case Format::kFtl:
      return "FTL";
    case Format::kFts:
      return "FTS";
    case Format::kDlf:
      return "DLF";
    case Format::kLlf:
      return "LLF";
    case Format::kTea:
      return "TEA";
    case Format::kAmb:
      return "AMB";
    case Format::kCin:
      return "CIN";
    case Format::kObj:
      return "OBJ";
    case Format::kJson:
      return "JSON";
    case Format::kGlb:
      return "GLB";
    default:
      return "unknown";
  }
}

Format formatFromExtension(std::string_view extension) noexcept {
  if (equalAsciiInsensitive(extension, ".ftl")) return Format::kFtl;
  if (equalAsciiInsensitive(extension, ".fts")) return Format::kFts;
  if (equalAsciiInsensitive(extension, ".dlf")) return Format::kDlf;
  if (equalAsciiInsensitive(extension, ".llf")) return Format::kLlf;
  if (equalAsciiInsensitive(extension, ".tea")) return Format::kTea;
  if (equalAsciiInsensitive(extension, ".amb")) return Format::kAmb;
  if (equalAsciiInsensitive(extension, ".cin")) return Format::kCin;
  if (equalAsciiInsensitive(extension, ".obj")) return Format::kObj;
  if (equalAsciiInsensitive(extension, ".json")) return Format::kJson;
  if (equalAsciiInsensitive(extension, ".glb")) return Format::kGlb;
  return Format::kUnknown;
}

Format formatFromPath(std::string_view path) noexcept {
  path = resourceFilename(path);
  const std::size_t dot = path.find_last_of('.');
  return formatFromExtension(dot == std::string_view::npos ? std::string_view{} : path.substr(dot));
}

std::string resourceFormatStem(std::string_view path) {
  path = resourceFilename(path);
  const std::size_t dot = path.find_last_of('.');
  if (dot != std::string_view::npos && formatFromExtension(path.substr(dot)) != Format::kUnknown)
    path = path.substr(0, dot);
  return std::string(path);
}

}  // namespace cli
