// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "bindings.h"

#include <exception>
#include <stdexcept>
#include <string>

namespace pistoris::python {

void bindResources(nanobind::module_& module) {
  try {
    bindResourceValues(module);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("resource values: ") + error.what());
  }
  try {
    bindLevel(module);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("Level: ") + error.what());
  }
  try {
    bindModelAnimation(module);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("Model/Animation: ") + error.what());
  }
  try {
    bindAmbianceCinematic(module);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("Ambiance/Cinematic: ") + error.what());
  }
}

}  // namespace pistoris::python
