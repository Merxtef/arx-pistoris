// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/location.hpp"

#include "native/dlf.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace pistoris {

FtlBinaryResult<ftl::Data> loadFtlStorage(std::span<const std::uint8_t> stored);
ArxReturnCode saveFtlStorage(const ftl::Data* data, std::vector<std::uint8_t>& stored, bool compress);

FtsBinaryResult<fts::Data> loadFtsStorage(std::span<const std::uint8_t> stored);
ArxReturnCode saveFtsStorage(const fts::Data* data, std::vector<std::uint8_t>& stored, bool compress);

LlfBinaryResult<llf::Data> loadLlfStorage(std::span<const std::uint8_t> stored);
ArxReturnCode saveLlfStorage(const llf::Data* data, std::string_view signer, std::vector<std::uint8_t>& stored,
                             bool compress);

DlfBinaryResult<DlfLoad> loadDlfStorage(std::span<const std::uint8_t> stored, bool read_embedded_lighting);
ArxReturnCode saveDlfStorage(const dlf::Data* data, const llf::Data* embedded_lighting, std::string_view signer,
                             std::vector<std::uint8_t>& stored, bool compress);

}  // namespace pistoris
