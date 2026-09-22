// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_H
#define ARX_PISTORIS_H

#ifdef ARX_PISTORIS_CPP_API
#error "arx_pistoris.h is the C ABI header; use pistoris.hpp for the C++ API target."
#endif

#include "arx_pistoris/ambiance.h"          // IWYU pragma: export
#include "arx_pistoris/animation.h"         // IWYU pragma: export
#include "arx_pistoris/base/abi.h"          // IWYU pragma: export
#include "arx_pistoris/base/audio.h"        // IWYU pragma: export
#include "arx_pistoris/base/buffer.h"       // IWYU pragma: export
#include "arx_pistoris/base/flags.h"        // IWYU pragma: export
#include "arx_pistoris/base/image.h"        // IWYU pragma: export
#include "arx_pistoris/base/indices.h"      // IWYU pragma: export
#include "arx_pistoris/base/math.h"         // IWYU pragma: export
#include "arx_pistoris/base/status.h"       // IWYU pragma: export
#include "arx_pistoris/base/string_view.h"  // IWYU pragma: export
#include "arx_pistoris/binary.h"            // IWYU pragma: export
#include "arx_pistoris/cinematic.h"         // IWYU pragma: export
#include "arx_pistoris/glb.h"               // IWYU pragma: export
#include "arx_pistoris/level.h"             // IWYU pragma: export
#include "arx_pistoris/level/images.h"      // IWYU pragma: export
#include "arx_pistoris/model.h"             // IWYU pragma: export
#include "arx_pistoris/native.h"            // IWYU pragma: export
#include "arx_pistoris/paths.h"             // IWYU pragma: export
#include "arx_pistoris/runtime.h"           // IWYU pragma: export
#include "arx_pistoris/runtime/types.h"     // IWYU pragma: export
#include "arx_pistoris/sound.h"             // IWYU pragma: export
#include "arx_pistoris/texture.h"           // IWYU pragma: export

#endif /*ARX_PISTORIS_H*/
