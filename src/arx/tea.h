// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once
#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

#include "utils/cursor.h"

namespace pistoris {

ArxReturnCode loadTea(tea::Data* d, ReadCursor& c);
ArxReturnCode saveTea(const tea::Data* d, WriteCursor& c);

ArxReturnCode validateTea(const tea::Data* d);

}  // namespace pistoris
