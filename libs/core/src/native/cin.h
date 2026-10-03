// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/location.hpp"

#include "utils/cursor.h"

namespace pistoris {

CinBinaryResult<cin::Data> loadCin(ReadCursor& cursor, NativeBinaryRegion region = NativeBinaryRegion::kStored);
ArxReturnCode saveCin(const cin::Data* data, WriteCursor& cursor);
ArxReturnCode validateCin(const cin::Data* data, CinLocation* failure_location = nullptr);

}  // namespace pistoris
