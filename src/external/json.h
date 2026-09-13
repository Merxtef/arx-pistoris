// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/tea.hpp"

#include <string>
#include <string_view>

namespace pistoris {

ArxReturnCode exportAmbToJson(const amb::Data& data, bool pretty, std::string& out);
ArxReturnCode importJsonToAmb(std::string_view text, amb::Data* out);
ArxReturnCode exportFtlToJson(const ftl::Data& d, bool pretty, std::string& out);
ArxReturnCode importJsonToFtl(std::string_view text, ftl::Data* out);
ArxReturnCode exportFtsToJson(const fts::Data& data, bool pretty, std::string& out);
ArxReturnCode importJsonToFts(std::string_view text, fts::Data* out);
ArxReturnCode exportDlfToJson(const dlf::Data& data, bool pretty, std::string_view signer, std::string& out);
ArxReturnCode importJsonToDlf(std::string_view text, dlf::Data* out);
ArxReturnCode exportLlfToJson(const llf::Data& data, bool pretty, std::string_view signer, std::string& out);
ArxReturnCode importJsonToLlf(std::string_view text, llf::Data* out);
ArxReturnCode exportTeaToJson(const tea::Data& d, bool pretty, std::string& out);
ArxReturnCode importJsonToTea(std::string_view text, tea::Data* out);

}  // namespace pistoris
