// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/location.hpp"

#include "utils/cursor.h"

#include <optional>
#include <string_view>

namespace pistoris {

struct DlfLoad {
  dlf::Data data;
  std::optional<llf::Data> embedded_lighting;
};

bool validDlfScenePath(std::string_view scene_path);
DlfBinaryResult<DlfLoad> loadDlf(ReadCursor& cursor, bool read_embedded_lighting,
                                 NativeBinaryRegion region = NativeBinaryRegion::kStored);
DlfBinaryResult<DlfLoad> loadDlf(ReadCursor& prefix, ReadCursor& payload, bool read_embedded_lighting,
                                 NativeBinaryRegion prefix_region, NativeBinaryRegion payload_region);
ArxReturnCode saveDlf(const dlf::Data* data, const llf::Data* embedded_lighting, std::string_view signer,
                      WriteCursor& cursor);
ArxReturnCode canonicalizeDlf(dlf::Data* data, DlfLocation* failure_location = nullptr);
ArxReturnCode validateDlf(const dlf::Data* data, DlfLocation* failure_location = nullptr);

}  // namespace pistoris
