// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/location.hpp"

#include "utils/cursor.h"

#include <string_view>

namespace pistoris {

LlfBinaryResult<llf::Data> loadLlf(ReadCursor& cursor, NativeBinaryRegion region = NativeBinaryRegion::kStored);
ArxReturnCode saveLlf(const llf::Data* data, std::string_view signer, WriteCursor& cursor);
ArxReturnCode validateLlf(const llf::Data* data, LlfLocation* failure_location = nullptr);

}  // namespace pistoris
