// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#ifndef ARX_PISTORIS_INDICES_H
#define ARX_PISTORIS_INDICES_H

#include "arx_pistoris/api.h"

#include <stdint.h>

// Public collection index types
// NOLINTBEGIN(readability-identifier-naming)

typedef uint32_t ArxRoomIndex;
typedef uint32_t ArxPortalIndex;
typedef uint32_t ArxAnchorIndex;
typedef uint32_t ArxAnchorConnectionIndex;
typedef uint32_t ArxNavSurfaceVertexIndex;
typedef uint32_t ArxLightIndex;
typedef uint32_t ArxEntityIndex;
typedef uint32_t ArxFogIndex;
typedef uint32_t ArxZoneIndex;
typedef uint32_t ArxPathIndex;
typedef uint32_t ArxVertexIndex;
typedef uint32_t ArxFaceIndex;
typedef uint32_t ArxTextureIndex;

#define ARX_NO_TEXTURE UINT32_MAX

// NOLINTEND(readability-identifier-naming)

#ifdef __cplusplus
namespace pistoris {

using VertexIndex = ::ArxVertexIndex;
using FaceIndex = ::ArxFaceIndex;
using TextureIndex = ::ArxTextureIndex;
using RoomIndex = ::ArxRoomIndex;
using PortalIndex = ::ArxPortalIndex;
using AnchorIndex = ::ArxAnchorIndex;
using AnchorConnectionIndex = ::ArxAnchorConnectionIndex;
using NavSurfaceVertexIndex = ::ArxNavSurfaceVertexIndex;
using LightIndex = ::ArxLightIndex;
using EntityIndex = ::ArxEntityIndex;
using FogIndex = ::ArxFogIndex;
using ZoneIndex = ::ArxZoneIndex;
using PathIndex = ::ArxPathIndex;

inline constexpr VertexIndex kInvalidVertexIndex = ARX_INVALID_INDEX;
inline constexpr FaceIndex kInvalidFaceIndex = ARX_INVALID_INDEX;
inline constexpr TextureIndex kNoTexture = ARX_NO_TEXTURE;
inline constexpr RoomIndex kInvalidRoomIndex = ARX_INVALID_INDEX;
inline constexpr PortalIndex kInvalidPortalIndex = ARX_INVALID_INDEX;
inline constexpr AnchorIndex kInvalidAnchorIndex = ARX_INVALID_INDEX;
inline constexpr AnchorConnectionIndex kInvalidAnchorConnectionIndex = ARX_INVALID_INDEX;
inline constexpr NavSurfaceVertexIndex kInvalidNavSurfaceVertexIndex = ARX_INVALID_INDEX;
inline constexpr LightIndex kInvalidLightIndex = ARX_INVALID_INDEX;
inline constexpr EntityIndex kInvalidEntityIndex = ARX_INVALID_INDEX;
inline constexpr FogIndex kInvalidFogIndex = ARX_INVALID_INDEX;
inline constexpr ZoneIndex kInvalidZoneIndex = ARX_INVALID_INDEX;
inline constexpr PathIndex kInvalidPathIndex = ARX_INVALID_INDEX;

}  // namespace pistoris
#endif

#endif /* ARX_PISTORIS_INDICES_H */
