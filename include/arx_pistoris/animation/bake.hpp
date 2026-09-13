// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/native/tea.hpp"
#include "arx_pistoris/sound.hpp"

#include <vector>

namespace pistoris {

struct NativeAnimationBundle {
  tea::Data tea;
  std::vector<SoundFile> sound_files;
};

}  // namespace pistoris
