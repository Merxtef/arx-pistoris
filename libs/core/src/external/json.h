// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/json/location.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/native/text.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace pistoris {

AmbResult<std::string> exportAmbToJson(const amb::Data& data, bool pretty, NativeTextMode text_mode);
JsonResult<amb::Data> importJsonToAmb(std::string_view text, NativeTextMode text_mode);
FtlResult<std::string> exportFtlToJson(const ftl::Data& d, bool pretty, NativeTextMode text_mode);
JsonResult<ftl::Data> importJsonToFtl(std::string_view text, NativeTextMode text_mode);
FtsResult<std::string> exportFtsToJson(const fts::Data& data, std::uint32_t level, bool pretty,
                                       NativeTextMode text_mode);
JsonResult<FtsJsonImport> importJsonToFts(std::string_view text, NativeTextMode text_mode);
DlfResult<std::string> exportDlfToJson(const dlf::Data& data, bool pretty, std::string_view signer,
                                       NativeTextMode text_mode);
JsonResult<dlf::Data> importJsonToDlf(std::string_view text, NativeTextMode text_mode);
LlfResult<std::string> exportLlfToJson(const llf::Data& data, bool pretty, std::string_view signer);
JsonResult<llf::Data> importJsonToLlf(std::string_view text);
TeaResult<std::string> exportTeaToJson(const tea::Data& d, bool pretty, NativeTextMode text_mode);
JsonResult<tea::Data> importJsonToTea(std::string_view text, NativeTextMode text_mode);

}  // namespace pistoris
