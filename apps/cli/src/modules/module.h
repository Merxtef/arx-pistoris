// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "formats/format.h"
#include "routes/options.h"
#include "routes/types.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace cli {

class IoService;
struct ParsedOptions;

enum class ModuleCategory : std::uint8_t {
  kSystem,
  kTerminalAction,
  kRoute,
  kSharedConversion,
  kOutputFormat,
  kOutputConverter,
  kFormatModifier,
  kNativeBakeModifier,
  kInputModifier,
};

enum class HelpSection : std::uint8_t {
  kOptions,
  kDebug,
};

enum class SingletonCategory : std::uint8_t {
  kNone,
  kTerminalAction,
  kOutputConverter,
};

enum class ModuleOrigin : std::uint8_t {
  kExplicit,
  kImplied,
};

using RouteMask = std::uint8_t;
using FormatMask = std::uint32_t;
constexpr RouteMask kNoRoutes = 0;
constexpr FormatMask kNoFormats = 0;
constexpr FormatMask kAllFormats = ~FormatMask{0};

constexpr RouteMask routeBit(RouteKind kind) {
  switch (kind) {
    case RouteKind::kModel:
      return 1u << 0;
    case RouteKind::kAnimation:
      return 1u << 1;
    case RouteKind::kAmbiance:
      return 1u << 2;
    case RouteKind::kCinematic:
      return 1u << 3;
    case RouteKind::kLevel:
      return 1u << 4;
    default:
      return 0;
  }
}

constexpr FormatMask formatBit(Format format) { return 1u << static_cast<unsigned>(format); }

struct ModuleParseResult {
  bool ok = true;
  bool stop = false;
};

struct ModuleParseContext {
  ParsedOptions& options;
  RouteOptions* route_options;
  int argc;
  const char* const* argv;
  int& index;

  template <typename T>
  T& routeOptions() const {
    return static_cast<T&>(*route_options);
  }
};

struct ModuleValidationContext {
  const ParsedOptions& options;
  const RouteOptions* route_options;

  template <typename T>
  const T& routeOptions() const {
    return static_cast<const T&>(*route_options);
  }
};

struct ModuleHelp {
  HelpSection section = HelpSection::kOptions;
  const char* usage = nullptr;
  const char* description = nullptr;
};

class Module;
struct RouteDescriptor;
using ModuleRef = const Module& (*)();

using ModuleArgumentBuilder = bool (*)(std::span<const std::string> source_arguments,
                                       std::vector<std::string>& out_arguments);

struct ModuleImplication {
  ModuleRef target = nullptr;
  ModuleArgumentBuilder arguments = nullptr;
};

class SystemModule;
class TerminalActionModule;
class RouteModule;
class SharedConversionModule;
template <FormatMask Outputs>
class OutputFormatModule;
template <FormatMask Outputs, Format Requested>
class OutputConverterModule;
template <FormatMask Formats>
class FormatModifierModule;
template <FormatMask Outputs>
class NativeBakeModifierModule;
class InputModifierModule;

class Module {
 public:
  virtual ~Module() = default;

  Module(const Module&) = delete;
  Module& operator=(const Module&) = delete;
  Module(Module&&) = delete;
  Module& operator=(Module&&) = delete;

  const char* stableName() const noexcept;
  virtual std::span<const char* const> keywords() const noexcept = 0;
  virtual ModuleCategory category() const noexcept = 0;
  virtual FormatMask outputFormats() const noexcept = 0;
  virtual FormatMask modifiedFormats() const noexcept { return kNoFormats; }
  virtual Format requestedOutputFormat() const noexcept = 0;
  virtual SingletonCategory singleton() const noexcept = 0;
  virtual ModuleHelp help(const RouteDescriptor* route) const = 0;

  virtual bool repeatable() const noexcept;
  virtual std::span<const ModuleRef> children() const noexcept;
  virtual std::span<const ModuleRef> dependencies() const noexcept;
  virtual std::span<const ModuleRef> incompatibleWith() const noexcept;
  virtual std::span<const ModuleImplication> implications() const noexcept;
  virtual ModuleParseResult parse(ModuleParseContext& ctx) const = 0;
  virtual bool validate(const ModuleValidationContext& context) const;

 private:
  Module() = default;

  friend class SystemModule;
  friend class TerminalActionModule;
  friend class RouteModule;
  friend class SharedConversionModule;
  template <FormatMask Outputs>
  friend class OutputFormatModule;
  template <FormatMask Outputs, Format Requested>
  friend class OutputConverterModule;
  template <FormatMask Formats>
  friend class FormatModifierModule;
  template <FormatMask Outputs>
  friend class NativeBakeModifierModule;
  friend class InputModifierModule;
};

class SystemModule : public Module {
 public:
  ModuleCategory category() const noexcept final { return ModuleCategory::kSystem; }
  FormatMask outputFormats() const noexcept final { return kAllFormats; }
  Format requestedOutputFormat() const noexcept final { return Format::kUnknown; }
  SingletonCategory singleton() const noexcept final { return SingletonCategory::kNone; }

 protected:
  SystemModule() = default;
};

class TerminalActionModule : public Module {
 public:
  ModuleCategory category() const noexcept final { return ModuleCategory::kTerminalAction; }
  FormatMask outputFormats() const noexcept final { return kAllFormats; }
  Format requestedOutputFormat() const noexcept final { return Format::kUnknown; }
  SingletonCategory singleton() const noexcept final { return SingletonCategory::kTerminalAction; }
  virtual int execute(const ParsedOptions& options, IoService& io) const = 0;

 protected:
  TerminalActionModule() = default;
};

class RouteModule : public Module {
 public:
  ModuleCategory category() const noexcept final { return ModuleCategory::kRoute; }
  FormatMask outputFormats() const noexcept final { return kAllFormats; }
  Format requestedOutputFormat() const noexcept final { return Format::kUnknown; }
  SingletonCategory singleton() const noexcept final { return SingletonCategory::kNone; }

 protected:
  RouteModule() = default;
};

class SharedConversionModule : public Module {
 public:
  ModuleCategory category() const noexcept final { return ModuleCategory::kSharedConversion; }
  FormatMask outputFormats() const noexcept final { return kAllFormats; }
  Format requestedOutputFormat() const noexcept final { return Format::kUnknown; }
  SingletonCategory singleton() const noexcept final { return SingletonCategory::kNone; }

 protected:
  SharedConversionModule() = default;
};

template <FormatMask Outputs>
class OutputFormatModule : public Module {
  static_assert(Outputs != kNoFormats && Outputs != kAllFormats);

 public:
  ModuleCategory category() const noexcept final { return ModuleCategory::kOutputFormat; }
  FormatMask outputFormats() const noexcept final { return Outputs; }
  Format requestedOutputFormat() const noexcept final { return Format::kUnknown; }
  SingletonCategory singleton() const noexcept final { return SingletonCategory::kNone; }

 protected:
  OutputFormatModule() = default;
};

template <FormatMask Outputs, Format Requested>
class OutputConverterModule : public Module {
  static_assert(Outputs != kNoFormats && Outputs != kAllFormats);
  static_assert(Requested == Format::kUnknown || (Outputs & formatBit(Requested)) != 0);

 public:
  ModuleCategory category() const noexcept final { return ModuleCategory::kOutputConverter; }
  FormatMask outputFormats() const noexcept final { return Outputs; }
  Format requestedOutputFormat() const noexcept final { return Requested; }
  SingletonCategory singleton() const noexcept final { return SingletonCategory::kOutputConverter; }

 protected:
  OutputConverterModule() = default;
};

template <FormatMask Formats>
class FormatModifierModule : public Module {
  static_assert(Formats != kNoFormats);

 public:
  ModuleCategory category() const noexcept final { return ModuleCategory::kFormatModifier; }
  FormatMask outputFormats() const noexcept final { return kAllFormats; }
  FormatMask modifiedFormats() const noexcept final { return Formats; }
  Format requestedOutputFormat() const noexcept final { return Format::kUnknown; }
  SingletonCategory singleton() const noexcept final { return SingletonCategory::kNone; }

 protected:
  FormatModifierModule() = default;
};

template <FormatMask Outputs>
class NativeBakeModifierModule : public Module {
  static_assert(Outputs != kNoFormats && Outputs != kAllFormats);

 public:
  ModuleCategory category() const noexcept final { return ModuleCategory::kNativeBakeModifier; }
  FormatMask outputFormats() const noexcept final { return Outputs; }
  Format requestedOutputFormat() const noexcept final { return Format::kUnknown; }
  SingletonCategory singleton() const noexcept final { return SingletonCategory::kNone; }

 protected:
  NativeBakeModifierModule() = default;
};

class InputModifierModule : public Module {
 public:
  ModuleCategory category() const noexcept final { return ModuleCategory::kInputModifier; }
  FormatMask outputFormats() const noexcept final { return kAllFormats; }
  Format requestedOutputFormat() const noexcept final { return Format::kUnknown; }
  SingletonCategory singleton() const noexcept final { return SingletonCategory::kNone; }

 protected:
  InputModifierModule() = default;
};

template <typename T>
const Module& moduleInstance() {
  static const T kModule;
  return kModule;
}

struct ModuleInvocation {
  const Module* module = nullptr;
  const RouteDescriptor* options_route = nullptr;
  ModuleOrigin origin = ModuleOrigin::kExplicit;
  const Module* implied_by = nullptr;
  std::vector<std::string> arguments;
};

}  // namespace cli
