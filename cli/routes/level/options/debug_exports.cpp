// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "formats/format.h"
#include "modules/module.h"
#include "routes/level/options/modules.h"

#include <span>

namespace cli::level::options {
namespace {

class DebugCellsModule final : public OutputConverterModule<formatBit(Format::kGlb), Format::kGlb> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--debug-cells"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kDebug, "--debug-cells", "Export Level cells as debug GLB."};
  }

  ModuleParseResult parse(ModuleParseContext&) const override { return {}; }
};

class DebugNavigationModule final : public OutputConverterModule<formatBit(Format::kGlb), Format::kGlb> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--debug-navigation"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kDebug, "--debug-navigation", "Export Level navigation state as debug GLB."};
  }

  ModuleParseResult parse(ModuleParseContext&) const override { return {}; }
};

class DebugRoomDistancesModule final : public OutputConverterModule<formatBit(Format::kGlb), Format::kGlb> {
 public:
  std::span<const char* const> keywords() const noexcept override {
    static constexpr const char* kKeywords[] = {"--debug-room-distances"};
    return kKeywords;
  }

  ModuleHelp help(const RouteDescriptor*) const noexcept override {
    return {HelpSection::kDebug, "--debug-room-distances", "Export Level room-distance diagnostics as GLB."};
  }

  ModuleParseResult parse(ModuleParseContext&) const override { return {}; }
};

}  // namespace

const Module& debugCellsModule() { return moduleInstance<DebugCellsModule>(); }

const Module& debugNavigationModule() { return moduleInstance<DebugNavigationModule>(); }

const Module& debugRoomDistancesModule() { return moduleInstance<DebugRoomDistancesModule>(); }

}  // namespace cli::level::options
