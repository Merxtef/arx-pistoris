// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"      // IWYU pragma: export
#include "arx_pistoris/flags.h"           // IWYU pragma: export
#include "arx_pistoris/level.hpp"         // IWYU pragma: export
#include "arx_pistoris/level/bake.hpp"    // IWYU pragma: export
#include "arx_pistoris/native/dlf.hpp"    // IWYU pragma: export
#include "arx_pistoris/native/ftl.hpp"    // IWYU pragma: export
#include "arx_pistoris/native/fts.hpp"    // IWYU pragma: export
#include "arx_pistoris/native/llf.hpp"    // IWYU pragma: export
#include "arx_pistoris/native/tea.hpp"    // IWYU pragma: export
#include "arx_pistoris/paths.hpp"         // IWYU pragma: export
#include "arx_pistoris/pistoris_types.h"  // IWYU pragma: export

#ifndef ARX_PISTORIS_CPP_API
#error "pistoris.hpp requires the arx_pistoris_cpp target."
#endif

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {

using Dlf = dlf::Data;
using Ftl = ftl::Data;
using Fts = fts::Data;
using Llf = llf::Data;
using Tea = tea::Data;

// Derived FTL cache; valid only after validateFtl and invalidated by manual edits

struct Obj {
  std::string text;
  std::string mtl;
};

struct DlfWriteOptions {
  const Llf* embedded_lighting = nullptr;
  std::string_view signer;
};

struct LlfWriteOptions {
  std::string_view signer;
};

const char* version();
const char* buildTimeString();
const char* errorString(ArxReturnCode rc);

void setLogCallback(ArxLogFn fn, void* userdata);

// --- Native Binary I/O ---

ArxReturnCode readDlf(std::span<const std::uint8_t> data, Dlf& out, std::optional<Llf>* embedded_lighting = nullptr);
ArxReturnCode writeDlf(const Dlf& dlf, const DlfWriteOptions& options, std::vector<std::uint8_t>& out,
                       bool compress = true);

ArxReturnCode readFtl(std::span<const std::uint8_t> data, Ftl& out);
ArxReturnCode writeFtl(const Ftl& ftl, std::vector<std::uint8_t>& out, bool compress = true);

ArxReturnCode readFts(std::span<const std::uint8_t> data, Fts& out);
ArxReturnCode writeFts(const Fts& fts, std::vector<std::uint8_t>& out, bool compress = true);

ArxReturnCode readLlf(std::span<const std::uint8_t> data, Llf& out);
ArxReturnCode writeLlf(const Llf& llf, std::vector<std::uint8_t>& out, bool compress = true);
ArxReturnCode writeLlf(const Llf& llf, const LlfWriteOptions& options, std::vector<std::uint8_t>& out,
                       bool compress = true);

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
ArxReturnCode exportJson(const Fts& fts, std::string& out, bool pretty = false);
ArxReturnCode importJson(std::string_view json, Fts& out);
ArxReturnCode exportJson(const Dlf& dlf, std::string& out, bool pretty = false, std::string_view signer = {});
ArxReturnCode importJson(std::string_view json, Dlf& out);
ArxReturnCode exportJson(const Llf& llf, std::string& out, bool pretty = false, std::string_view signer = {});
ArxReturnCode importJson(std::string_view json, Llf& out);
ArxReturnCode exportJson(const Tea& tea, std::string& out, bool pretty = false);
ArxReturnCode importJson(std::string_view json, Tea& out);

// --- Utilities ---

ArxReturnCode validate(const Dlf& dlf);
ArxReturnCode validate(const Ftl& ftl);
ArxReturnCode validate(const Fts& fts);
ArxReturnCode validate(const Llf& llf);
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
