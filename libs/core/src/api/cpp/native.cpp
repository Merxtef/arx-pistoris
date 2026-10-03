// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/native.hpp"

#include "arx_pistoris/base/status.h"

#include "api/result_failure.h"
#include "api/status_boundary.h"
#include "native/amb.h"
#include "native/cin.h"
#include "native/dlf.h"
#include "native/ftl.h"
#include "native/fts.h"
#include "native/llf.h"
#include "native/storage.h"
#include "native/tea.h"
#include "utils/cursor.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {

AmbBinaryResult<Amb> readAmb(std::span<const std::uint8_t> data) noexcept {
  return api_detail::resultBoundary(
      [&]() -> AmbBinaryResult<Amb> {
        ReadCursor cursor(data.data(), data.size());
        auto result = loadAmb(cursor);
        return result;
      },
      [&](ArxReturnCode code, std::string_view detail) {
        return api_detail::ambBinaryFailure<Amb>(code, std::nullopt, detail);
      });
}

AmbResult<std::vector<std::uint8_t>> writeAmb(const Amb& amb) noexcept {
  constexpr std::string_view kOperation = "AMB binary write";
  return api_detail::resultBoundary(
      [&]() -> AmbResult<std::vector<std::uint8_t>> {
        WriteCursor cursor;
        const ArxReturnCode code = saveAmb(&amb, cursor);
        if (code != ARX_OK) {
          AmbLocation location;
          const ArxReturnCode validation = validateAmb(&amb, &location);
          return validation == code ? api_detail::ambFailure<std::vector<std::uint8_t>>(code, location, {}, kOperation)
                                    : api_detail::ambFailure<std::vector<std::uint8_t>>(
                                          code, std::nullopt, std::string_view{}, kOperation);
        }
        return AmbResult<std::vector<std::uint8_t>>::success(cursor.take());
      },
      [&](ArxReturnCode code, std::string_view detail) {
        return api_detail::ambFailure<std::vector<std::uint8_t>>(code, std::nullopt, detail, kOperation);
      });
}

CinBinaryResult<Cin> readCin(std::span<const std::uint8_t> data) noexcept {
  return api_detail::resultBoundary(
      [&]() -> CinBinaryResult<Cin> {
        ReadCursor cursor(data.data(), data.size());
        auto result = loadCin(cursor);
        return result;
      },
      [&](ArxReturnCode code, std::string_view detail) {
        return api_detail::cinBinaryFailure<Cin>(code, std::nullopt, detail);
      });
}

CinResult<std::vector<std::uint8_t>> writeCin(const Cin& cin) noexcept {
  constexpr std::string_view kOperation = "CIN binary write";
  return api_detail::resultBoundary(
      [&]() -> CinResult<std::vector<std::uint8_t>> {
        WriteCursor cursor;
        const ArxReturnCode code = saveCin(&cin, cursor);
        if (code != ARX_OK) {
          CinLocation location;
          const ArxReturnCode validation = validateCin(&cin, &location);
          return validation == code ? api_detail::cinFailure<std::vector<std::uint8_t>>(code, location, {}, kOperation)
                                    : api_detail::cinFailure<std::vector<std::uint8_t>>(
                                          code, std::nullopt, std::string_view{}, kOperation);
        }
        return CinResult<std::vector<std::uint8_t>>::success(cursor.take());
      },
      [&](ArxReturnCode code, std::string_view detail) {
        return api_detail::cinFailure<std::vector<std::uint8_t>>(code, std::nullopt, detail, kOperation);
      });
}

DlfBinaryResult<DlfBundle> readDlf(std::span<const std::uint8_t> data) noexcept {
  return api_detail::resultBoundary(
      [&]() -> DlfBinaryResult<DlfBundle> {
        auto loaded = loadDlfStorage(data, true);
        if (!loaded) return std::move(loaded).propagate<DlfBundle>();
        DlfBundle result;
        result.dlf = std::move(loaded->data);
        result.embedded_lighting = std::move(loaded->embedded_lighting);
        return DlfBinaryResult<DlfBundle>::success(std::move(result));
      },
      [&](ArxReturnCode code, std::string_view detail) {
        return api_detail::dlfBinaryFailure<DlfBundle>(code, std::nullopt, detail);
      });
}

DlfWriteResult<std::vector<std::uint8_t>> writeDlf(const Dlf& dlf, const DlfWriteOptions& options,
                                                   bool compress) noexcept {
  constexpr std::string_view kOperation = "DLF binary write";
  return api_detail::resultBoundary(
      [&]() -> DlfWriteResult<std::vector<std::uint8_t>> {
        std::vector<std::uint8_t> result;
        const ArxReturnCode code = saveDlfStorage(&dlf, options.embedded_lighting, options.signer, result, compress);
        if (code != ARX_OK) {
          DlfLocation dlf_location;
          if (validateDlf(&dlf, &dlf_location) == code)
            return api_detail::dlfWriteFailure<std::vector<std::uint8_t>>(
                code, DlfWriteLocation{dlf_location}, {}, kOperation);
          LlfLocation llf_location;
          if (options.embedded_lighting && validateLlf(options.embedded_lighting, &llf_location) == code)
            return api_detail::dlfWriteFailure<std::vector<std::uint8_t>>(
                code, DlfWriteLocation{llf_location}, {}, kOperation);
          return api_detail::dlfWriteFailure<std::vector<std::uint8_t>>(
              code, std::nullopt, std::string_view{}, kOperation);
        }
        return DlfWriteResult<std::vector<std::uint8_t>>::success(std::move(result));
      },
      [&](ArxReturnCode code, std::string_view detail) {
        return api_detail::dlfWriteFailure<std::vector<std::uint8_t>>(code, std::nullopt, detail, kOperation);
      });
}

FtlBinaryResult<Ftl> readFtl(std::span<const std::uint8_t> data) noexcept {
  return api_detail::resultBoundary(
      [&]() -> FtlBinaryResult<Ftl> {
        auto result = loadFtlStorage(data);
        return result;
      },
      [&](ArxReturnCode code, std::string_view detail) {
        return api_detail::ftlBinaryFailure<Ftl>(code, std::nullopt, detail);
      });
}

FtlResult<std::vector<std::uint8_t>> writeFtl(const Ftl& ftl, bool compress) noexcept {
  constexpr std::string_view kOperation = "FTL binary write";
  return api_detail::resultBoundary(
      [&]() -> FtlResult<std::vector<std::uint8_t>> {
        std::vector<std::uint8_t> result;
        const ArxReturnCode code = saveFtlStorage(&ftl, result, compress);
        if (code != ARX_OK) {
          FtlLocation location;
          return validateFtl(&ftl, &location) == code
                     ? api_detail::ftlFailure<std::vector<std::uint8_t>>(code, location, {}, kOperation)
                     : api_detail::ftlFailure<std::vector<std::uint8_t>>(
                           code, std::nullopt, std::string_view{}, kOperation);
        }
        return FtlResult<std::vector<std::uint8_t>>::success(std::move(result));
      },
      [&](ArxReturnCode code, std::string_view detail) {
        return api_detail::ftlFailure<std::vector<std::uint8_t>>(code, std::nullopt, detail, kOperation);
      });
}

FtsBinaryResult<Fts> readFts(std::span<const std::uint8_t> data) noexcept {
  return api_detail::resultBoundary(
      [&]() -> FtsBinaryResult<Fts> {
        auto result = loadFtsStorage(data);
        return result;
      },
      [&](ArxReturnCode code, std::string_view detail) {
        return api_detail::ftsBinaryFailure<Fts>(code, std::nullopt, detail);
      });
}

FtsResult<std::vector<std::uint8_t>> writeFts(const Fts& fts, bool compress) noexcept {
  constexpr std::string_view kOperation = "FTS binary write";
  return api_detail::resultBoundary(
      [&]() -> FtsResult<std::vector<std::uint8_t>> {
        std::vector<std::uint8_t> result;
        const ArxReturnCode code = saveFtsStorage(&fts, result, compress);
        if (code != ARX_OK) {
          FtsLocation location;
          return validateFts(&fts, &location) == code
                     ? api_detail::ftsFailure<std::vector<std::uint8_t>>(code, location, {}, kOperation)
                     : api_detail::ftsFailure<std::vector<std::uint8_t>>(
                           code, std::nullopt, std::string_view{}, kOperation);
        }
        return FtsResult<std::vector<std::uint8_t>>::success(std::move(result));
      },
      [&](ArxReturnCode code, std::string_view detail) {
        return api_detail::ftsFailure<std::vector<std::uint8_t>>(code, std::nullopt, detail, kOperation);
      });
}

LlfBinaryResult<Llf> readLlf(std::span<const std::uint8_t> data) noexcept {
  return api_detail::resultBoundary(
      [&]() -> LlfBinaryResult<Llf> {
        auto result = loadLlfStorage(data);
        return result;
      },
      [&](ArxReturnCode code, std::string_view detail) {
        return api_detail::llfBinaryFailure<Llf>(code, std::nullopt, detail);
      });
}

LlfResult<std::vector<std::uint8_t>> writeLlf(const Llf& llf, bool compress) noexcept {
  return writeLlf(llf, LlfWriteOptions{}, compress);
}

LlfResult<std::vector<std::uint8_t>> writeLlf(const Llf& llf, const LlfWriteOptions& options, bool compress) noexcept {
  constexpr std::string_view kOperation = "LLF binary write";
  return api_detail::resultBoundary(
      [&]() -> LlfResult<std::vector<std::uint8_t>> {
        std::vector<std::uint8_t> result;
        const ArxReturnCode code = saveLlfStorage(&llf, options.signer, result, compress);
        if (code != ARX_OK) {
          LlfLocation location;
          return validateLlf(&llf, &location) == code
                     ? api_detail::llfFailure<std::vector<std::uint8_t>>(code, location, {}, kOperation)
                     : api_detail::llfFailure<std::vector<std::uint8_t>>(
                           code, std::nullopt, std::string_view{}, kOperation);
        }
        return LlfResult<std::vector<std::uint8_t>>::success(std::move(result));
      },
      [&](ArxReturnCode code, std::string_view detail) {
        return api_detail::llfFailure<std::vector<std::uint8_t>>(code, std::nullopt, detail, kOperation);
      });
}

TeaBinaryResult<Tea> readTea(std::span<const std::uint8_t> data) noexcept {
  return api_detail::resultBoundary(
      [&]() -> TeaBinaryResult<Tea> {
        ReadCursor cursor(data.data(), data.size());
        auto result = loadTea(cursor);
        return result;
      },
      [&](ArxReturnCode code, std::string_view detail) {
        return api_detail::teaBinaryFailure<Tea>(code, std::nullopt, detail);
      });
}

TeaResult<std::vector<std::uint8_t>> writeTea(const Tea& tea) noexcept {
  constexpr std::string_view kOperation = "TEA binary write";
  return api_detail::resultBoundary(
      [&]() -> TeaResult<std::vector<std::uint8_t>> {
        WriteCursor cursor;
        const ArxReturnCode code = saveTea(&tea, cursor);
        if (code != ARX_OK) {
          TeaLocation location;
          return validateTea(&tea, &location) == code
                     ? api_detail::teaFailure<std::vector<std::uint8_t>>(code, location, {}, kOperation)
                     : api_detail::teaFailure<std::vector<std::uint8_t>>(
                           code, std::nullopt, std::string_view{}, kOperation);
        }
        return TeaResult<std::vector<std::uint8_t>>::success(cursor.take());
      },
      [&](ArxReturnCode code, std::string_view detail) {
        return api_detail::teaFailure<std::vector<std::uint8_t>>(code, std::nullopt, detail, kOperation);
      });
}

AmbResult<void> validate(const Amb& amb) noexcept {
  AmbLocation location;
  return api_detail::validationBoundary<AmbResult<void>>([&] { return validateAmb(&amb, &location); }, location);
}

CinResult<void> validate(const Cin& cin) noexcept {
  CinLocation location;
  return api_detail::validationBoundary<CinResult<void>>([&] { return validateCin(&cin, &location); }, location);
}

DlfResult<void> validate(const Dlf& dlf) noexcept {
  DlfLocation location;
  return api_detail::validationBoundary<DlfResult<void>>([&] { return validateDlf(&dlf, &location); }, location);
}

FtlResult<void> validate(const Ftl& ftl) noexcept {
  FtlLocation location;
  return api_detail::validationBoundary<FtlResult<void>>([&] { return validateFtl(&ftl, &location); }, location);
}

FtsResult<void> validate(const Fts& fts) noexcept {
  FtsLocation location;
  return api_detail::validationBoundary<FtsResult<void>>([&] { return validateFts(&fts, &location); }, location);
}

LlfResult<void> validate(const Llf& llf) noexcept {
  LlfLocation location;
  return api_detail::validationBoundary<LlfResult<void>>([&] { return validateLlf(&llf, &location); }, location);
}

TeaResult<void> validate(const Tea& tea) noexcept {
  TeaLocation location;
  return api_detail::validationBoundary<TeaResult<void>>([&] { return validateTea(&tea, &location); }, location);
}

}  // namespace pistoris
