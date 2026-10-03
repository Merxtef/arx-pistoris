# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

set(PYTHON_PACKAGE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/python/pistoris")
set(PYTHON_PACKAGE_BUILD "${PROJECT_BINARY_DIR}/python/pistoris")
file(REMOVE_RECURSE "${PYTHON_PACKAGE_BUILD}")
if(EXISTS "${PYTHON_PACKAGE_BUILD}")
  message(FATAL_ERROR
    "Could not reset the generated Python package at ${PYTHON_PACKAGE_BUILD}. "
    "Close any Python interpreter using this build and configure again."
  )
endif()
file(MAKE_DIRECTORY "${PYTHON_PACKAGE_BUILD}/native")

nanobind_add_module(pistoris_python
  NB_STATIC
  STABLE_ABI
  src/ambiance_cinematic.cpp
  src/common.cpp
  src/level.cpp
  src/module.cpp
  src/model_animation.cpp
  src/native.cpp
  src/paths.cpp
  src/resource_values.cpp
  src/resources.cpp
)
arx_target_vendored(nanobind-static-abi3)
set_target_properties(pistoris_python PROPERTIES
  OUTPUT_NAME _core
  LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/python/pistoris"
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/python/pistoris"
)
target_link_libraries(pistoris_python PRIVATE arx_pistoris_cpp)
target_include_directories(pistoris_python PRIVATE "${PROJECT_SOURCE_DIR}/libs/core/src")
target_include_directories(pistoris_python SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/third_party/nanobind/include")
target_compile_features(pistoris_python PRIVATE cxx_std_20)
arx_target_defaults(pistoris_python)

file(GLOB PYTHON_PUBLIC_MODULES CONFIGURE_DEPENDS
  "${PYTHON_PACKAGE_SOURCE}/*.py"
  "${PYTHON_PACKAGE_SOURCE}/*.pyi"
)
foreach(module IN LISTS PYTHON_PUBLIC_MODULES)
  get_filename_component(module_name "${module}" NAME)
  configure_file("${module}" "${PYTHON_PACKAGE_BUILD}/${module_name}" COPYONLY)
endforeach()
configure_file("${PYTHON_PACKAGE_SOURCE}/py.typed" "${PYTHON_PACKAGE_BUILD}/py.typed" COPYONLY)
configure_file("${PYTHON_PACKAGE_SOURCE}/native/__init__.py" "${PYTHON_PACKAGE_BUILD}/native/__init__.py" COPYONLY)
file(GLOB PYTHON_PUBLIC_NATIVE_STUBS CONFIGURE_DEPENDS "${PYTHON_PACKAGE_SOURCE}/native/*.pyi")
foreach(stub IN LISTS PYTHON_PUBLIC_NATIVE_STUBS)
  get_filename_component(stub_name "${stub}" NAME)
  configure_file("${stub}" "${PYTHON_PACKAGE_BUILD}/native/${stub_name}" COPYONLY)
endforeach()

set(PYTHON_PRIVATE_STUB_DIR "${PYTHON_PACKAGE_BUILD}/_core")
set(PYTHON_STUB_MARKER "${PYTHON_PACKAGE_BUILD}/_core-stubs.marker")
nanobind_add_stub(pistoris_python_core_stub
  MODULE pistoris._core
  RECURSIVE
  OUTPUT_PATH "${PYTHON_PACKAGE_BUILD}"
  MARKER_FILE "${PYTHON_STUB_MARKER}"
  PYTHON_PATH "${PROJECT_BINARY_DIR}/python"
  PATTERN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/stubgen.pattern"
  DEPENDS pistoris_python "${CMAKE_CURRENT_SOURCE_DIR}/normalize_stubs.py"
)
add_custom_command(TARGET pistoris_python_core_stub POST_BUILD
  COMMAND ${Python_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/normalize_stubs.py"
    "${PYTHON_PRIVATE_STUB_DIR}"
  VERBATIM
)

add_custom_target(pistoris_python_package ALL
  DEPENDS pistoris_python pistoris_python_core_stub
)

install(TARGETS pistoris_python
  LIBRARY DESTINATION pistoris COMPONENT python
  RUNTIME DESTINATION pistoris COMPONENT python
)
install(DIRECTORY "${PYTHON_PACKAGE_SOURCE}/"
  DESTINATION pistoris
  COMPONENT python
  FILES_MATCHING PATTERN "*.py" PATTERN "*.pyi" PATTERN "py.typed"
)
install(DIRECTORY "${PYTHON_PRIVATE_STUB_DIR}/"
  DESTINATION pistoris/_core
  COMPONENT python
  FILES_MATCHING PATTERN "*.pyi"
)

if(BUILD_TESTING)
  add_test(NAME arx_pistoris_python_tests
    COMMAND ${CMAKE_COMMAND} -E env
      "PYTHONPATH=${PROJECT_BINARY_DIR}/python"
      ${Python_EXECUTABLE} -m unittest discover -s "${PROJECT_SOURCE_DIR}/tests/python" -v
  )
  set_tests_properties(arx_pistoris_python_tests PROPERTIES LABELS python)
endif()
