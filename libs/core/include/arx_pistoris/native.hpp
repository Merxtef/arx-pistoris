// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/json/location.hpp"
#include "arx_pistoris/native/amb.hpp"       // IWYU pragma: export
#include "arx_pistoris/native/cin.hpp"       // IWYU pragma: export
#include "arx_pistoris/native/dlf.hpp"       // IWYU pragma: export
#include "arx_pistoris/native/ftl.hpp"       // IWYU pragma: export
#include "arx_pistoris/native/fts.hpp"       // IWYU pragma: export
#include "arx_pistoris/native/llf.hpp"       // IWYU pragma: export
#include "arx_pistoris/native/location.hpp"  // IWYU pragma: export
#include "arx_pistoris/native/tea.hpp"       // IWYU pragma: export
#include "arx_pistoris/native/text.hpp"      // IWYU pragma: export

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {

struct DlfWriteOptions {
  const Llf* embedded_lighting = nullptr;
  // Optional printable-ASCII arx-pistoris/<signer> suffix, truncated to field capacity
  std::string_view signer;
};

struct LlfWriteOptions {
  // Optional printable-ASCII arx-pistoris/<signer> suffix, truncated to field capacity
  std::string_view signer;
};

struct DlfBundle {
  Dlf dlf;
  std::optional<Llf> embedded_lighting;
};

struct FtsJsonImport {  // NOLINT(bugprone-exception-escape): MSVC debug STL misreports container moves
  Fts fts;
  std::uint32_t level = 0;
};

// --- Binary I/O ---

[[nodiscard]] AmbBinaryResult<Amb> readAmb(std::span<const std::uint8_t> data) noexcept;
[[nodiscard]] AmbResult<std::vector<std::uint8_t>> writeAmb(const Amb& amb) noexcept;
[[nodiscard]] CinBinaryResult<Cin> readCin(std::span<const std::uint8_t> data) noexcept;
[[nodiscard]] CinResult<std::vector<std::uint8_t>> writeCin(const Cin& cin) noexcept;
[[nodiscard]] DlfBinaryResult<DlfBundle> readDlf(std::span<const std::uint8_t> data) noexcept;
[[nodiscard]] DlfWriteResult<std::vector<std::uint8_t>> writeDlf(const Dlf& dlf, const DlfWriteOptions& options,
                                                                 bool compress = true) noexcept;

[[nodiscard]] FtlBinaryResult<Ftl> readFtl(std::span<const std::uint8_t> data) noexcept;
[[nodiscard]] FtlResult<std::vector<std::uint8_t>> writeFtl(const Ftl& ftl, bool compress = true) noexcept;

[[nodiscard]] FtsBinaryResult<Fts> readFts(std::span<const std::uint8_t> data) noexcept;
[[nodiscard]] FtsResult<std::vector<std::uint8_t>> writeFts(const Fts& fts, bool compress = true) noexcept;

[[nodiscard]] LlfBinaryResult<Llf> readLlf(std::span<const std::uint8_t> data) noexcept;
[[nodiscard]] LlfResult<std::vector<std::uint8_t>> writeLlf(const Llf& llf, bool compress = true) noexcept;
[[nodiscard]] LlfResult<std::vector<std::uint8_t>> writeLlf(const Llf& llf, const LlfWriteOptions& options,
                                                            bool compress = true) noexcept;

[[nodiscard]] TeaBinaryResult<Tea> readTea(std::span<const std::uint8_t> data) noexcept;
[[nodiscard]] TeaResult<std::vector<std::uint8_t>> writeTea(const Tea& tea) noexcept;

// --- JSON conversion ---

// JSON is UTF-8. text_mode controls decoding native carrier fields during JSON
// export and encoding JSON text into native carrier fields during import.

[[nodiscard]] AmbResult<std::string> toAmbJson(const Amb& amb, bool pretty = false,
                                               NativeTextMode text_mode = NativeTextMode::kAuto) noexcept;
[[nodiscard]] JsonResult<Amb> fromAmbJson(std::string_view json,
                                          NativeTextMode text_mode = NativeTextMode::kUtf8) noexcept;
[[nodiscard]] DlfResult<std::string> toDlfJson(const Dlf& dlf, bool pretty = false, std::string_view signer = {},
                                               NativeTextMode text_mode = NativeTextMode::kAuto) noexcept;
[[nodiscard]] JsonResult<Dlf> fromDlfJson(std::string_view json,
                                          NativeTextMode text_mode = NativeTextMode::kUtf8) noexcept;
[[nodiscard]] FtlResult<std::string> toFtlJson(const Ftl& ftl, bool pretty = false,
                                               NativeTextMode text_mode = NativeTextMode::kAuto) noexcept;
[[nodiscard]] JsonResult<Ftl> fromFtlJson(std::string_view json,
                                          NativeTextMode text_mode = NativeTextMode::kUtf8) noexcept;
[[nodiscard]] FtsResult<std::string> toFtsJson(const Fts& fts, std::uint32_t level, bool pretty = false,
                                               NativeTextMode text_mode = NativeTextMode::kAuto) noexcept;
[[nodiscard]] JsonResult<FtsJsonImport> fromFtsJson(std::string_view json,
                                                    NativeTextMode text_mode = NativeTextMode::kUtf8) noexcept;
[[nodiscard]] LlfResult<std::string> toLlfJson(const Llf& llf, bool pretty = false,
                                               std::string_view signer = {}) noexcept;
[[nodiscard]] JsonResult<Llf> fromLlfJson(std::string_view json) noexcept;
[[nodiscard]] TeaResult<std::string> toTeaJson(const Tea& tea, bool pretty = false,
                                               NativeTextMode text_mode = NativeTextMode::kAuto) noexcept;
[[nodiscard]] JsonResult<Tea> fromTeaJson(std::string_view json,
                                          NativeTextMode text_mode = NativeTextMode::kUtf8) noexcept;

// --- Validation ---

[[nodiscard]] AmbResult<void> validate(const Amb& amb) noexcept;
[[nodiscard]] CinResult<void> validate(const Cin& cin) noexcept;
[[nodiscard]] DlfResult<void> validate(const Dlf& dlf) noexcept;
[[nodiscard]] FtlResult<void> validate(const Ftl& ftl) noexcept;
[[nodiscard]] FtsResult<void> validate(const Fts& fts) noexcept;
[[nodiscard]] LlfResult<void> validate(const Llf& llf) noexcept;
[[nodiscard]] TeaResult<void> validate(const Tea& tea) noexcept;

}  // namespace pistoris
