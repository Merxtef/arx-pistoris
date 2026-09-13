// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/model.hpp"

#include "base/unique_prefix.h"
#include "console/diagnostics.h"
#include "modules/module.h"
#include "modules/parsing.h"
#include "routes/model/options.h"
#include "routes/model/options/modules.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace cli::model::options {
namespace {

bool parseSlot(const char* text, std::int8_t& out) {
  if (std::string_view{text} == "-") {
    out = -1;
    return true;
  }
  std::uint32_t value = 0;
  if (!modules::parseUint32(text, value) || value < 1 || value > 3) return false;
  out = static_cast<std::int8_t>(value);
  return true;
}

class InputIconModule final : public InputModifierModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--input-icon"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions, "--input-icon <PATH>", "Use this image as the Model inventory icon."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--input-icon: expected argument");
      return {.ok = false};
    }
    ctx.routeOptions<ModelOptions>().input_icon = ctx.argv[++ctx.index];
    return {};
  }
};

class IconSlotsModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--icon-slots"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--icon-slots <WIDTH> <HEIGHT>",
            "Set inventory width and height in slots; '-' derives that dimension from the image."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 2 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--icon-slots: expected width and height");
      return {.ok = false};
    }
    pistoris::Model::InventoryIconSetOptions& options = ctx.routeOptions<ModelOptions>().inventory_icon_set;
    if (!parseSlot(ctx.argv[ctx.index + 1], options.width_slots) ||
        !parseSlot(ctx.argv[ctx.index + 2], options.height_slots)) {
      diagnostic(DiagnosticCode::kInvalidModuleValue, "--icon-slots: expected '-' or a slot count inside [1, 3]");
      return {.ok = false};
    }
    ctx.index += 2;
    return {};
  }
};

class IconLayoutModule final : public RouteModule {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--icon-layout"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kOptions,
            "--icon-layout <LAYOUT=CENTER>",
            "Set content layout: center, top-left/right, bottom-left/right, or stretch."};
  }

  ModuleParseResult parse(ModuleParseContext& ctx) const override {
    if (ctx.index + 1 >= ctx.argc) {
      diagnostic(DiagnosticCode::kMissingArgument, "--icon-layout: expected a layout");
      return {.ok = false};
    }
    struct Candidate {
      std::string_view name;
      pistoris::Model::InventoryIconLayout layout;
    };
    static constexpr Candidate kCandidates[] = {
        {"center", pistoris::Model::InventoryIconLayout::kCenter},
        {"top-left", pistoris::Model::InventoryIconLayout::kTopLeft},
        {"top-right", pistoris::Model::InventoryIconLayout::kTopRight},
        {"bottom-left", pistoris::Model::InventoryIconLayout::kBottomLeft},
        {"bottom-right", pistoris::Model::InventoryIconLayout::kBottomRight},
        {"stretch", pistoris::Model::InventoryIconLayout::kStretch},
    };
    const char* value = ctx.argv[++ctx.index];
    const auto match = resolveUniquePrefix(
        value ? std::string_view(value) : std::string_view(),
        std::span<const Candidate>{kCandidates},
        [](const Candidate& candidate) { return candidate.name; },
        PrefixCase::kAsciiInsensitive);
    if (match.status == UniquePrefixStatus::kAmbiguous) {
      diagnostic(DiagnosticCode::kInvalidMode, "--icon-layout: ambiguous layout '%s'", value);
      return {.ok = false};
    }
    if (match.status == UniquePrefixStatus::kNone) {
      diagnostic(DiagnosticCode::kInvalidMode,
                 "--icon-layout: expected center, top-left, top-right, bottom-left, bottom-right, or stretch");
      return {.ok = false};
    }
    ctx.routeOptions<ModelOptions>().inventory_icon_render.layout = match.value->layout;
    return {};
  }
};

}  // namespace

const Module& inputIconModule() { return moduleInstance<InputIconModule>(); }

const Module& iconSlotsModule() { return moduleInstance<IconSlotsModule>(); }

const Module& iconLayoutModule() { return moduleInstance<IconLayoutModule>(); }

}  // namespace cli::model::options
