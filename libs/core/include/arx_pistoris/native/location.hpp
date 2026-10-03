// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/location.hpp"
#include "arx_pistoris/base/result.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

namespace pistoris {

template <class Element>
struct NativeLocation {
  Element element = {};
  std::size_t index = kNoElementIndex;
  std::size_t subindex = kNoElementIndex;
  std::string field;
};

enum class NativeBinaryRegion : std::uint8_t {
  kStored,
  kDecodedPayload,
};

template <class Element>
struct NativeBinaryLocation {
  Element element = {};
  std::size_t index = kNoElementIndex;
  std::size_t subindex = kNoElementIndex;
  NativeBinaryRegion region = NativeBinaryRegion::kStored;
  std::size_t byte_offset = kNoElementIndex;
  std::size_t requested_bytes = 0;
  std::string field;
};

enum class AmbElement : std::uint8_t { kHeader, kTrack, kKey, kSound };
enum class CinElement : std::uint8_t { kHeader, kIllustration, kKeyframe, kSound };
enum class DlfElement : std::uint8_t { kHeader, kEntity, kFog, kZone, kZonePoint, kPath, kPathNode };
enum class FtlElement : std::uint8_t {
  kHeader,
  kVertex,
  kFace,
  kTexture,
  kBone,
  kActionPoint,
  kSelection,
};
enum class FtsElement : std::uint8_t {
  kHeader,
  kCell,
  kVertex,
  kFace,
  kTexture,
  kRoom,
  kPortal,
  kRoomDistance,
  kAnchor,
  kAnchorConnection,
};
enum class LlfElement : std::uint8_t { kHeader, kLight, kVertexColor };
enum class TeaElement : std::uint8_t { kHeader, kKeyframe, kGroupTransform, kSound };

using AmbLocation = NativeLocation<AmbElement>;
using CinLocation = NativeLocation<CinElement>;
using DlfLocation = NativeLocation<DlfElement>;
using FtlLocation = NativeLocation<FtlElement>;
using FtsLocation = NativeLocation<FtsElement>;
using LlfLocation = NativeLocation<LlfElement>;
using TeaLocation = NativeLocation<TeaElement>;

using AmbBinaryLocation = NativeBinaryLocation<AmbElement>;
using CinBinaryLocation = NativeBinaryLocation<CinElement>;
using DlfBinaryElement = std::variant<DlfElement, LlfElement>;
using DlfBinaryLocation = NativeBinaryLocation<DlfBinaryElement>;
using FtlBinaryLocation = NativeBinaryLocation<FtlElement>;
using FtsBinaryLocation = NativeBinaryLocation<FtsElement>;
using LlfBinaryLocation = NativeBinaryLocation<LlfElement>;
using TeaBinaryLocation = NativeBinaryLocation<TeaElement>;

using DlfWriteLocation = std::variant<DlfLocation, LlfLocation>;

template <class T>
using AmbResult = Result<T, AmbLocation>;
template <class T>
using CinResult = Result<T, CinLocation>;
template <class T>
using DlfResult = Result<T, DlfLocation>;
template <class T>
using FtlResult = Result<T, FtlLocation>;
template <class T>
using FtsResult = Result<T, FtsLocation>;
template <class T>
using LlfResult = Result<T, LlfLocation>;
template <class T>
using TeaResult = Result<T, TeaLocation>;

template <class T>
using AmbBinaryResult = Result<T, AmbBinaryLocation>;
template <class T>
using CinBinaryResult = Result<T, CinBinaryLocation>;
template <class T>
using DlfBinaryResult = Result<T, DlfBinaryLocation>;
template <class T>
using FtlBinaryResult = Result<T, FtlBinaryLocation>;
template <class T>
using FtsBinaryResult = Result<T, FtsBinaryLocation>;
template <class T>
using LlfBinaryResult = Result<T, LlfBinaryLocation>;
template <class T>
using TeaBinaryResult = Result<T, TeaBinaryLocation>;

template <class T>
using DlfWriteResult = Result<T, DlfWriteLocation>;

}  // namespace pistoris
