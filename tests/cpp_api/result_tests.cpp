// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/types.h"
#include "arx_pistoris/base/indexed_view.hpp"
#include "arx_pistoris/base/result.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/error.hpp"
#include "arx_pistoris/model/location.hpp"
#include "arx_pistoris/runtime.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {

struct TestLocation {
  std::size_t index = 0;
};

struct ThrowingMoveValue {
  static inline bool throw_on_move = false;

  int value = 0;

  ThrowingMoveValue() = default;
  explicit ThrowingMoveValue(int source_value) : value(source_value) {}
  ThrowingMoveValue(const ThrowingMoveValue&) = default;
  ThrowingMoveValue& operator=(const ThrowingMoveValue&) = default;
  // NOLINTNEXTLINE(bugprone-exception-escape,performance-noexcept-move-constructor): exercises throwing payloads
  ThrowingMoveValue(ThrowingMoveValue&& other) {
    if (throw_on_move) throw std::runtime_error("move failed");
    value = other.value;
  }
  // NOLINTNEXTLINE(bugprone-exception-escape,performance-noexcept-move-constructor): exercises throwing payloads
  ThrowingMoveValue& operator=(ThrowingMoveValue&& other) {
    if (throw_on_move) throw std::runtime_error("move failed");
    value = other.value;
    return *this;
  }
};

struct TestViewTag;

using TestResult = pistoris::Result<std::string, TestLocation>;
using TestView = pistoris::IndexedView<int, TestViewTag>;

static_assert(std::is_nothrow_move_constructible_v<TestResult>);
static_assert(std::is_same_v<decltype(std::declval<TestResult&>().error()), const TestResult::ErrorType*>);
static_assert(std::ranges::random_access_range<TestView>);
static_assert(std::ranges::sized_range<TestView>);
static_assert(std::ranges::borrowed_range<TestView>);

TEST_SUITE("C++ result") {
  TEST_CASE("Stores one success value") {
    TestResult result = TestResult::success("value");
    REQUIRE(result);
    CHECK(result.code() == ARX_OK);
    REQUIRE(result.get());
    CHECK(*result == "value");
    CHECK(result.error() == nullptr);
  }

  TEST_CASE("Stores one located error") {
    TestResult result = TestResult::failure(ARX_INDEX_OUT_OF_RANGE, TestLocation{.index = 7}, "vertex");
    CHECK_FALSE(result);
    CHECK(result.code() == ARX_INDEX_OUT_OF_RANGE);
    CHECK(result.get() == nullptr);
    REQUIRE(result.error());
    REQUIRE(result.error()->location());
    CHECK(result.error()->location()->index == 7);
    CHECK(result.error()->detail().compare("vertex") == 0);
  }

  TEST_CASE("Void results distinguish success from failure") {
    using VoidResult = pistoris::Result<void, TestLocation>;
    VoidResult success = VoidResult::success();
    CHECK(success);
    CHECK(success.code() == ARX_OK);

    VoidResult failure = VoidResult::failure(ARX_INVALID_STATE, std::nullopt);
    CHECK_FALSE(failure);
    CHECK(failure.code() == ARX_INVALID_STATE);
    REQUIRE(failure.error());
    CHECK_FALSE(failure.error()->location());
  }

  TEST_CASE("Failure propagation preserves immutable error data") {
    TestResult source = TestResult::failure(ARX_INDEX_OUT_OF_RANGE, TestLocation{.index = 9}, "group");
    pistoris::Result<int, TestLocation> propagated = std::move(source).propagate<int>();

    CHECK_FALSE(propagated);
    CHECK(propagated.code() == ARX_INDEX_OUT_OF_RANGE);
    REQUIRE(propagated.error());
    REQUIRE(propagated.error()->location());
    CHECK(propagated.error()->location()->index == 9);
    CHECK(propagated.error()->detail().compare("group") == 0);
  }

  TEST_CASE("Public error descriptions include semantic locations") {
    pistoris::ModelLocation location;
    location.resource_path = "graph/obj3d/interactive/items/example/example.ftl";
    location.element = pistoris::ModelElement::kFace;
    location.index = 4;
    location.subindex = 2;
    location.label = "blade";
    const auto result =
        pistoris::ModelResult<void>::failure(ARX_MODEL_BAD_FACE_VERTEX, std::move(location), "invalid corner");

    REQUIRE(result.error());
    const std::string description = pistoris::describeError(*result.error());
    CHECK(description.find(pistoris::errorString(result.code())) != std::string::npos);
    CHECK(description.find("face[4][2]") != std::string::npos);
    CHECK(description.find("example.ftl") != std::string::npos);
    CHECK(description.find("invalid corner") != std::string::npos);
  }

#ifdef NDEBUG
  TEST_CASE("ARX_OK cannot construct a failed Result") {
    TestResult result = TestResult::failure(ARX_OK, std::nullopt);
    CHECK_FALSE(result);
    CHECK(result.code() == ARX_INTERNAL_ERROR);
  }
#endif

  TEST_CASE("Throwing assignment preserves an inspectable failure state") {
    using ThrowingResult = pistoris::Result<ThrowingMoveValue, TestLocation>;
    ThrowingResult target = ThrowingResult::failure(ARX_INVALID_STATE, TestLocation{.index = 4});
    ThrowingResult source = ThrowingResult::success(ThrowingMoveValue{7});

    ThrowingMoveValue::throw_on_move = true;
    CHECK_THROWS_AS(target = std::move(source), std::runtime_error);
    ThrowingMoveValue::throw_on_move = false;

    CHECK_FALSE(target);
    CHECK(target.code() == ARX_INTERNAL_ERROR);
    REQUIRE(target.error());
    CHECK_FALSE(target.error()->location());
  }

  TEST_CASE("Indexed views provide runtime random-access range semantics") {
    pistoris::Animation animation;
    std::array<ArxAnimationKeyframeInput, 3> input{};
    input[0].keyframe.frame = 0;
    input[1].keyframe.frame = 4;
    input[2].keyframe.frame = 9;
    REQUIRE(animation.replaceKeyframes(10, input.data(), input.size()));

    const pistoris::Animation::KeyframesView view = animation.keyframes();
    REQUIRE(view.size() == input.size());
    CHECK(view[1].frame == 4);
    CHECK(view.end() - view.begin() == 3);
    CHECK((*(view.begin() + 2)).frame == 9);
    CHECK(view.begin()[1].frame == 4);
    CHECK(view.begin() < view.end());

    std::array<std::uint32_t, 3> frames{};
    std::ranges::transform(view, frames.begin(), &ArxAnimationKeyframe::frame);
    CHECK(frames == std::array<std::uint32_t, 3>{0, 4, 9});
  }
}

}  // namespace
