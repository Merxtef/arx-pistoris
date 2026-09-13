// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "routes/types.h"

namespace cli {

class IoService;
class ResourceOutputService;

class ExecutionContext {
 public:
  ExecutionContext(IoService& io, ResourceOutputService& resource_outputs, Route route) noexcept
      : io_(io), resource_outputs_(resource_outputs), route_(route) {}

  [[nodiscard]] IoService& io() const noexcept { return io_; }
  [[nodiscard]] ResourceOutputService& resourceOutputs() const noexcept { return resource_outputs_; }
  [[nodiscard]] const Route& route() const noexcept { return route_; }

 private:
  IoService& io_;
  ResourceOutputService& resource_outputs_;
  Route route_;
};

}  // namespace cli
