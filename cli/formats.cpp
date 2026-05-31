// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "formats.h"

#include "io.h"

#include <cctype>
#include <cstring>
#include <string_view>

namespace cli {

const char* formatName(Format fmt) {
  switch (fmt) {
    case Format::kUnset:
      return "unset";
    case Format::kFtl:
      return "FTL";
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
  char lower[6]   = {};
  std::size_t i   = 0;
  for (; ext[i] != '\0' && i + 1 < sizeof(lower); ++i) {
    lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[i])));
  }
  if (ext[i] != '\0') return Format::kUnknown;

  if (std::strcmp(lower, ".ftl") == 0) return Format::kFtl;
  if (std::strcmp(lower, ".tea") == 0) return Format::kTea;
  if (std::strcmp(lower, ".obj") == 0) return Format::kObj;
  if (std::strcmp(lower, ".json") == 0) return Format::kJson;
  if (std::strcmp(lower, ".glb") == 0) return Format::kGlb;
  return Format::kUnknown;
}

static RouteKind detectJsonRouteKind(const std::vector<std::uint8_t>& buf) {
  std::string_view text(reinterpret_cast<const char*>(buf.data()), buf.size());
  if (text.find("\"keyframes\"") != std::string_view::npos &&
      text.find("\"totalNumberOfFrames\"") != std::string_view::npos) {
    return RouteKind::kAnimation;
  }
  if (text.find("\"vertices\"") != std::string_view::npos &&
      text.find("\"textureContainers\"") != std::string_view::npos) {
    return RouteKind::kModel;
  }
  return RouteKind::kUnknown;
}

Route detectRoute(const std::vector<std::uint8_t>& buf, const char* path) {
  if (isFtl(buf)) return {.kind = RouteKind::kModel, .input = Format::kFtl};
  if (isTea(buf)) return {.kind = RouteKind::kAnimation, .input = Format::kTea};
  if (isGlb(buf)) return {.kind = RouteKind::kModel, .input = Format::kGlb};

  Route route;
  route.input = formatFromPath(path);

  switch (route.input) {
    case Format::kObj:
      route.kind = RouteKind::kModel;
      break;
    case Format::kJson:
      route.kind = detectJsonRouteKind(buf);
      break;
    default:
      route.kind = RouteKind::kUnknown;
      break;
  }

  return route;
}

}  // namespace cli
