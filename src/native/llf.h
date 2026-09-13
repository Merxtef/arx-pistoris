// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/llf.hpp"

#include "utils/cursor.h"

#include <string_view>

namespace pistoris {

ArxReturnCode loadLlf(llf::Data* data, ReadCursor& cursor);
ArxReturnCode saveLlf(const llf::Data* data, std::string_view signer, WriteCursor& cursor);
ArxReturnCode validateLlf(const llf::Data* data);

}  // namespace pistoris
