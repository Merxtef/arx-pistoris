// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/location.hpp"

#include "utils/cursor.h"

#include <cstddef>
#include <cstdint>

namespace pistoris {

namespace fts_detail {

constexpr std::size_t kMaxSourceChecks = 0x10000;
constexpr std::size_t kSourceCheckPathSize = 256;
constexpr std::size_t kSourceCheckDataSize = 512;
constexpr std::size_t kSourceCheckSize = kSourceCheckPathSize + kSourceCheckDataSize;

struct StorageHeader {
  char path[256] = {};
  std::int32_t count = 0;
  float version = kFtsVersion;
  std::int32_t uncompressed_size = 0;
  std::int32_t padding[3] = {};
};
static_assert(sizeof(StorageHeader) == 280);

}  // namespace fts_detail

FtsBinaryResult<fts::Data> loadFts(ReadCursor& cursor, NativeBinaryRegion region = NativeBinaryRegion::kStored);
FtsBinaryResult<fts::Data> loadFts(ReadCursor& prefix, ReadCursor& payload, NativeBinaryRegion prefix_region,
                                   NativeBinaryRegion payload_region);
ArxReturnCode saveFts(const fts::Data* d, WriteCursor& c);
ArxReturnCode canonicalizeFts(fts::Data* d, FtsLocation* failure_location = nullptr);
ArxReturnCode validateFts(const fts::Data* d, FtsLocation* failure_location = nullptr);

}  // namespace pistoris
