// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/amb.hpp"  // IWYU pragma: export
#include "arx_pistoris/native/dlf.hpp"  // IWYU pragma: export
#include "arx_pistoris/native/ftl.hpp"  // IWYU pragma: export
#include "arx_pistoris/native/fts.hpp"  // IWYU pragma: export
#include "arx_pistoris/native/llf.hpp"  // IWYU pragma: export
#include "arx_pistoris/native/tea.hpp"  // IWYU pragma: export

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {

struct DlfWriteOptions {
  const Llf* embedded_lighting = nullptr;
  // Optional arx-pistoris/<signer> suffix, truncated to field capacity
  std::string_view signer;
};

struct LlfWriteOptions {
  // Optional arx-pistoris/<signer> suffix, truncated to field capacity
  std::string_view signer;
};

// --- Binary I/O ---

[[nodiscard]] ArxReturnCode readAmb(std::span<const std::uint8_t> data, Amb& out) noexcept;
[[nodiscard]] ArxReturnCode writeAmb(const Amb& amb, std::vector<std::uint8_t>& out) noexcept;
[[nodiscard]] ArxReturnCode readDlf(std::span<const std::uint8_t> data, Dlf& out,
                                    std::optional<Llf>* embedded_lighting = nullptr) noexcept;
[[nodiscard]] ArxReturnCode writeDlf(const Dlf& dlf, const DlfWriteOptions& options, std::vector<std::uint8_t>& out,
                                     bool compress = true) noexcept;

[[nodiscard]] ArxReturnCode readFtl(std::span<const std::uint8_t> data, Ftl& out) noexcept;
[[nodiscard]] ArxReturnCode writeFtl(const Ftl& ftl, std::vector<std::uint8_t>& out, bool compress = true) noexcept;

[[nodiscard]] ArxReturnCode readFts(std::span<const std::uint8_t> data, Fts& out) noexcept;
[[nodiscard]] ArxReturnCode writeFts(const Fts& fts, std::vector<std::uint8_t>& out, bool compress = true) noexcept;

[[nodiscard]] ArxReturnCode readLlf(std::span<const std::uint8_t> data, Llf& out) noexcept;
[[nodiscard]] ArxReturnCode writeLlf(const Llf& llf, std::vector<std::uint8_t>& out, bool compress = true) noexcept;
[[nodiscard]] ArxReturnCode writeLlf(const Llf& llf, const LlfWriteOptions& options, std::vector<std::uint8_t>& out,
                                     bool compress = true) noexcept;

[[nodiscard]] ArxReturnCode readTea(std::span<const std::uint8_t> data, Tea& out) noexcept;
[[nodiscard]] ArxReturnCode writeTea(const Tea& tea, std::vector<std::uint8_t>& out) noexcept;

// --- JSON conversion ---

[[nodiscard]] ArxReturnCode toJson(const Amb& amb, std::string& out, bool pretty = false) noexcept;
[[nodiscard]] ArxReturnCode fromJson(std::string_view json, Amb& out) noexcept;
[[nodiscard]] ArxReturnCode toJson(const Dlf& dlf, std::string& out, bool pretty = false,
                                   std::string_view signer = {}) noexcept;
[[nodiscard]] ArxReturnCode fromJson(std::string_view json, Dlf& out) noexcept;
[[nodiscard]] ArxReturnCode toJson(const Ftl& ftl, std::string& out, bool pretty = false) noexcept;
[[nodiscard]] ArxReturnCode fromJson(std::string_view json, Ftl& out) noexcept;
[[nodiscard]] ArxReturnCode toJson(const Fts& fts, std::string& out, bool pretty = false) noexcept;
[[nodiscard]] ArxReturnCode fromJson(std::string_view json, Fts& out) noexcept;
[[nodiscard]] ArxReturnCode toJson(const Llf& llf, std::string& out, bool pretty = false,
                                   std::string_view signer = {}) noexcept;
[[nodiscard]] ArxReturnCode fromJson(std::string_view json, Llf& out) noexcept;
[[nodiscard]] ArxReturnCode toJson(const Tea& tea, std::string& out, bool pretty = false) noexcept;
[[nodiscard]] ArxReturnCode fromJson(std::string_view json, Tea& out) noexcept;

// --- Validation ---

[[nodiscard]] ArxReturnCode validate(const Amb& amb) noexcept;
[[nodiscard]] ArxReturnCode validate(const Dlf& dlf) noexcept;
[[nodiscard]] ArxReturnCode validate(const Ftl& ftl) noexcept;
[[nodiscard]] ArxReturnCode validate(const Fts& fts) noexcept;
[[nodiscard]] ArxReturnCode validate(const Llf& llf) noexcept;
[[nodiscard]] ArxReturnCode validate(const Tea& tea) noexcept;

}  // namespace pistoris
