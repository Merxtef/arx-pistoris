// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/native/tea.hpp"

#include "utils/cursor.h"

namespace pistoris {

TeaBinaryResult<tea::Data> loadTea(ReadCursor& cursor, NativeBinaryRegion region = NativeBinaryRegion::kStored);
ArxReturnCode saveTea(const tea::Data* d, WriteCursor& c);

ArxReturnCode canonicalizeTea(tea::Data* d, TeaLocation* failure_location = nullptr);
ArxReturnCode validateTea(const tea::Data* d, TeaLocation* failure_location = nullptr);

}  // namespace pistoris
