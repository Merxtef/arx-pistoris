// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/llf.hpp"

#include "utils/cursor.h"

#include <optional>
#include <string_view>

namespace pistoris {

bool validDlfScenePath(std::string_view scene_path) noexcept;
ArxReturnCode loadDlf(dlf::Data* data, std::optional<llf::Data>* embedded_lighting, ReadCursor& cursor);
ArxReturnCode loadDlf(dlf::Data* data, std::optional<llf::Data>* embedded_lighting, ReadCursor& prefix,
                      ReadCursor& payload);
ArxReturnCode saveDlf(const dlf::Data* data, const llf::Data* embedded_lighting, std::string_view signer,
                      WriteCursor& cursor);
ArxReturnCode validateDlf(const dlf::Data* data);

}  // namespace pistoris
