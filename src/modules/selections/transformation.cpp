// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/math.hpp"

#include "modules/selections.h"
#include "utils/math/finite.h"
#include "utils/math/mat3.h"

#include <optional>

namespace pistoris::selections {
namespace {

template <class Transform>
Error validateLeadingVertices(const SelectionsData& selections, Transform transform) noexcept {
  for (SelectionId id = 0; id < 64U; ++id) {
    const std::optional<SelectionLeadingVertex>& leading_vertex = selections.slots[id].leading_vertex;
    if (!occupied(selections, id) || !leading_vertex) continue;
    const SelectionLeadingVertex& leading = *leading_vertex;
    if (!math::finite(transform(leading.position))) return Error::kBadLeadingPosition;
  }
  return Error::kNone;
}

template <class Transform>
void applyToLeadingVertices(SelectionsData& selections, Transform transform) noexcept {
  for (SelectionId id = 0; id < 64U; ++id) {
    std::optional<SelectionLeadingVertex>& leading_vertex = selections.slots[id].leading_vertex;
    if (!occupied(selections, id) || !leading_vertex) continue;
    SelectionLeadingVertex& leading = *leading_vertex;
    leading.position = transform(leading.position);
  }
}

}  // namespace

Error validateScale(const SelectionsData& selections, float factor) noexcept {
  return validateLeadingVertices(selections, [factor](const ArxVector3& position) { return position * factor; });
}

void applyScale(SelectionsData& selections, float factor) noexcept {
  applyToLeadingVertices(selections, [factor](const ArxVector3& position) { return position * factor; });
}

Error validateRotation(const SelectionsData& selections, const ArxMat3& rotation) noexcept {
  return validateLeadingVertices(selections, [&rotation](const ArxVector3& position) { return rotation * position; });
}

void applyRotation(SelectionsData& selections, const ArxMat3& rotation) noexcept {
  applyToLeadingVertices(selections, [&rotation](const ArxVector3& position) { return rotation * position; });
}

Error validateTranslation(const SelectionsData& selections, const ArxVector3& offset) noexcept {
  return validateLeadingVertices(selections, [&offset](const ArxVector3& position) { return position + offset; });
}

void applyTranslation(SelectionsData& selections, const ArxVector3& offset) noexcept {
  applyToLeadingVertices(selections, [&offset](const ArxVector3& position) { return position + offset; });
}

}  // namespace pistoris::selections
