// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/amb.hpp"

#include "utils/cursor.h"

namespace pistoris {

ArxReturnCode loadAmb(amb::Data* data, ReadCursor& cursor);
ArxReturnCode saveAmb(const amb::Data* data, WriteCursor& cursor);
ArxReturnCode validateAmb(const amb::Data* data);
void canonicalizeAmb(amb::Data& data) noexcept;

}  // namespace pistoris
