// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/cin.hpp"

#include "utils/cursor.h"

namespace pistoris {

ArxReturnCode loadCin(cin::Data* data, ReadCursor& cursor);
ArxReturnCode saveCin(const cin::Data* data, WriteCursor& cursor);
ArxReturnCode validateCin(const cin::Data* data);

}  // namespace pistoris
