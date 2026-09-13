// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/paths/types.h"

#include "modules/resource.h"

#include <string_view>

namespace pistoris::resource {

Error validatePath(std::string_view path, ArxResourceKind kind) noexcept;

}  // namespace pistoris::resource
