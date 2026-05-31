// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/pistoris_types.h"
#include "arx_pistoris/tea_data.hpp"

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris {

ArxReturnCode exportFtlTeaToGlb(const ftl::Data& ftl, std::span<const tea::Data> teas, std::vector<uint8_t>& out,
                                std::span<const std::pair<std::string, std::string>> extras = {});

ArxReturnCode exportFtlTeaToGlb(const ftl::Data& ftl, std::initializer_list<const tea::Data*> teas,
                                std::vector<uint8_t>& out,
                                std::span<const std::pair<std::string, std::string>> extras = {});

ArxReturnCode exportFtlTeaToGlb(const ftl::Data& ftl, std::span<const tea::Data* const> teas, std::vector<uint8_t>& out,
                                std::span<const std::pair<std::string, std::string>> extras = {});

ArxReturnCode importGlbToFtlTea(std::span<const uint8_t> glb, std::string_view glb_filename, ftl::Data& out_ftl,
                                std::vector<tea::Data>& out_teas,
                                std::vector<std::pair<std::string, std::string>>* out_extras = nullptr);

}  // namespace pistoris
