// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace pistoris {

ArxReturnCode loadFtlStorage(ftl::Data* data, std::span<const std::uint8_t> stored);
ArxReturnCode saveFtlStorage(const ftl::Data* data, std::vector<std::uint8_t>& stored, bool compress);

ArxReturnCode loadFtsStorage(fts::Data* data, std::span<const std::uint8_t> stored);
ArxReturnCode saveFtsStorage(const fts::Data* data, std::vector<std::uint8_t>& stored, bool compress);

ArxReturnCode loadLlfStorage(llf::Data* data, std::span<const std::uint8_t> stored);
ArxReturnCode saveLlfStorage(const llf::Data* data, std::string_view signer, std::vector<std::uint8_t>& stored,
                             bool compress);

ArxReturnCode loadDlfStorage(dlf::Data* data, std::optional<llf::Data>* embedded_lighting,
                             std::span<const std::uint8_t> stored);
ArxReturnCode saveDlfStorage(const dlf::Data* data, const llf::Data* embedded_lighting, std::string_view signer,
                             std::vector<std::uint8_t>& stored, bool compress);

}  // namespace pistoris
