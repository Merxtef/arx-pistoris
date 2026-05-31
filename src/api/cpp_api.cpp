// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/pistoris.hpp"

#include "api/api_helpers.h"
#include "api/strerror.h"
#include "arx/ftl.h"
#include "arx/tea.h"
#include "external/glb.h"
#include "external/json.h"
#include "external/obj.h"
#include "utils/cursor.h"
#include "utils/log.h"
#include "utils/math/xform.h"
#include "version.h"

#include <exception>
#include <new>
#include <string>
#include <utility>

namespace pistoris {

ArxLogFn log_fn = nullptr;
void* log_ud    = nullptr;

const char* buildTime();

namespace {

template <class Fn>
ArxReturnCode guard(const char* where, Fn&& fn) {
  try {
    return std::forward<Fn>(fn)();
  } catch (const std::bad_alloc&) {
    log(ARX_LOG_ERROR, std::string(where) + ": allocation failed");
    return ARX_BAD_ALLOC;
  } catch (const std::exception& e) {
    log(ARX_LOG_ERROR, std::string(where) + ": unexpected exception: " + e.what());
    return ARX_UNKNOWN_ERROR;
  } catch (...) {
    log(ARX_LOG_ERROR, std::string(where) + ": unexpected exception");
    return ARX_UNKNOWN_ERROR;
  }
}

}  // namespace

const char* version() { return kVersion; }

const char* buildTimeString() { return buildTime(); }

const char* errorString(ArxReturnCode rc) { return arx_pistoris_strerror(rc); }

void setLogCallback(ArxLogFn fn, void* userdata) {
  log_fn = fn;
  log_ud = userdata;
}

ArxReturnCode readFtl(std::span<const std::uint8_t> data, Ftl& out) {
  return guard("readFtl", [&] {
    Ftl tmp;
    ReadCursor c(data.data(), data.size());
    ArxReturnCode rc = loadFtl(&tmp, c);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode writeFtl(const Ftl& ftl, std::vector<std::uint8_t>& out) {
  return guard("writeFtl", [&] {
    WriteCursor c;
    ArxReturnCode rc = saveFtl(&ftl, c);
    if (rc == ARX_OK) out = c.take();
    return rc;
  });
}

ArxReturnCode readTea(std::span<const std::uint8_t> data, Tea& out) {
  return guard("readTea", [&] {
    Tea tmp;
    ReadCursor c(data.data(), data.size());
    ArxReturnCode rc = loadTea(&tmp, c);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode writeTea(const Tea& tea, std::vector<std::uint8_t>& out) {
  return guard("writeTea", [&] {
    WriteCursor c;
    ArxReturnCode rc = saveTea(&tea, c);
    if (rc == ARX_OK) out = c.take();
    return rc;
  });
}

ArxReturnCode exportObj(const Ftl& ftl, std::string_view stem, Obj& out) {
  return guard("exportObj", [&] {
    Obj tmp;
    ArxReturnCode rc = exportFtlToObj(ftl, stem, tmp.text);
    if (rc != ARX_OK) return rc;
    rc = exportFtlToMtl(ftl, tmp.mtl);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode importObj(std::string_view obj, std::string_view mtl, std::string_view filename, Ftl& out) {
  return guard("importObj", [&] {
    Ftl tmp;
    ArxReturnCode rc = importObjToFtl(obj, mtl, filename, &tmp);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode importObj(const Obj& obj, std::string_view filename, Ftl& out) {
  return importObj(obj.text, obj.mtl, filename, out);
}

ArxReturnCode exportGlb(const Ftl& ftl, std::span<const Tea> teas, std::vector<std::uint8_t>& out) {
  return guard("exportGlb", [&] {
    std::vector<std::uint8_t> tmp;
    ArxReturnCode rc = exportFtlTeaToGlb(ftl, teas, tmp);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode importGlb(std::span<const std::uint8_t> data, std::string_view filename, Ftl& out_ftl,
                        std::vector<Tea>& out_teas) {
  return guard("importGlb", [&] {
    Ftl tmp_ftl;
    std::vector<Tea> tmp_teas;
    ArxReturnCode rc = importGlbToFtlTea(data, filename, tmp_ftl, tmp_teas);
    if (rc == ARX_OK) {
      out_ftl  = std::move(tmp_ftl);
      out_teas = std::move(tmp_teas);
    }
    return rc;
  });
}

ArxReturnCode exportJson(const Ftl& ftl, std::string& out, bool pretty) {
  return guard("exportJson", [&] {
    std::string tmp;
    ArxReturnCode rc = exportFtlToJson(ftl, pretty, tmp);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode importJson(std::string_view json, Ftl& out) {
  return guard("importJson", [&] {
    Ftl tmp;
    ArxReturnCode rc = importJsonToFtl(json, &tmp);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode exportJson(const Tea& tea, std::string& out, bool pretty) {
  return guard("exportJson", [&] {
    std::string tmp;
    ArxReturnCode rc = exportTeaToJson(tea, pretty, tmp);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode importJson(std::string_view json, Tea& out) {
  return guard("importJson", [&] {
    Tea tmp;
    ArxReturnCode rc = importJsonToTea(json, &tmp);
    if (rc == ARX_OK) out = std::move(tmp);
    return rc;
  });
}

ArxReturnCode validate(const Ftl& ftl) {
  return guard("validateFtl", [&] { return validateFtl(&ftl); });
}

ArxReturnCode validate(const Tea& tea) {
  return guard("validateTea", [&] { return validateTea(&tea); });
}

ArxReturnCode applyTransform(Ftl& ftl, const AffineXform& xform) {
  return guard("applyTransform", [&] { return api::applyTransform(ftl, xform); });
}

ArxReturnCode applyTransform(Tea& tea, const AffineXform& xform) {
  return guard("applyTransform", [&] { return api::applyTransform(tea, xform); });
}

ArxReturnCode overwriteTexturePaths(Ftl& ftl, std::string_view path) {
  return guard("overwriteTexturePaths", [&] { return api::overwriteTexturePaths(ftl, path, "overwriteTexturePaths"); });
}

ArxReturnCode snapFtlBoneOriginsToReference(Ftl& target, const Ftl& reference) {
  return guard("snapFtlBoneOriginsToReference", [&] { return api::snapFtlBoneOriginsToReference(target, reference); });
}

ArxReturnCode snapFtlActionPointsToReference(Ftl& target, const Ftl& reference) {
  return guard("snapFtlActionPointsToReference",
               [&] { return api::snapFtlActionPointsToReference(target, reference); });
}

ArxReturnCode copyFtlSyntheticSelectionAffiliations(Ftl& target, const Ftl& reference) {
  return guard("copyFtlSyntheticSelectionAffiliations",
               [&] { return api::copyFtlSyntheticSelectionAffiliations(target, reference); });
}

}  // namespace pistoris
