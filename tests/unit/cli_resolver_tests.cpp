// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/arx_math.h"
#include "arx_pistoris/level.hpp"

#include "../../cli/modules/implication.h"
#include "../../cli/pipeline/execution_context.h"
#include "../../cli/pipeline/parsed.h"
#include "../../cli/routes/descriptor.h"
#include "../../cli/routes/level/options.h"
#include "../../cli/routes/level/state.h"
#include "formats/format.h"
#include "modules/module.h"

#include <array>
#include <cstdlib>
#include <initializer_list>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

template <typename T>
concept HasConversion = requires(T value) { value.conversion; };

template <typename T>
concept HasFormat = requires(T value) { value.format; };

template <typename T>
concept HasNativeOutput = requires(T value) { value.native_output; };

template <typename T>
concept HasOutputConverter = requires(T value) { value.output_converter; };

template <typename T>
concept HasRouteKind = requires(T value) { value.route_kind; };

template <typename T>
concept HasRouteConstraints = requires(T value) { value.route_constraints; };

template <typename T>
concept HasInputs = requires(T value) { value.inputs; };

template <typename T>
concept HasOutput = requires(T value) { value.output; };

template <typename T>
concept HasModules = requires(T value) { value.modules; };

template <typename T>
concept HasModuleQueries = requires(const T& value, const cli::Module& module) {
  value.hasModuleCategory(cli::ModuleCategory::kRoute);
  value.hasModule(module);
};

namespace {

const cli::Module& testRadiusModule();
const cli::Module& testMountModule();
const cli::Module& implyRadius30A();
const cli::Module& implyRadius30B();
const cli::Module& implyRadius40();
const cli::Module& implyInvalidRadius();
const cli::Module& implyMounts();
const cli::Module& nestedRoot();
const cli::Module& nestedMiddle();
const cli::Module& cycleA();
const cli::Module& cycleB();

class TestModule final : public cli::SystemModule {
 public:
  explicit TestModule(const char* keyword, std::span<const cli::ModuleImplication> implications = {})
      : keywords_{keyword}, implications_(implications) {}

  std::span<const char* const> keywords() const noexcept override { return keywords_; }
  cli::ModuleHelp help(const cli::RouteDescriptor*) const noexcept override {
    return {cli::HelpSection::kOptions, keywords_[0], "Test module"};
  }
  std::span<const cli::ModuleImplication> implications() const noexcept override { return implications_; }
  cli::ModuleParseResult parse(cli::ModuleParseContext&) const override { return {}; }

 private:
  std::array<const char*, 1> keywords_;
  std::span<const cli::ModuleImplication> implications_;
};

class TestRadiusModule final : public cli::SystemModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--test-radius"};
    return kKeywords;
  }
  cli::ModuleHelp help(const cli::RouteDescriptor*) const noexcept override {
    return {cli::HelpSection::kOptions, "--test-radius <VALUE>", "Test radius"};
  }
  cli::ModuleParseResult parse(cli::ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) return {.ok = false};
    char* end = nullptr;
    float value = std::strtof(ctx.argv[++ctx.index], &end);
    if (end == ctx.argv[ctx.index] || *end != '\0') return {.ok = false};
    ctx.options.format_modifiers.glb.arx_units_per_unit = value;
    return {};
  }
};

class TestMountModule final : public cli::SystemModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--test-mount"};
    return kKeywords;
  }
  cli::ModuleHelp help(const cli::RouteDescriptor*) const noexcept override {
    return {cli::HelpSection::kOptions, "--test-mount <PATH>", "Test mount"};
  }
  bool repeatable() const noexcept override { return true; }
  cli::ModuleParseResult parse(cli::ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) return {.ok = false};
    ctx.options.mounts.emplace_back(ctx.argv[++ctx.index]);
    return {};
  }
};

bool radius30Arguments(std::span<const std::string>, std::vector<std::string>& out) {
  out.emplace_back("30");
  return true;
}

bool radius40Arguments(std::span<const std::string>, std::vector<std::string>& out) {
  out.emplace_back("40");
  return true;
}

bool invalidRadiusArguments(std::span<const std::string>, std::vector<std::string>&) { return false; }

bool mount2Arguments(std::span<const std::string>, std::vector<std::string>& out) {
  out.emplace_back("path2/");
  return true;
}

bool mount3Arguments(std::span<const std::string>, std::vector<std::string>& out) {
  out.emplace_back("path3/");
  return true;
}

const cli::Module& testRadiusModule() {
  static const TestRadiusModule kModule;
  return kModule;
}

const cli::Module& testMountModule() {
  static const TestMountModule kModule;
  return kModule;
}

const cli::Module& implyRadius30A() {
  static constexpr cli::ModuleImplication kImplications[] = {{testRadiusModule, radius30Arguments}};
  static const TestModule kModule("--test-imply-radius-30-a", kImplications);
  return kModule;
}

const cli::Module& implyRadius30B() {
  static constexpr cli::ModuleImplication kImplications[] = {{testRadiusModule, radius30Arguments}};
  static const TestModule kModule("--test-imply-radius-30-b", kImplications);
  return kModule;
}

const cli::Module& implyRadius40() {
  static constexpr cli::ModuleImplication kImplications[] = {{testRadiusModule, radius40Arguments}};
  static const TestModule kModule("--test-imply-radius-40", kImplications);
  return kModule;
}

const cli::Module& implyInvalidRadius() {
  static constexpr cli::ModuleImplication kImplications[] = {{testRadiusModule, invalidRadiusArguments}};
  static const TestModule kModule("--test-imply-invalid-radius", kImplications);
  return kModule;
}

const cli::Module& implyMounts() {
  static constexpr cli::ModuleImplication kImplications[] = {
      {testMountModule, mount2Arguments},
      {testMountModule, mount3Arguments},
  };
  static const TestModule kModule("--test-imply-mounts", kImplications);
  return kModule;
}

const cli::Module& nestedMiddle() {
  static constexpr cli::ModuleImplication kImplications[] = {{testRadiusModule, radius30Arguments}};
  static const TestModule kModule("--test-nested-middle", kImplications);
  return kModule;
}

const cli::Module& nestedRoot() {
  static constexpr cli::ModuleImplication kImplications[] = {{nestedMiddle, nullptr}};
  static const TestModule kModule("--test-nested-root", kImplications);
  return kModule;
}

const cli::Module& cycleA() {
  static constexpr cli::ModuleImplication kImplications[] = {{cycleB, nullptr}};
  static const TestModule kModule("--test-cycle-a", kImplications);
  return kModule;
}

const cli::Module& cycleB() {
  static constexpr cli::ModuleImplication kImplications[] = {{cycleA, nullptr}};
  static const TestModule kModule("--test-cycle-b", kImplications);
  return kModule;
}

cli::ModuleInvocation invoke(const cli::Module& module, std::initializer_list<const char*> arguments = {}) {
  cli::ModuleInvocation invocation;
  invocation.module = &module;
  for (const char* argument : arguments) invocation.arguments.emplace_back(argument);
  return invocation;
}

std::size_t invocationCount(std::span<const cli::ModuleInvocation> invocations, const cli::Module& module) {
  std::size_t count = 0;
  for (const cli::ModuleInvocation& invocation : invocations)
    if (invocation.module == &module) ++count;
  return count;
}

}  // namespace

TEST_SUITE("cli_resolver") {
  TEST_CASE("FormatMasksUseOrdinaryBitSetSemantics") {
    static_assert(cli::kNoFormats == 0);
    static_assert((cli::kAllFormats & cli::formatBit(cli::Format::kGlb)) != 0);
  }

  TEST_CASE("ExecutionContextKeepsRouteSpecificStateOutOfSharedEnvelope") {
    static_assert(!HasRouteKind<cli::ExecutionContext>);
    static_assert(!HasConversion<cli::ExecutionContext>);
    static_assert(!HasFormat<cli::ExecutionContext>);
    static_assert(!HasNativeOutput<cli::ExecutionContext>);
    static_assert(!HasOutputConverter<cli::ExecutionContext>);
    static_assert(!HasModules<cli::ExecutionContext>);
    static_assert(HasModuleQueries<cli::ExecutionContext>);
  }

  TEST_CASE("LevelRouteStateCannotCopyOrMoveItsLevel") {
    static_assert(!std::is_copy_constructible_v<cli::level::LevelInput>);
    static_assert(!std::is_copy_assignable_v<cli::level::LevelInput>);
    static_assert(!std::is_move_constructible_v<cli::level::LevelInput>);
    static_assert(!std::is_move_assignable_v<cli::level::LevelInput>);
  }

  TEST_CASE("RouteProbeReceivesResolvedConstraints") { static_assert(HasRouteConstraints<cli::RouteProbeContext>); }

  TEST_CASE("ParsedCliOwnsSplitPositionals") {
    static_assert(HasInputs<cli::ParsedCli>);
    static_assert(HasOutput<cli::ParsedCli>);
    static_assert(!HasInputs<cli::ParsedOptions>);
    static_assert(!HasOutput<cli::ParsedOptions>);
    static_assert(!HasOutputConverter<cli::ParsedOptions>);
  }

  TEST_CASE("AnchorGenerationOptionsDeriveSpacingAndLinkDistance") {
    cli::level::LevelOptions options;
    options.anchor_generation.radius = 80.0f;

    pistoris::Level::AnchorGenOptions generation = cli::level::effectiveAnchorGenerationOptions(options);
    pistoris::Level::AnchorConnectionGenOptions connection =
        cli::level::effectiveAnchorConnectionOptions(options, generation);

    CHECK(generation.radius == doctest::Approx(80.0f));
    CHECK(generation.sample_spacing == doctest::Approx(160.0f));
    CHECK(connection.max_distance == doctest::Approx(240.0f));
    CHECK(connection.radius_scale == doctest::Approx(0.9f));
  }

  TEST_CASE("LevelAppliesExplicitGlbFormatModifiersOverItsDefaults") {
    cli::level::LevelOptions options;
    cli::FormatModifierOptions modifiers;
    modifiers.glb.arx_units_per_unit = 25.0f;
    modifiers.glb.arx_offset = pistoris::ArxVector3{10.0f, 20.0f, 30.0f};

    cli::level::applyFormatModifiers(options, modifiers);

    CHECK(options.glb_import.arx_units_per_glb_unit == doctest::Approx(25.0f));
    CHECK(options.glb_export.arx_units_per_glb_unit == doctest::Approx(25.0f));
    REQUIRE(options.glb_import.arx_offset.has_value());
    CHECK(options.glb_import.arx_offset->x == doctest::Approx(10.0f));
    CHECK(options.glb_import.arx_offset->y == doctest::Approx(20.0f));
    CHECK(options.glb_import.arx_offset->z == doctest::Approx(30.0f));
    CHECK(options.glb_export.arx_offset.x == doctest::Approx(10.0f));
    CHECK(options.glb_export.arx_offset.y == doctest::Approx(20.0f));
    CHECK(options.glb_export.arx_offset.z == doctest::Approx(30.0f));
  }

  TEST_CASE("ExplicitAnchorSpacingStopsSpacingPropagationButStillFeedsLinkDistance") {
    cli::level::LevelOptions options;
    options.anchor_generation.radius = 80.0f;
    options.anchor_generation.sample_spacing = 100.0f;
    options.anchor_spacing_specified = true;

    pistoris::Level::AnchorGenOptions generation = cli::level::effectiveAnchorGenerationOptions(options);
    pistoris::Level::AnchorConnectionGenOptions connection =
        cli::level::effectiveAnchorConnectionOptions(options, generation);

    CHECK(generation.radius == doctest::Approx(80.0f));
    CHECK(generation.sample_spacing == doctest::Approx(100.0f));
    CHECK(connection.max_distance == doctest::Approx(150.0f));
  }

  TEST_CASE("ExplicitAnchorLinkDistanceStopsLinkDistancePropagation") {
    cli::level::LevelOptions options;
    options.anchor_generation.radius = 80.0f;
    options.anchor_connection.max_distance = 180.0f;
    options.anchor_link_distance_specified = true;

    pistoris::Level::AnchorGenOptions generation = cli::level::effectiveAnchorGenerationOptions(options);
    pistoris::Level::AnchorConnectionGenOptions connection =
        cli::level::effectiveAnchorConnectionOptions(options, generation);

    CHECK(generation.sample_spacing == doctest::Approx(160.0f));
    CHECK(connection.max_distance == doctest::Approx(180.0f));
  }

  TEST_CASE("ExplicitSingletonOverridesConflictingImplications") {
    const std::vector<cli::ModuleInvocation> explicit_modules = {
        invoke(implyRadius30A()), invoke(implyRadius40()), invoke(testRadiusModule(), {"55"})};
    cli::ParsedOptions options;
    std::vector<cli::ModuleInvocation> effective;

    REQUIRE(cli::resolveEffectiveModules(explicit_modules, options, effective));
    REQUIRE(options.format_modifiers.glb.arx_units_per_unit.has_value());
    CHECK(*options.format_modifiers.glb.arx_units_per_unit == doctest::Approx(55.0f));
    CHECK(invocationCount(effective, testRadiusModule()) == 1);
    CHECK(effective.back().module == &testRadiusModule());
    CHECK(effective.back().origin == cli::ModuleOrigin::kExplicit);
  }

  TEST_CASE("ExplicitSingletonSuppressesItsImplicationBuilder") {
    const std::vector<cli::ModuleInvocation> explicit_modules = {invoke(implyInvalidRadius()),
                                                                 invoke(testRadiusModule(), {"55"})};
    cli::ParsedOptions options;
    std::vector<cli::ModuleInvocation> effective;

    REQUIRE(cli::resolveEffectiveModules(explicit_modules, options, effective));
    REQUIRE(options.format_modifiers.glb.arx_units_per_unit.has_value());
    CHECK(*options.format_modifiers.glb.arx_units_per_unit == doctest::Approx(55.0f));
    CHECK(invocationCount(effective, testRadiusModule()) == 1);
  }

  TEST_CASE("UnsuppressedImplicationBuilderFailureRejectsResolution") {
    const std::vector<cli::ModuleInvocation> explicit_modules = {invoke(implyInvalidRadius())};
    cli::ParsedOptions options;
    std::vector<cli::ModuleInvocation> effective;

    CHECK_FALSE(cli::resolveEffectiveModules(explicit_modules, options, effective));
    CHECK(effective.empty());
  }

  TEST_CASE("ConflictingImpliedSingletonValuesFail") {
    const std::vector<cli::ModuleInvocation> explicit_modules = {invoke(implyRadius30A()), invoke(implyRadius40())};
    cli::ParsedOptions options;
    std::vector<cli::ModuleInvocation> effective;

    CHECK_FALSE(cli::resolveEffectiveModules(explicit_modules, options, effective));
    CHECK(effective.empty());
  }

  TEST_CASE("IdenticalImpliedSingletonValuesCoalesce") {
    const std::vector<cli::ModuleInvocation> explicit_modules = {invoke(implyRadius30A()), invoke(implyRadius30B())};
    cli::ParsedOptions options;
    std::vector<cli::ModuleInvocation> effective;

    REQUIRE(cli::resolveEffectiveModules(explicit_modules, options, effective));
    REQUIRE(options.format_modifiers.glb.arx_units_per_unit.has_value());
    CHECK(*options.format_modifiers.glb.arx_units_per_unit == doctest::Approx(30.0f));
    CHECK(invocationCount(effective, testRadiusModule()) == 1);
  }

  TEST_CASE("RepeatableImplicationsExpandInline") {
    const std::vector<cli::ModuleInvocation> explicit_modules = {
        invoke(testMountModule(), {"path1/"}), invoke(implyMounts()), invoke(testMountModule(), {"path4/"})};
    cli::ParsedOptions options;
    std::vector<cli::ModuleInvocation> effective;

    REQUIRE(cli::resolveEffectiveModules(explicit_modules, options, effective));
    REQUIRE(options.mounts.size() == 4);
    CHECK(options.mounts[0] == "path1/");
    CHECK(options.mounts[1] == "path2/");
    CHECK(options.mounts[2] == "path3/");
    CHECK(options.mounts[3] == "path4/");
  }

  TEST_CASE("ImplicationsExpandTransitively") {
    const std::vector<cli::ModuleInvocation> explicit_modules = {invoke(nestedRoot())};
    cli::ParsedOptions options;
    std::vector<cli::ModuleInvocation> effective;

    REQUIRE(cli::resolveEffectiveModules(explicit_modules, options, effective));
    REQUIRE(effective.size() == 3);
    CHECK(effective[0].module == &nestedRoot());
    CHECK(effective[1].module == &nestedMiddle());
    CHECK(effective[2].module == &testRadiusModule());
    REQUIRE(options.format_modifiers.glb.arx_units_per_unit.has_value());
    CHECK(*options.format_modifiers.glb.arx_units_per_unit == doctest::Approx(30.0f));
  }

  TEST_CASE("ImplicationCyclesFailWithoutPublishingPartialState") {
    const std::vector<cli::ModuleInvocation> explicit_modules = {invoke(cycleA())};
    cli::ParsedOptions options;
    options.mounts.emplace_back("preserved");
    std::vector<cli::ModuleInvocation> effective = {invoke(testMountModule(), {"preserved"})};

    CHECK_FALSE(cli::resolveEffectiveModules(explicit_modules, options, effective));
    REQUIRE(options.mounts.size() == 1);
    CHECK(options.mounts[0] == "preserved");
    REQUIRE(effective.size() == 1);
    CHECK(effective[0].module == &testMountModule());
  }
}
