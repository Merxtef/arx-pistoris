// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/common_data.hpp"
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

#ifndef ARX_PISTORIS_CPP_API
#error "pistoris.hpp requires the arx_pistoris_cpp target."
#endif

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {

using Ftl = ftl::Data;
using Tea = tea::Data;

// Ftl::extras is derived/cache-like state for advanced callers. Avoid relying on it unless you have just validated the
// model. Manual Ftl edits may leave it stale, and future versions may change these derived fields.

struct Obj {
  std::string text;
  std::string mtl;
};

const char* version();
const char* buildTimeString();
const char* errorString(ArxReturnCode rc);

void setLogCallback(ArxLogFn fn, void* userdata);

// --- Native Binary I/O (FTL / TEA) ---

ArxReturnCode readFtl(std::span<const std::uint8_t> data, Ftl& out);
ArxReturnCode writeFtl(const Ftl& ftl, std::vector<std::uint8_t>& out);

ArxReturnCode readTea(std::span<const std::uint8_t> data, Tea& out);
ArxReturnCode writeTea(const Tea& tea, std::vector<std::uint8_t>& out);

// --- External Conversion (OBJ / GLB / JSON) ---

ArxReturnCode exportObj(const Ftl& ftl, std::string_view stem, Obj& out);
ArxReturnCode importObj(std::string_view obj, std::string_view mtl, std::string_view filename, Ftl& out);
ArxReturnCode importObj(const Obj& obj, std::string_view filename, Ftl& out);

ArxReturnCode exportGlb(const Ftl& ftl, std::span<const Tea> teas, std::vector<std::uint8_t>& out);
ArxReturnCode importGlb(std::span<const std::uint8_t> data, std::string_view filename, Ftl& out_ftl,
                        std::vector<Tea>& out_teas);

ArxReturnCode exportJson(const Ftl& ftl, std::string& out, bool pretty = false);
ArxReturnCode importJson(std::string_view json, Ftl& out);
ArxReturnCode exportJson(const Tea& tea, std::string& out, bool pretty = false);
ArxReturnCode importJson(std::string_view json, Tea& out);

// --- Utilities ---

ArxReturnCode validate(const Ftl& ftl);
ArxReturnCode validate(const Tea& tea);

AffineXform makeAffineXform(float rx_deg, float ry_deg, float rz_deg, float sx, float sy, float sz, float tx, float ty,
                            float tz);

ArxReturnCode applyTransform(Ftl& ftl, const AffineXform& xform);
ArxReturnCode applyTransform(Tea& tea, const AffineXform& xform);

ArxReturnCode overwriteTexturePaths(Ftl& ftl, std::string_view path);
ArxReturnCode snapFtlBoneOriginsToReference(Ftl& target, const Ftl& reference);
ArxReturnCode snapFtlActionPointsToReference(Ftl& target, const Ftl& reference);
ArxReturnCode copyFtlSyntheticSelectionAffiliations(Ftl& target, const Ftl& reference);

}  // namespace pistoris
