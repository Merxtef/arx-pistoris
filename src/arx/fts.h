// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "utils/cursor.h"

namespace pistoris {

ArxReturnCode loadFts(fts::Data* d, ReadCursor& c);
ArxReturnCode saveFts(const fts::Data* d, WriteCursor& c);
ArxReturnCode validateFts(const fts::Data* d);

}  // namespace pistoris
