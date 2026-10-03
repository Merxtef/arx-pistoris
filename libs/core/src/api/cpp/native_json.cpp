// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/json/location.hpp"
#include "arx_pistoris/native.hpp"

#include "api/result_failure.h"
#include "api/status_boundary.h"
#include "external/json.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pistoris {

AmbResult<std::string> toAmbJson(const Amb& amb, bool pretty, NativeTextMode text_mode) noexcept {
  constexpr std::string_view kOperation = "AMB -> JSON conversion";
  return api_detail::resultBoundary([&] { return exportAmbToJson(amb, pretty, text_mode); },
                                    [&](ArxReturnCode code, std::string_view detail) {
                                      return api_detail::ambFailure<std::string>(
                                          code, std::nullopt, detail, kOperation);
                                    });
}

JsonResult<Amb> fromAmbJson(std::string_view json, NativeTextMode text_mode) noexcept {
  constexpr std::string_view kOperation = "JSON -> AMB conversion";
  return api_detail::resultBoundary([&] { return importJsonToAmb(json, text_mode); },
                                    [&](ArxReturnCode code, std::string_view detail) {
                                      return api_detail::jsonFailure<Amb>(code, std::nullopt, detail, kOperation);
                                    });
}

DlfResult<std::string> toDlfJson(const Dlf& dlf, bool pretty, std::string_view signer,
                                 NativeTextMode text_mode) noexcept {
  constexpr std::string_view kOperation = "DLF -> JSON conversion";
  return api_detail::resultBoundary([&] { return exportDlfToJson(dlf, pretty, signer, text_mode); },
                                    [&](ArxReturnCode code, std::string_view detail) {
                                      return api_detail::dlfFailure<std::string>(
                                          code, std::nullopt, detail, kOperation);
                                    });
}

JsonResult<Dlf> fromDlfJson(std::string_view json, NativeTextMode text_mode) noexcept {
  constexpr std::string_view kOperation = "JSON -> DLF conversion";
  return api_detail::resultBoundary([&] { return importJsonToDlf(json, text_mode); },
                                    [&](ArxReturnCode code, std::string_view detail) {
                                      return api_detail::jsonFailure<Dlf>(code, std::nullopt, detail, kOperation);
                                    });
}

FtlResult<std::string> toFtlJson(const Ftl& ftl, bool pretty, NativeTextMode text_mode) noexcept {
  constexpr std::string_view kOperation = "FTL -> JSON conversion";
  return api_detail::resultBoundary([&] { return exportFtlToJson(ftl, pretty, text_mode); },
                                    [&](ArxReturnCode code, std::string_view detail) {
                                      return api_detail::ftlFailure<std::string>(
                                          code, std::nullopt, detail, kOperation);
                                    });
}

JsonResult<Ftl> fromFtlJson(std::string_view json, NativeTextMode text_mode) noexcept {
  constexpr std::string_view kOperation = "JSON -> FTL conversion";
  return api_detail::resultBoundary([&] { return importJsonToFtl(json, text_mode); },
                                    [&](ArxReturnCode code, std::string_view detail) {
                                      return api_detail::jsonFailure<Ftl>(code, std::nullopt, detail, kOperation);
                                    });
}

FtsResult<std::string> toFtsJson(const Fts& fts, std::uint32_t level, bool pretty, NativeTextMode text_mode) noexcept {
  constexpr std::string_view kOperation = "FTS -> JSON conversion";
  return api_detail::resultBoundary([&] { return exportFtsToJson(fts, level, pretty, text_mode); },
                                    [&](ArxReturnCode code, std::string_view detail) {
                                      return api_detail::ftsFailure<std::string>(
                                          code, std::nullopt, detail, kOperation);
                                    });
}

JsonResult<FtsJsonImport> fromFtsJson(std::string_view json, NativeTextMode text_mode) noexcept {
  constexpr std::string_view kOperation = "JSON -> FTS conversion";
  return api_detail::resultBoundary([&] { return importJsonToFts(json, text_mode); },
                                    [&](ArxReturnCode code, std::string_view detail) {
                                      return api_detail::jsonFailure<FtsJsonImport>(
                                          code, std::nullopt, detail, kOperation);
                                    });
}

LlfResult<std::string> toLlfJson(const Llf& llf, bool pretty, std::string_view signer) noexcept {
  constexpr std::string_view kOperation = "LLF -> JSON conversion";
  return api_detail::resultBoundary([&] { return exportLlfToJson(llf, pretty, signer); },
                                    [&](ArxReturnCode code, std::string_view detail) {
                                      return api_detail::llfFailure<std::string>(
                                          code, std::nullopt, detail, kOperation);
                                    });
}

JsonResult<Llf> fromLlfJson(std::string_view json) noexcept {
  constexpr std::string_view kOperation = "JSON -> LLF conversion";
  return api_detail::resultBoundary([&] { return importJsonToLlf(json); },
                                    [&](ArxReturnCode code, std::string_view detail) {
                                      return api_detail::jsonFailure<Llf>(code, std::nullopt, detail, kOperation);
                                    });
}

TeaResult<std::string> toTeaJson(const Tea& tea, bool pretty, NativeTextMode text_mode) noexcept {
  constexpr std::string_view kOperation = "TEA -> JSON conversion";
  return api_detail::resultBoundary([&] { return exportTeaToJson(tea, pretty, text_mode); },
                                    [&](ArxReturnCode code, std::string_view detail) {
                                      return api_detail::teaFailure<std::string>(
                                          code, std::nullopt, detail, kOperation);
                                    });
}

JsonResult<Tea> fromTeaJson(std::string_view json, NativeTextMode text_mode) noexcept {
  constexpr std::string_view kOperation = "JSON -> TEA conversion";
  return api_detail::resultBoundary([&] { return importJsonToTea(json, text_mode); },
                                    [&](ArxReturnCode code, std::string_view detail) {
                                      return api_detail::jsonFailure<Tea>(code, std::nullopt, detail, kOperation);
                                    });
}

}  // namespace pistoris
