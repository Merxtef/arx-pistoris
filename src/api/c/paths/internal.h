
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/status.h"
#include "arx_pistoris/paths.h"
#include "arx_pistoris/paths.hpp"
#include "arx_pistoris/paths/types.h"

#include "api/c/internal.h"

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace pistoris::c_paths {

bool validOutput(char* out, std::size_t capacity, std::size_t* out_size) noexcept;

ArxReturnCode publishPath(std::string_view value, char* out, std::size_t capacity, std::size_t* out_size) noexcept;

bool valid(ArxModelPathView value) noexcept;

bool valid(ArxAnimationPathView value) noexcept;

bool valid(ArxCinematicPathView value) noexcept;

bool valid(ArxAmbiancePathView value) noexcept;

pistoris::paths::ModelPathView modelView(ArxModelPathView value) noexcept;

pistoris::paths::AnimationPathView animationView(ArxAnimationPathView value) noexcept;

ArxModelPathView cView(pistoris::paths::ModelPathView value) noexcept;

ArxAnimationPathView cView(pistoris::paths::AnimationPathView value) noexcept;

ArxCinematicPathView cView(pistoris::paths::CinematicPathView value) noexcept;

ArxAmbiancePathView cView(pistoris::paths::AmbiancePathView value) noexcept;

ArxResourceSearchLocation cView(pistoris::paths::ResourceSearchLocation value) noexcept;

ArxReturnCode typeAt(std::span<const std::string_view> types, std::size_t index, ArxStringView* out_type) noexcept;

template <class Builder>
ArxReturnCode buildPath(char* out, std::size_t capacity, std::size_t* out_size, Builder&& builder) noexcept {
  if (!validOutput(out, capacity, out_size)) return ARX_INVALID_DATA_POINTER;
  *out_size = 0;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    std::string result;
    if (!builder(result)) return ARX_INVALID_IDENTIFIER;
    return publishPath(result, out, capacity, out_size);
  });
}

template <class Parser>
ArxReturnCode parseLevel(ArxStringView path, std::uint32_t* out_level, Parser&& parser) noexcept {
  if (!pistoris::c_api::valid(path) || !out_level) return ARX_INVALID_DATA_POINTER;
  *out_level = 0;
  return pistoris::c_api::guard([&]() -> ArxReturnCode {
    std::uint32_t level = 0;
    if (!parser(pistoris::c_api::stringView(path), level)) return ARX_INVALID_IDENTIFIER;
    *out_level = level;
    return ARX_OK;
  });
}

}  // namespace pistoris::c_paths
