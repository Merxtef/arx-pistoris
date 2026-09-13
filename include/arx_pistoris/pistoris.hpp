// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/ambiance.hpp"        // IWYU pragma: export
#include "arx_pistoris/ambiance/bake.hpp"   // IWYU pragma: export
#include "arx_pistoris/animation.hpp"       // IWYU pragma: export
#include "arx_pistoris/animation/bake.hpp"  // IWYU pragma: export
#include "arx_pistoris/base/audio.h"        // IWYU pragma: export
#include "arx_pistoris/base/flags.h"        // IWYU pragma: export
#include "arx_pistoris/base/image.h"        // IWYU pragma: export
#include "arx_pistoris/base/indices.h"      // IWYU pragma: export
#include "arx_pistoris/base/math.hpp"       // IWYU pragma: export
#include "arx_pistoris/base/status.h"       // IWYU pragma: export
#include "arx_pistoris/base/string_view.h"  // IWYU pragma: export
#include "arx_pistoris/binary.hpp"          // IWYU pragma: export
#include "arx_pistoris/glb.hpp"             // IWYU pragma: export
#include "arx_pistoris/level.hpp"           // IWYU pragma: export
#include "arx_pistoris/level/bake.hpp"      // IWYU pragma: export
#include "arx_pistoris/level/images.hpp"    // IWYU pragma: export
#include "arx_pistoris/model.hpp"           // IWYU pragma: export
#include "arx_pistoris/model/bake.hpp"      // IWYU pragma: export
#include "arx_pistoris/model/glb.hpp"       // IWYU pragma: export
#include "arx_pistoris/model/obj.hpp"       // IWYU pragma: export
#include "arx_pistoris/native.hpp"          // IWYU pragma: export
#include "arx_pistoris/paths.hpp"           // IWYU pragma: export
#include "arx_pistoris/runtime.hpp"         // IWYU pragma: export
#include "arx_pistoris/runtime/types.h"     // IWYU pragma: export
#include "arx_pistoris/sound.hpp"           // IWYU pragma: export
#include "arx_pistoris/texture.hpp"         // IWYU pragma: export

#ifndef ARX_PISTORIS_CPP_API
#error "pistoris.hpp requires the arx_pistoris_cpp target."
#endif
