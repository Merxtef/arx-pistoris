// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ambiance/location.hpp"
#include "arx_pistoris/animation/location.hpp"
#include "arx_pistoris/base/result.hpp"
#include "arx_pistoris/cinematic/location.hpp"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/json/location.hpp"
#include "arx_pistoris/level/location.hpp"
#include "arx_pistoris/model/location.hpp"
#include "arx_pistoris/model/obj_location.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/resource_io/status.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>

namespace pistoris::resource_io {

enum class ResourceIoOperation : std::uint8_t {
  kOpenMount,
  kRead,
  kWrite,
  kListDirectory,
  kScanCatalog,
  kClassify,
};

using ResourceContentLocation =
    std::variant<AmbianceLocation, AnimationLocation, CinematicLocation, LevelLocation, ModelLocation, GlbLocation,
                 JsonLocation, ObjLocation, AmbLocation, CinLocation, DlfLocation, FtlLocation, FtsLocation,
                 LlfLocation, TeaLocation, AmbBinaryLocation, CinBinaryLocation, DlfBinaryLocation, FtlBinaryLocation,
                 FtsBinaryLocation, LlfBinaryLocation, TeaBinaryLocation>;

struct ResourceIoLocation {
  ResourceIoOperation operation = ResourceIoOperation::kRead;
  std::string resource_path;
  std::filesystem::path native_path;
  std::uint64_t mount_mask = 0;
  std::optional<ResourceContentLocation> content_location;
};

template <class T>
using ResourceIoResult = Result<T, ResourceIoLocation>;

[[nodiscard]] inline const char* errorString(ArxReturnCode code) noexcept {
  return arx_pistoris_resource_io_strerror(code);
}

[[nodiscard]] std::string describeError(const Error<ResourceIoLocation>& error);

}  // namespace pistoris::resource_io
