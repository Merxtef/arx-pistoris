// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/glb/location.hpp"

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace pistoris::glb {

struct Failure {
  std::optional<GlbLocation> location = GlbLocation{};
  std::string detail;
};

inline GlbLocation failureLocation(GlbElement element, std::size_t index = kNoElementIndex,
                                   std::size_t subindex = kNoElementIndex, std::string_view label = {},
                                   std::string_view property = {}) {
  GlbLocation result;
  result.element = element;
  result.index = index;
  result.subindex = subindex;
  result.label = label;
  result.property = property;
  return result;
}

inline void setFailureLocation(Failure* failure, GlbLocation location) {
  if (failure == nullptr) return;
  failure->location = std::move(location);
  failure->detail.clear();
}

inline ArxReturnCode recordFailure(Failure* failure, ArxReturnCode code, std::optional<GlbLocation> location,
                                   std::string detail) {
  if (failure != nullptr) {
    failure->location = std::move(location);
    failure->detail = std::move(detail);
  }
  return code;
}

template <class... Args>
ArxReturnCode recordFailure(Failure* failure, ArxReturnCode code, std::optional<GlbLocation> location,
                            std::format_string<Args...> detail, Args&&... args) {
  if (failure != nullptr) {
    failure->location = std::move(location);
    failure->detail = std::format(detail, std::forward<Args>(args)...);
  }
  return code;
}

}  // namespace pistoris::glb
