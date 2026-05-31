// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris.hpp"

namespace cli::animation {

struct Context {
  pistoris::Tea tea;
};

struct Invocation {
  const char* input  = nullptr;
  const char* output = nullptr;  // null = inspect
};

}  // namespace cli::animation
