// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"

#include "formats/classification.h"

#include <cstdint>
#include <optional>

namespace cli::level {

ArxReturnCode decodeFts(const ClassifiedPath& input, pistoris::Fts& out);
ArxReturnCode decodeLlf(const ClassifiedPath& input, pistoris::Llf& out);
ArxReturnCode decodeDlf(const ClassifiedPath& input, pistoris::Dlf& out,
                        std::optional<pistoris::Llf>* embedded_lighting = nullptr);
void applyLevelNumber(std::uint32_t level, pistoris::Fts& fts, pistoris::Dlf* dlf);
void applyLevelNumber(std::uint32_t level, pistoris::Dlf& dlf);

}  // namespace cli::level
