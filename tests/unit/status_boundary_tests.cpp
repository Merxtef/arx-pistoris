// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/base/error.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/result.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/cinematic/location.hpp"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/level/location.hpp"
#include "arx_pistoris/model/location.hpp"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/runtime.hpp"
#include "arx_pistoris/runtime/types.h"
#include "arx_pistoris/sound.hpp"

#include "api/c/internal.h"
#include "api/result_failure.h"
#include "api/status_boundary.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

struct LogCapture {
  int errors = 0;
  int debug = 0;
  std::vector<std::string> messages;
};

void captureLog(ArxLogLevel level, const char* message, void* userdata) {
  auto& capture = *static_cast<LogCapture*>(userdata);
  if (level == ARX_LOG_ERROR) ++capture.errors;
  if (level == ARX_LOG_DEBUG) ++capture.debug;
  capture.messages.emplace_back(message);
}

}  // namespace

TEST_SUITE("api::status_boundary") {
  TEST_CASE("C guards replace the previous error snapshot on every call") {
    ArxError error = ARX_ERROR_INIT;
    const pistoris::ModelLocation location = pistoris::api_detail::resourceLocation(pistoris::ModelElement::kFace, 7);

    CHECK(pistoris::c_api::guard(&error, [&] {
            const auto failure = pistoris::ModelResult<void>::failure(ARX_MODEL_BAD_FACE_VERTEX, location);
            return pistoris::c_api::publish(failure, &error);
          }) == ARX_MODEL_BAD_FACE_VERTEX);
    CHECK(error.location.kind == ARX_ERROR_LOCATION_MODEL);
    CHECK(error.location.index == 7);
    CHECK(error.private_data != nullptr);

    CHECK(pistoris::c_api::guard(&error, [] { return ARX_MODEL_BAD_FACE_VERTEX; }) == ARX_MODEL_BAD_FACE_VERTEX);
    CHECK(error.code == ARX_MODEL_BAD_FACE_VERTEX);
    CHECK(error.location.kind == ARX_ERROR_LOCATION_NONE);
    CHECK(error.private_data == nullptr);

    CHECK(pistoris::c_api::guard(&error, [] { return ARX_OK; }) == ARX_OK);
    CHECK(error.code == ARX_OK);
    CHECK(error.private_data == nullptr);

    CHECK(pistoris::c_api::guard(&error, []() -> ArxReturnCode { throw std::bad_alloc(); }) == ARX_BAD_ALLOC);
    CHECK(error.code == ARX_BAD_ALLOC);
    CHECK(pistoris::c_api::guard(&error, []() -> ArxReturnCode { throw std::runtime_error("failure"); }) ==
          ARX_INTERNAL_ERROR);
    CHECK(error.code == ARX_INTERNAL_ERROR);
    pistoris::c_api::clearError(&error);
  }

  TEST_CASE("Preserves status results and translates escaping exceptions") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);
    CHECK(pistoris::api_detail::statusBoundary([] { return ARX_INVALID_OPTIONS; }) == ARX_INVALID_OPTIONS);
    CHECK(pistoris::api_detail::statusBoundary([]() -> ArxReturnCode { throw std::bad_alloc(); }) == ARX_BAD_ALLOC);
    CHECK(pistoris::api_detail::statusBoundary([]() -> ArxReturnCode { throw std::runtime_error("failure"); }) ==
          ARX_INTERNAL_ERROR);
    CHECK(pistoris::api_detail::statusBoundary([]() -> ArxReturnCode { throw 1; }) == ARX_INTERNAL_ERROR);
    pistoris::setLogCallback(nullptr, nullptr);
    CHECK(capture.errors == 0);
    CHECK(capture.debug == 3);
  }

  TEST_CASE("Validation boundaries report source failures at debug level") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    const pistoris::ModelLocation location = pistoris::api_detail::resourceLocation(pistoris::ModelElement::kResource);

    const pistoris::ModelResult<void> invalid = pistoris::api_detail::validationBoundary<pistoris::ModelResult<void>>(
        [] { return ARX_MODEL_NO_GEOMETRY; }, location);
    const pistoris::ModelResult<void> allocation =
        pistoris::api_detail::validationBoundary<pistoris::ModelResult<void>>(
            []() -> ArxReturnCode { throw std::bad_alloc(); }, location);
    const pistoris::ModelResult<void> unexpected =
        pistoris::api_detail::validationBoundary<pistoris::ModelResult<void>>(
            []() -> ArxReturnCode { throw std::runtime_error("failure"); }, location);

    pistoris::setLogCallback(nullptr, nullptr);
    CHECK(invalid.code() == ARX_MODEL_NO_GEOMETRY);
    CHECK(allocation.code() == ARX_BAD_ALLOC);
    CHECK(unexpected.code() == ARX_INTERNAL_ERROR);
    CHECK(capture.errors == 0);
    CHECK(capture.debug == 3);
  }

  TEST_CASE("Reporting a validation result preserves its error and logs once") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    const pistoris::ModelLocation location = pistoris::api_detail::resourceLocation(pistoris::ModelElement::kFace, 4);
    pistoris::ModelResult<void> validation = pistoris::api_detail::validationBoundary<pistoris::ModelResult<void>>(
        [] { return ARX_MODEL_BAD_FACE_VERTEX; }, location);
    const pistoris::ModelResult<std::size_t> result =
        pistoris::api_detail::modelFailure<std::size_t>(std::move(validation), "Model compaction");

    pistoris::setLogCallback(nullptr, nullptr);
    REQUIRE(result.error() != nullptr);
    REQUIRE(result.error()->location().has_value());
    CHECK(result.error()->location()->element == pistoris::ModelElement::kFace);
    CHECK(result.error()->location()->index == 4);
    CHECK(capture.errors == 0);
    CHECK(capture.debug == 1);
  }

  TEST_CASE("Generic result construction and propagation are silent") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    pistoris::ModelResult<void> source = pistoris::ModelResult<void>::failure(
        ARX_MODEL_BAD_FACE_VERTEX, pistoris::api_detail::resourceLocation(pistoris::ModelElement::kFace, 1));
    const auto propagated = std::move(source).propagate<std::size_t>();

    pistoris::setLogCallback(nullptr, nullptr);
    CHECK_FALSE(propagated);
    CHECK(capture.errors == 0);
    CHECK(capture.debug == 0);
  }

  TEST_CASE("Cross-domain failure remapping preserves the source log") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    auto source = pistoris::api_detail::modelFailure<void>(
        ARX_MODEL_BAD_FACE_VERTEX,
        pistoris::api_detail::resourceLocation("items/example.ftl", pistoris::ModelElement::kFace, 2),
        "invalid face");
    const auto remapped = pistoris::api_detail::remapFailure<std::size_t, pistoris::LevelGlbExportLocation>(
        std::move(source),
        [](const pistoris::ModelLocation& location) { return pistoris::LevelGlbExportLocation{location}; });

    pistoris::setLogCallback(nullptr, nullptr);
    REQUIRE(remapped.error() != nullptr);
    REQUIRE(remapped.error()->location().has_value());
    const auto* location = std::get_if<pistoris::ModelLocation>(&*remapped.error()->location());
    REQUIRE(location != nullptr);
    CHECK(location->resource_path == "items/example.ftl");
    CHECK(location->element == pistoris::ModelElement::kFace);
    CHECK(location->index == 2);
    CHECK(remapped.error()->detail().compare("invalid face") == 0);
    CHECK(capture.errors == 0);
    CHECK(capture.debug == 1);
  }

  TEST_CASE("Native binary source failures log once at debug level") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    const std::array<std::uint8_t, 4> malformed{};
    const auto result = pistoris::readFtl(malformed);

    pistoris::setLogCallback(nullptr, nullptr);
    CHECK_FALSE(result);
    CHECK(capture.errors == 0);
    CHECK(capture.debug == 1);
    REQUIRE(capture.messages.size() == 1);
    CHECK(capture.messages.front().find("FTL binary read source failure") != std::string::npos);
  }

  TEST_CASE("Domain boundaries report returned and escaping failures once") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    const pistoris::ModelLocation location = pistoris::api_detail::resourceLocation(pistoris::ModelElement::kFace, 7);
    const pistoris::ModelResult<void> returned = pistoris::api_detail::modelBoundary(
        [&] { return pistoris::api_detail::modelFailure<void>(ARX_MODEL_BAD_FACE_VERTEX, location, "invalid face"); });
    const pistoris::ModelResult<void> allocation =
        pistoris::api_detail::modelBoundary([]() -> pistoris::ModelResult<void> { throw std::bad_alloc(); });
    const pistoris::ModelResult<void> unexpected = pistoris::api_detail::modelBoundary(
        []() -> pistoris::ModelResult<void> { throw std::runtime_error("failure"); });

    pistoris::setLogCallback(nullptr, nullptr);
    REQUIRE(returned.error() != nullptr);
    REQUIRE(returned.error()->location().has_value());
    CHECK(returned.error()->location()->element == pistoris::ModelElement::kFace);
    CHECK(returned.error()->location()->index == 7);
    CHECK(returned.error()->detail().compare("invalid face") == 0);
    CHECK(allocation.code() == ARX_BAD_ALLOC);
    CHECK(unexpected.code() == ARX_INTERNAL_ERROR);
    CHECK(capture.errors == 0);
    CHECK(capture.debug == 3);
  }

  TEST_CASE("Domain status boundaries report ordinary failures once") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    const pistoris::ModelLocation location =
        pistoris::api_detail::resourceLocation(pistoris::ModelElement::kSelection, 3);
    const pistoris::ModelResult<void> result =
        pistoris::api_detail::modelStatusBoundary<void>([] { return ARX_INDEX_OUT_OF_RANGE; }, location);

    pistoris::setLogCallback(nullptr, nullptr);
    REQUIRE(result.error() != nullptr);
    REQUIRE(result.error()->location().has_value());
    CHECK(result.error()->location()->index == 3);
    CHECK(capture.errors == 0);
    CHECK(capture.debug == 1);
  }

  TEST_CASE("Domain failure logs semantic operation and location") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    pistoris::GlbLocation location;
    location.element = pistoris::GlbElement::kMesh;
    location.index = 2;
    location.label = "Body";
    const pistoris::GlbResult<void> result = pistoris::api_detail::glbFailure<void>(
        ARX_GLB_BAD_MODEL_GEOMETRY, location, "invalid face", "GLB -> Model conversion");

    pistoris::setLogCallback(nullptr, nullptr);
    CHECK_FALSE(result);
    CHECK(result.code() == ARX_GLB_BAD_MODEL_GEOMETRY);
    CHECK(capture.errors == 0);
    CHECK(capture.debug == 1);
    REQUIRE(capture.messages.size() == 1);
    CHECK(capture.messages.front().find("GLB -> Model conversion source failure") != std::string::npos);
    CHECK(capture.messages.front().find("GLB mesh[2] 'Body'") != std::string::npos);
    CHECK(capture.messages.front().find("invalid face") != std::string::npos);
    CHECK(capture.messages.front().find("glbFailure") == std::string::npos);
  }

  TEST_CASE("Cinematic failure logs typed sound and language identities") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    pistoris::SoundHandle sound = pistoris::kNoSoundHandle;
    REQUIRE(pistoris::soundHandle(pistoris::SoundKind::kSpeech, 3, sound) == ARX_OK);
    const pistoris::CinematicLocation location =
        pistoris::api_detail::cinematicSoundEncodingLocation("cinematic:test", sound, 7);
    const pistoris::CinematicResult<void> result = pistoris::api_detail::cinematicFailure<void>(
        ARX_CINEMATIC_BAD_LANGUAGE, location, {}, "Cinematic sound update");

    pistoris::setLogCallback(nullptr, nullptr);
    CHECK_FALSE(result);
    REQUIRE(capture.messages.size() == 1);
    CHECK(capture.messages.front().find("speech sound encoding[3] language[7]") != std::string::npos);
    CHECK(capture.messages.front().find("cinematic:test") != std::string::npos);
  }

  TEST_CASE("Nested GLB failure locations name both owning arrays") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    pistoris::GlbLocation location;
    location.element = pistoris::GlbElement::kPrimitive;
    location.index = 2;
    location.subindex = 4;
    location.label = "Body";
    location.property = "POSITION";
    const pistoris::GlbResult<void> result = pistoris::api_detail::glbFailure<void>(
        ARX_GLB_BAD_MODEL_GEOMETRY, location, "invalid accessor", "GLB -> Model conversion");

    pistoris::setLogCallback(nullptr, nullptr);
    CHECK_FALSE(result);
    REQUIRE(capture.messages.size() == 1);
    CHECK(capture.messages.front().find("GLB mesh[2] primitive[4] 'Body' property 'POSITION'") != std::string::npos);
  }

  TEST_CASE("Resource boundaries enrich failures without logging again") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    const pistoris::ModelLocation location = pistoris::api_detail::resourceLocation(pistoris::ModelElement::kFace, 2);
    const pistoris::ModelResult<void> result = pistoris::api_detail::modelBoundary(
        "graph/obj3d/interactive/items/example/example.ftl",
        [&] { return pistoris::api_detail::modelFailure<void>(ARX_MODEL_BAD_FACE_VERTEX, location); });

    pistoris::setLogCallback(nullptr, nullptr);
    REQUIRE(result.error() != nullptr);
    REQUIRE(result.error()->location().has_value());
    CHECK(result.error()->location()->resource_path == "graph/obj3d/interactive/items/example/example.ftl");
    CHECK(capture.errors == 0);
    CHECK(capture.debug == 1);
  }

  TEST_CASE("Resource boundaries locate exception failures at their owning resource") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    const auto allocation =
        pistoris::api_detail::modelBoundary("graph/obj3d/interactive/items/example/example.ftl",
                                            []() -> pistoris::ModelResult<void> { throw std::bad_alloc(); });
    const auto unexpected = pistoris::api_detail::modelBoundary(
        "graph/obj3d/interactive/items/example/example.ftl",
        []() -> pistoris::ModelResult<void> { throw std::runtime_error("failure"); });

    pistoris::setLogCallback(nullptr, nullptr);
    for (const auto* result : {&allocation, &unexpected}) {
      REQUIRE(result->error() != nullptr);
      REQUIRE(result->error()->location().has_value());
      CHECK(result->error()->location()->element == pistoris::ModelElement::kResource);
      CHECK(result->error()->location()->resource_path == "graph/obj3d/interactive/items/example/example.ftl");
    }
    CHECK(allocation.code() == ARX_BAD_ALLOC);
    CHECK(unexpected.code() == ARX_INTERNAL_ERROR);
    CHECK(capture.errors == 0);
    CHECK(capture.debug == 2);
    REQUIRE(capture.messages.size() == 2);
    CHECK(capture.messages[0].find("graph/obj3d/interactive/items/example/example.ftl") != std::string::npos);
    CHECK(capture.messages[1].find("graph/obj3d/interactive/items/example/example.ftl") != std::string::npos);
  }

  TEST_CASE("Resource enrichment supplies a broad location without becoming a source log") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    const auto result = pistoris::api_detail::modelBoundary("graph/obj3d/interactive/items/example/example.ftl", [] {
      return pistoris::ModelResult<void>::failure(ARX_INVALID_OPTIONS, std::nullopt);
    });

    pistoris::setLogCallback(nullptr, nullptr);
    REQUIRE(result.error() != nullptr);
    REQUIRE(result.error()->location().has_value());
    CHECK(result.error()->location()->element == pistoris::ModelElement::kResource);
    CHECK(result.error()->location()->resource_path == "graph/obj3d/interactive/items/example/example.ftl");
    CHECK(capture.errors == 0);
    CHECK(capture.debug == 0);
  }

  TEST_CASE("Resource validation exception failures retain their supplied location") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    const pistoris::ModelLocation location = pistoris::api_detail::resourceLocation(pistoris::ModelElement::kResource);
    const auto result = pistoris::api_detail::resourceValidationBoundary<pistoris::ModelResult<void>>(
        "graph/obj3d/interactive/items/example/example.ftl",
        []() -> ArxReturnCode { throw std::runtime_error("failure"); },
        location);

    pistoris::setLogCallback(nullptr, nullptr);
    REQUIRE(result.error() != nullptr);
    REQUIRE(result.error()->location().has_value());
    CHECK(result.error()->location()->element == pistoris::ModelElement::kResource);
    CHECK(result.error()->location()->resource_path == "graph/obj3d/interactive/items/example/example.ftl");
    CHECK(capture.errors == 0);
    CHECK(capture.debug == 1);
  }

  TEST_CASE("ARX_OK failure normalization reports its call site at debug level") {
    LogCapture capture;
    pistoris::setLogCallback(captureLog, &capture);

    const pistoris::ModelLocation location = pistoris::api_detail::resourceLocation(pistoris::ModelElement::kResource);
    const pistoris::ModelResult<void> result = pistoris::api_detail::modelFailure<void>(ARX_OK, location);

    pistoris::setLogCallback(nullptr, nullptr);
    CHECK_FALSE(result);
    CHECK(result.code() == ARX_INTERNAL_ERROR);
    CHECK(capture.errors == 0);
    CHECK(capture.debug == 2);
    REQUIRE(capture.messages.size() == 2);
    CHECK(capture.messages.front().find("ARX_OK passed to failure construction") != std::string::npos);
    CHECK(capture.messages.front().find("status_boundary_tests.cpp") != std::string::npos);
  }
}
