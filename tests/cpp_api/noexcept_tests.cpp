// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/binary.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/images.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/native.hpp"

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

static_assert(noexcept(std::declval<const pistoris::Animation&>().validate()));
static_assert(noexcept(std::declval<const pistoris::Ambiance&>().validate()));
static_assert(noexcept(std::declval<const pistoris::Model&>().validate()));
static_assert(noexcept(std::declval<const pistoris::Level&>().validate()));

static_assert(noexcept(std::declval<pistoris::Animation&>().setResourcePath({})));
static_assert(noexcept(std::declval<pistoris::Ambiance&>().setResourcePath({})));
static_assert(noexcept(std::declval<pistoris::Ambiance&>().trimTracksToMaster()));
static_assert(noexcept(std::declval<pistoris::Model&>().setResourcePath({})));
static_assert(noexcept(std::declval<pistoris::Level&>().setResourcePath({})));

static_assert(noexcept(std::declval<const pistoris::Animation&>().bakeNative(std::declval<pistoris::Tea&>())));
static_assert(noexcept(std::declval<const pistoris::Ambiance&>().bakeNative(std::declval<pistoris::Amb&>())));
static_assert(noexcept(std::declval<const pistoris::Model&>().bakeNativeBundle(
    std::declval<const pistoris::NativeTextureBakeOptions&>(), std::declval<pistoris::NativeModelBundle&>())));
static_assert(noexcept(std::declval<const pistoris::Level&>().bakeNativeBundle(
    std::declval<const pistoris::Level::NativeBakeOptions&>(), std::declval<pistoris::NativeLevelBundle&>())));

static_assert(noexcept(pistoris::Model::importGlb(std::declval<pistoris::Model&>(), {})));
static_assert(noexcept(pistoris::Level::importGlb(std::declval<pistoris::Level&>(), {})));
static_assert(noexcept(pistoris::Ambiance::importGlb(std::declval<pistoris::Ambiance&>(), {})));
static_assert(noexcept(pistoris::Model::importObj(std::declval<pistoris::Model&>(), std::string_view{},
                                                  std::string_view{})));

static_assert(noexcept(pistoris::readFtl({}, std::declval<pistoris::Ftl&>())));
static_assert(noexcept(pistoris::writeTea(std::declval<const pistoris::Tea&>(),
                                          std::declval<std::vector<std::uint8_t>&>())));
static_assert(noexcept(pistoris::binary::validateEncodedImage({})));
static_assert(noexcept(pistoris::level_images::renderLoadingScreenPng(
    {}, pistoris::level_images::LoadingScreenLayout::kOriginal, std::declval<std::vector<std::uint8_t>&>())));

TEST_CASE("C++ status API is noexcept") { CHECK(true); }
