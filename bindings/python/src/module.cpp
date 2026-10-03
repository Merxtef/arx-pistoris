// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "bindings.h"

#include <exception>
#include <stdexcept>
#include <string>

namespace {

template <class Bind>
void bindSection(const char* name, Bind bind, nanobind::module_& module) {
  try {
    bind(module);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("failed to initialize ") + name + " bindings: " + error.what());
  }
}

}  // namespace

NB_MODULE(_core, module) {
  module.doc() = "Pistoris Python bindings";
  bindSection("common", pistoris::python::bindCommon, module);
  bindSection("paths", pistoris::python::bindPaths, module);
  bindSection("native", pistoris::python::bindNative, module);
  bindSection("resource", pistoris::python::bindResources, module);
}
