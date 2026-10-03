// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/binary.hpp"
#include "arx_pistoris/cinematic.hpp"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/images.hpp"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/bake.hpp"
#include "arx_pistoris/native.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
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
static_assert(noexcept(std::declval<const pistoris::Model&>().renderIcon(
    std::declval<const pistoris::Model::InventoryIconRenderOptions&>())));

static_assert(noexcept(std::declval<const pistoris::Animation&>().bakeNative()));
static_assert(noexcept(std::declval<const pistoris::Ambiance&>().bakeNative()));
static_assert(noexcept(
    std::declval<const pistoris::Model&>().bakeNativeBundle(std::declval<const pistoris::NativeModelBakeOptions&>())));
static_assert(noexcept(std::declval<const pistoris::Level&>().bakeNativeBundle(
    std::declval<const pistoris::Level::NativeBakeOptions&>())));

static_assert(noexcept(pistoris::Model::importGlb({})));
static_assert(noexcept(pistoris::Level::importGlb({})));
static_assert(noexcept(pistoris::Ambiance::importGlb({})));
static_assert(noexcept(pistoris::Model::importObj(std::string_view{}, std::string_view{})));

static_assert(std::is_nothrow_move_constructible_v<pistoris::Animation>);
static_assert(std::is_nothrow_move_assignable_v<pistoris::Animation>);
static_assert(std::is_nothrow_move_constructible_v<pistoris::Ambiance>);
static_assert(std::is_nothrow_move_assignable_v<pistoris::Ambiance>);
static_assert(std::is_nothrow_move_constructible_v<pistoris::Cinematic>);
static_assert(std::is_nothrow_move_assignable_v<pistoris::Cinematic>);
static_assert(std::is_nothrow_move_constructible_v<pistoris::Model>);
static_assert(std::is_nothrow_move_assignable_v<pistoris::Model>);
static_assert(std::is_nothrow_move_constructible_v<pistoris::Level>);
static_assert(std::is_nothrow_move_assignable_v<pistoris::Level>);

static_assert(noexcept(pistoris::readFtl({})));
static_assert(noexcept(pistoris::writeTea(std::declval<const pistoris::Tea&>())));
static_assert(noexcept(pistoris::binary::classifyTextEncoding({})));
static_assert(noexcept(pistoris::binary::latin1ToUtf8({}, std::declval<std::string&>())));
static_assert(noexcept(pistoris::binary::utf8ToLatin1({}, std::declval<std::string&>())));
static_assert(noexcept(pistoris::binary::validateEncodedImage({})));
static_assert(noexcept(pistoris::level_images::renderLoadingScreen({}, {},
                                                                   std::declval<std::vector<std::uint8_t>&>())));

TEST_CASE("C++ status API is noexcept") { CHECK(true); }
