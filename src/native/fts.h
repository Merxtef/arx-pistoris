// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/fts.hpp"

#include "utils/cursor.h"

namespace pistoris {

ArxReturnCode loadFts(fts::Data* d, ReadCursor& c);
ArxReturnCode loadFts(fts::Data* d, ReadCursor& prefix, ReadCursor& payload);
ArxReturnCode saveFts(const fts::Data* d, WriteCursor& c);
ArxReturnCode canonicalizeFts(fts::Data* d);
ArxReturnCode validateFts(const fts::Data* d);

}  // namespace pistoris
