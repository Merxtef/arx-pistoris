// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/text.hpp"

#include <string>
#include <string_view>

namespace cli::io_detail {

ArxReturnCode nativeTextToUtf8(std::string_view raw, pistoris::NativeTextMode mode, std::string& out);

}  // namespace cli::io_detail
