// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "formats/format.h"

#include "io/paths.h"

#include <cctype>
#include <cstddef>
#include <cstring>

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

Format formatFromPath(const char* path) {
  const char* ext = fileExtension(path);
  char lower[6] = {};
  std::size_t i = 0;
  for (; ext[i] != '\0' && i + 1 < sizeof(lower); ++i) {
    lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[i])));
  }
  if (ext[i] != '\0') return Format::kUnknown;

  if (std::strcmp(lower, ".ftl") == 0) return Format::kFtl;
  if (std::strcmp(lower, ".fts") == 0) return Format::kFts;
  if (std::strcmp(lower, ".dlf") == 0) return Format::kDlf;
  if (std::strcmp(lower, ".llf") == 0) return Format::kLlf;
  if (std::strcmp(lower, ".tea") == 0) return Format::kTea;
  if (std::strcmp(lower, ".obj") == 0) return Format::kObj;
  if (std::strcmp(lower, ".json") == 0) return Format::kJson;
  if (std::strcmp(lower, ".glb") == 0) return Format::kGlb;
  return Format::kUnknown;
}

}  // namespace cli
