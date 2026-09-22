// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native.h"
#include "arx_pistoris/native/amb.hpp"
#include "arx_pistoris/native/cin.hpp"
#include "arx_pistoris/native/dlf.hpp"
#include "arx_pistoris/native/ftl.hpp"
#include "arx_pistoris/native/fts.hpp"
#include "arx_pistoris/native/llf.hpp"
#include "arx_pistoris/native/tea.hpp"

#include "api/c/internal.h"

struct arx_pistoris_amb {
  pistoris::amb::Data value;
};

struct arx_pistoris_cin {
  pistoris::cin::Data value;
};

struct arx_pistoris_dlf {
  pistoris::dlf::Data value;
};

struct arx_pistoris_ftl {
  pistoris::ftl::Data value;
};

struct arx_pistoris_fts {
  pistoris::fts::Data value;
};

struct arx_pistoris_llf {
  pistoris::llf::Data value;
};

struct arx_pistoris_tea {
  pistoris::tea::Data value;
};
