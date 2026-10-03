// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/location.hpp"

#include "utils/cursor.h"

namespace pistoris {

AmbBinaryResult<amb::Data> loadAmb(ReadCursor& cursor, NativeBinaryRegion region = NativeBinaryRegion::kStored);
ArxReturnCode saveAmb(const amb::Data* data, WriteCursor& cursor);
ArxReturnCode canonicalizeAmb(amb::Data* data, AmbLocation* failure_location = nullptr);
ArxReturnCode validateAmb(const amb::Data* data, AmbLocation* failure_location = nullptr);

}  // namespace pistoris
