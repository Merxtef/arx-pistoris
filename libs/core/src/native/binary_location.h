// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/native/location.hpp"

#include "utils/cursor.h"

#include <cstddef>
#include <string>

namespace pistoris::native_binary {

template <class Element>
[[nodiscard]] NativeBinaryLocation<Element> location(const CursorLocation& source, NativeBinaryRegion region) {
  NativeBinaryLocation<Element> result;
  result.element = static_cast<Element>(source.element);
  result.index = source.index;
  result.subindex = source.subindex;
  result.region = region;
  result.byte_offset = source.offset;
  result.field = std::string(source.field);
  return result;
}

template <class Element>
[[nodiscard]] NativeBinaryLocation<Element> semanticLocation(const NativeLocation<Element>& source,
                                                             NativeBinaryRegion region) {
  NativeBinaryLocation<Element> result;
  result.element = source.element;
  result.index = source.index;
  result.subindex = source.subindex;
  result.region = region;
  result.field = source.field;
  return result;
}

[[nodiscard]] inline DlfBinaryLocation dlfSemanticLocation(const DlfLocation& source, NativeBinaryRegion region) {
  DlfBinaryLocation result;
  result.element = DlfBinaryElement{source.element};
  result.index = source.index;
  result.subindex = source.subindex;
  result.region = region;
  result.field = source.field;
  return result;
}

[[nodiscard]] inline DlfBinaryLocation location(const CursorLocation& source, NativeBinaryRegion region) {
  DlfBinaryLocation result;
  result.element =
      source.secondary ? DlfBinaryElement{LlfElement(source.element)} : DlfBinaryElement{DlfElement(source.element)};
  result.index = source.index;
  result.subindex = source.subindex;
  result.region = region;
  result.byte_offset = source.offset;
  result.field = std::string(source.field);
  return result;
}

template <class Element>
void capture(NativeBinaryLocation<Element>& out, const ReadCursor& cursor, NativeBinaryRegion region) {
  const CursorError error = cursor.error();
  const CursorLocation current = cursor.location();
  const bool failed = error.kind != CursorErrorKind::kOk;
  out.element = static_cast<Element>(failed ? error.element : current.element);
  out.index = failed ? error.index : current.index;
  out.subindex = failed ? error.subindex : current.subindex;
  out.region = region;
  out.byte_offset = failed ? error.offset : current.offset;
  out.requested_bytes = failed ? error.needed : 0;
  out.field = failed ? std::string(error.field) : std::string(current.field);
}

template <class Element>
[[nodiscard]] NativeBinaryLocation<Element> location(const ReadCursor& cursor, NativeBinaryRegion region) {
  NativeBinaryLocation<Element> result;
  capture(result, cursor, region);
  return result;
}

inline void capture(DlfBinaryLocation& out, const ReadCursor& cursor, NativeBinaryRegion region) {
  const CursorError error = cursor.error();
  const CursorLocation current = cursor.location();
  const bool failed = error.kind != CursorErrorKind::kOk;
  const std::uint32_t element = failed ? error.element : current.element;
  const bool secondary = failed ? error.secondary : current.secondary;
  out.element = secondary ? DlfBinaryElement{LlfElement(element)} : DlfBinaryElement{DlfElement(element)};
  out.index = failed ? error.index : current.index;
  out.subindex = failed ? error.subindex : current.subindex;
  out.region = region;
  out.byte_offset = failed ? error.offset : current.offset;
  out.requested_bytes = failed ? error.needed : 0;
  out.field = failed ? std::string(error.field) : std::string(current.field);
}

[[nodiscard]] inline DlfBinaryLocation location(const ReadCursor& cursor, NativeBinaryRegion region) {
  DlfBinaryLocation result;
  capture(result, cursor, region);
  return result;
}

}  // namespace pistoris::native_binary
