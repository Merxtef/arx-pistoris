// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/texture.hpp"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {

struct ModelModules;
struct ObjBundle;

ArxReturnCode importObjToModel(std::string_view obj, std::span<const ObjMaterialLibraryView> material_libraries,
                               ModelModules& out, std::vector<std::string>* texture_source_paths = nullptr);
ArxReturnCode exportModelToObj(const ModelModules& model, std::string_view stem, const ObjExportOptions& options,
                               ObjBundle& out);

}  // namespace pistoris
