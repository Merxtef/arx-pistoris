// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/text.hpp"

#include "formats/classification.h"

#include <cstdint>
#include <optional>
#include <string>

namespace cli::level {

ArxReturnCode decodeFts(const ClassifiedPath& input, pistoris::NativeTextMode text_mode, pistoris::Fts& out,
                        std::string* failure = nullptr);
ArxReturnCode decodeLlf(const ClassifiedPath& input, pistoris::Llf& out, std::string* failure = nullptr);
ArxReturnCode decodeDlf(const ClassifiedPath& input, pistoris::NativeTextMode text_mode, pistoris::Dlf& out,
                        std::optional<pistoris::Llf>* embedded_lighting = nullptr, std::string* failure = nullptr);
void applyLevelNumber(std::uint32_t level, pistoris::Dlf& dlf);

}  // namespace cli::level
