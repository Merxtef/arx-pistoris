// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_pistoris.h"
#include "arx_pistoris/ftl_data.hpp"
#include "arx_pistoris/tea_data.hpp"

namespace pistoris::ftl {

// Non-owning; lifetime tied to h.
inline Data& fromHandle(ArxFtlHandle h) { return *reinterpret_cast<Data*>(h); }

}  // namespace pistoris::ftl

namespace pistoris::tea {

// Non-owning; lifetime tied to h.
inline Data& fromHandle(ArxTeaHandle h) { return *reinterpret_cast<Data*>(h); }

}  // namespace pistoris::tea
