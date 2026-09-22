// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/ftl.hpp"

#include "utils/cursor.h"

namespace pistoris {

ArxReturnCode loadFtl(ftl::Data* h, ReadCursor& c);
ArxReturnCode saveFtl(const ftl::Data* d, WriteCursor& c);

ArxReturnCode canonicalizeFtl(ftl::Data* d);
ArxReturnCode validateFtl(const ftl::Data* d);

}  // namespace pistoris
