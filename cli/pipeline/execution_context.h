// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "formats/classification.h"
#include "formats/format.h"
#include "modules/module.h"
#include "routes/types.h"

#include <span>
#include <vector>

namespace cli {

class IoService;

class ExecutionContext {
 public:
  ExecutionContext(Format output_format, IoService& io, const std::vector<ClassifiedPath>& inputs, Route route,
                   std::span<const ModuleInvocation> modules) noexcept
      : output_format_(output_format), io_(io), inputs_(inputs), route_(route), modules_(modules) {}

  [[nodiscard]] Format outputFormat() const noexcept { return output_format_; }
  [[nodiscard]] IoService& io() const noexcept { return io_; }
  [[nodiscard]] const std::vector<ClassifiedPath>& inputs() const noexcept { return inputs_; }
  [[nodiscard]] const Route& route() const noexcept { return route_; }

  [[nodiscard]] bool hasModuleCategory(ModuleCategory category) const noexcept {
    for (const ModuleInvocation& invocation : modules_) {
      if (invocation.module && invocation.module->category() == category) return true;
    }
    return false;
  }

  [[nodiscard]] bool hasModule(const Module& module) const noexcept {
    for (const ModuleInvocation& invocation : modules_) {
      if (invocation.module == &module) return true;
    }
    return false;
  }

 private:
  Format output_format_;
  IoService& io_;
  const std::vector<ClassifiedPath>& inputs_;
  Route route_;
  std::span<const ModuleInvocation> modules_;
};

}  // namespace cli
