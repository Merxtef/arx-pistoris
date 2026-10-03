// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include <nanobind/nanobind.h>

namespace pistoris::python {

namespace nb = nanobind;

void bindCommon(nb::module_& module);
void bindPaths(nb::module_& module);
void bindNative(nb::module_& module);
void bindResourceValues(nb::module_& module);
void bindLevel(nb::module_& module);
void bindModelAnimation(nb::module_& module);
void bindAmbianceCinematic(nb::module_& module);
void bindResources(nb::module_& module);

}  // namespace pistoris::python
