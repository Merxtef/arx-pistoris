// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"

#include <string>
#include <string_view>

namespace pistoris {

ArxReturnCode exportFtlToObj(const ftl::Data& d, std::string_view obj_stem, std::string& out);

// empty out if no texture containers
ArxReturnCode exportFtlToMtl(const ftl::Data& d, std::string& out);

// out must be freshly initialized
ArxReturnCode importObjToFtl(std::string_view obj, std::string_view mtl, std::string_view obj_filename, ftl::Data* out);

}  // namespace pistoris
