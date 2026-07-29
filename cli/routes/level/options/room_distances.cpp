// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/level.hpp"

#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/parsing.h"
#include "routes/level/options.h"
#include "routes/level/options/modules.h"

#include <cmath>
#include <span>

namespace cli::level::options {
namespace {

class GenerateRoomDistancesModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--generate-room-distances"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--generate-room-distances",
            "Generate Level room-distance data from portals and static geometry."};
  }

  std::span<const ModuleRef> children() const noexcept override {
    static constexpr ModuleRef kModules[] = {
        roomDistanceSpacingModule,
        roomDistanceOffsetModule,
        roomDistanceHeightModule,
        roomDistanceLinkDistanceModule,
    };
    return kModules;
  }

  std::span<const ModuleRef> incompatibleWith() const noexcept override {
    static constexpr ModuleRef kModules[] = {debugCellsModule};
    return kModules;
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    ctx.routeOptions<LevelOptions>().generate_room_distances = true;
    return {};
  }

  bool validate(const ModuleValidationContext& context) const override {
    const pistoris::Level::RoomDistanceGenOptions& generation =
        context.routeOptions<LevelOptions>().room_distance_generation;
    if (!std::isfinite(generation.sample_spacing) ||
        generation.sample_spacing < pistoris::kMinRoomDistanceSampleSpacing) {
      diagnostic(DiagnosticCode::kInvalidModuleValue,
                 "--rdist-spacing must be at least %.3g",
                 static_cast<double>(pistoris::kMinRoomDistanceSampleSpacing));
      return false;
    }
    if (!std::isfinite(generation.portal_side_offset) || generation.portal_side_offset <= 0.0f ||
        generation.portal_side_offset > pistoris::kMaxRoomDistancePortalOffset) {
      diagnostic(DiagnosticCode::kInvalidModuleValue,
                 "--rdist-offset must be inside (0, %.3g]",
                 static_cast<double>(pistoris::kMaxRoomDistancePortalOffset));
      return false;
    }
    if (!std::isfinite(generation.sample_height_offset) ||
        generation.sample_height_offset < pistoris::kMinRoomDistanceSampleHeight) {
      diagnostic(DiagnosticCode::kInvalidModuleValue,
                 "--rdist-height must be at least %.3g",
                 static_cast<double>(pistoris::kMinRoomDistanceSampleHeight));
      return false;
    }

    const float minimum_link_distance = generation.sample_spacing * pistoris::kRoomDistanceMinLinkDistanceSpacingFactor;
    if (!std::isfinite(generation.max_link_distance) ||
        (context.routeOptions<LevelOptions>().room_distance_link_distance_specified &&
         generation.max_link_distance < minimum_link_distance) ||
        (!context.routeOptions<LevelOptions>().room_distance_link_distance_specified &&
         generation.max_link_distance < 0.0f)) {
      diagnostic(DiagnosticCode::kInvalidModuleValue,
                 "--rdist-link-distance must be at least %.3g when specified",
                 static_cast<double>(minimum_link_distance));
      return false;
    }
    return true;
  }
};

class RoomDistanceSpacingModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--rdist-spacing"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--rdist-spacing <UNITS=100>", "Set room-distance graph sample spacing."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return {modules::consumeFloat(
        ctx, ctx.routeOptions<LevelOptions>().room_distance_generation.sample_spacing, stableName(), "numeric units")};
  }
};

class RoomDistanceOffsetModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--rdist-offset"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--rdist-offset <UNITS=10>", "Set room-distance portal access offset."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return {modules::consumeFloat(ctx,
                                  ctx.routeOptions<LevelOptions>().room_distance_generation.portal_side_offset,
                                  stableName(),
                                  "numeric units")};
  }
};

class RoomDistanceHeightModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--rdist-height"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--rdist-height <UNITS=82.5>", "Set room-distance sample height offset."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    return {modules::consumeFloat(ctx,
                                  ctx.routeOptions<LevelOptions>().room_distance_generation.sample_height_offset,
                                  stableName(),
                                  "numeric units")};
  }
};

class RoomDistanceLinkDistanceModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--rdist-link-distance"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--rdist-link-distance <UNITS=1.5*spacing>",
            "Set room-distance visibility graph link distance."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (!modules::consumeFloat(ctx,
                               ctx.routeOptions<LevelOptions>().room_distance_generation.max_link_distance,
                               stableName(),
                               "numeric units"))
      return {.ok = false};
    ctx.routeOptions<LevelOptions>().room_distance_link_distance_specified = true;
    return {};
  }
};

}  // namespace

const Module& generateRoomDistancesModule() { return moduleInstance<GenerateRoomDistancesModule>(); }

const Module& roomDistanceSpacingModule() { return moduleInstance<RoomDistanceSpacingModule>(); }

const Module& roomDistanceOffsetModule() { return moduleInstance<RoomDistanceOffsetModule>(); }

const Module& roomDistanceHeightModule() { return moduleInstance<RoomDistanceHeightModule>(); }

const Module& roomDistanceLinkDistanceModule() { return moduleInstance<RoomDistanceLinkDistanceModule>(); }

}  // namespace cli::level::options
