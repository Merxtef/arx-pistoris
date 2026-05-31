// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/pistoris.hpp"

#include <vector>

namespace cli::model {

struct Context {
  pistoris::Ftl ftl;
  pistoris::Ftl reference_ftl;
  bool has_reference = false;
  std::vector<pistoris::Tea> teas;
};

struct Invocation {
  const char* input  = nullptr;
  const char* output = nullptr;  // null = inspect
  std::vector<const char*> extras;
};

}  // namespace cli::model
