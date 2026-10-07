# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

add_library(arx_pistoris_resource_io STATIC
  src/catalog.cpp
  src/document.cpp
  src/native_path.cpp
  src/output.cpp
  src/resource_mounts.cpp
  src/resources.cpp
  src/status.cpp
)
set_target_properties(arx_pistoris_resource_io PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(arx_pistoris_resource_io
  PUBLIC arx_pistoris_cpp
  PRIVATE nlohmann_json
)
target_include_directories(arx_pistoris_resource_io
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${ARX_PISTORIS_CORE_SOURCE_DIR}
)
target_compile_features(arx_pistoris_resource_io PUBLIC cxx_std_20)
if(WIN32)
  target_link_libraries(arx_pistoris_resource_io PRIVATE Ole32 Shell32)
endif()
arx_target_defaults(arx_pistoris_resource_io)

add_library(arx_pistoris::resource_io ALIAS arx_pistoris_resource_io)
