// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"

#include "utils/cursor.h"

namespace pistoris {

ArxReturnCode loadFtl(ftl::Data* h, ReadCursor& c);
ArxReturnCode saveFtl(const ftl::Data* d, WriteCursor& c);

// rebuilds d->extras
ArxReturnCode validateFtl(const ftl::Data* d);

}  // namespace pistoris
