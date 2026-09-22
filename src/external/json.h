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
#include "arx_pistoris/native/text.hpp"

#include <string>
#include <string_view>

namespace pistoris {

ArxReturnCode exportAmbToJson(const amb::Data& data, bool pretty, NativeTextMode text_mode, std::string& out);
ArxReturnCode importJsonToAmb(std::string_view text, NativeTextMode text_mode, amb::Data* out);
ArxReturnCode exportFtlToJson(const ftl::Data& d, bool pretty, NativeTextMode text_mode, std::string& out);
ArxReturnCode importJsonToFtl(std::string_view text, NativeTextMode text_mode, ftl::Data* out);
ArxReturnCode exportFtsToJson(const fts::Data& data, bool pretty, NativeTextMode text_mode, std::string& out);
ArxReturnCode importJsonToFts(std::string_view text, NativeTextMode text_mode, fts::Data* out);
ArxReturnCode exportDlfToJson(const dlf::Data& data, bool pretty, std::string_view signer, NativeTextMode text_mode,
                              std::string& out);
ArxReturnCode importJsonToDlf(std::string_view text, NativeTextMode text_mode, dlf::Data* out);
ArxReturnCode exportLlfToJson(const llf::Data& data, bool pretty, std::string_view signer, std::string& out);
ArxReturnCode importJsonToLlf(std::string_view text, llf::Data* out);
ArxReturnCode exportTeaToJson(const tea::Data& d, bool pretty, NativeTextMode text_mode, std::string& out);
ArxReturnCode importJsonToTea(std::string_view text, NativeTextMode text_mode, tea::Data* out);

}  // namespace pistoris
