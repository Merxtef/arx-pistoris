// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

#include <string>
#include <string_view>

namespace pistoris {

ArxReturnCode exportFtlToJson(const ftl::Data& d, bool pretty, std::string& out);
ArxReturnCode importJsonToFtl(std::string_view text, ftl::Data* out);
ArxReturnCode exportTeaToJson(const tea::Data& d, bool pretty, std::string& out);
ArxReturnCode importJsonToTea(std::string_view text, tea::Data* out);

}  // namespace pistoris
