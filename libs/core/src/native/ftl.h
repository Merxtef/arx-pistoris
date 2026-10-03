// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/location.hpp"

#include "utils/cursor.h"

namespace pistoris {

FtlBinaryResult<ftl::Data> loadFtl(ReadCursor& cursor, NativeBinaryRegion region = NativeBinaryRegion::kStored);
ArxReturnCode saveFtl(const ftl::Data* d, WriteCursor& c);

ArxReturnCode canonicalizeFtl(ftl::Data* d, FtlLocation* failure_location = nullptr);
ArxReturnCode validateFtl(const ftl::Data* d, FtlLocation* failure_location = nullptr);

}  // namespace pistoris
