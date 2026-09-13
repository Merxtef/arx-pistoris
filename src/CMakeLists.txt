# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

add_library(arx_pistoris_internal_config INTERFACE)
target_include_directories(arx_pistoris_internal_config INTERFACE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_BINARY_DIR}
)
target_compile_definitions(arx_pistoris_internal_config INTERFACE ARX_PISTORIS_ENABLE_LEVEL_DEBUG_API)

function(arx_add_internal_object_library target)
  add_library(${target} OBJECT ${ARGN})
  set_target_properties(${target} PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
  )
  target_link_libraries(${target} PRIVATE arx_pistoris_internal_config)
  arx_target_defaults(${target})
endfunction()

arx_add_internal_object_library(arx_pistoris_utils
  utils/audio.cpp
  utils/encoded_image.cpp
  utils/identifier.cpp
  utils/math/mat3.cpp
  utils/math/mat4.cpp
  utils/math/triangulation.cpp
  utils/name_tokens.cpp
  utils/pkware.cpp
  utils/portable_filename.cpp
  utils/resource_path.cpp
  utils/spatial/arx_level_grid_index.cpp
  utils/spatial/xz_point_index.cpp
  utils/text_cursor.cpp
)
target_link_libraries(arx_pistoris_utils PRIVATE mapbox_earcut arx_pistoris_audio_codecs arx_pistoris_stb
                                                   arx_pistoris_pkware)

add_subdirectory(modules)

arx_add_internal_object_library(arx_pistoris_external
  external/material_name.cpp
)

arx_add_internal_object_library(arx_pistoris_native
  native/amb.cpp
  native/dlf.cpp
  native/ftl.cpp
  native/fts.cpp
  native/lighting_layout.cpp
  native/llf.cpp
  native/resource_lookup.cpp
  native/storage.cpp
  native/tea.cpp
  native/write_metadata.cpp
)

arx_add_internal_object_library(arx_pistoris_paths
  paths/ambiance.cpp
  paths/animation.cpp
  paths/cinematic.cpp
  paths/common.cpp
  paths/entity_class.cpp
  paths/level.cpp
  paths/model.cpp
)

arx_add_internal_object_library(arx_pistoris_editing
  ambiance/ambiance.cpp
  ambiance/timing.cpp
  animation/animation.cpp
  level/images.cpp
  level/level.cpp
  level/minimap/generation.cpp
  level/navigation.cpp
  level/rooms.cpp
  level/static_lighting/generation.cpp
  level/validation.cpp
  model/generation.cpp
  model/model.cpp
  model/reference.cpp
  model/transformation.cpp
)

arx_add_internal_object_library(arx_pistoris_native_conversion
  ambiance/native.cpp
  animation/native.cpp
  level/native/bake.cpp
  level/native/build.cpp
  level/native/dlf.cpp
  level/native/fts/bake.cpp
  level/native/fts/build.cpp
  level/native/fts/quad_packing.cpp
  level/native/fts/texture_shards.cpp
  level/native/llf.cpp
  level/native/textures.cpp
  level/native.cpp
  model/native/bake.cpp
  model/native/build.cpp
)

arx_add_internal_object_library(arx_pistoris_glb
  ambiance/glb.cpp
  external/glb/accessor.cpp
  external/glb/ambiance/export.cpp
  external/glb/ambiance/import.cpp
  external/glb/container.cpp
  external/glb/geometry_material.cpp
  external/glb/level/anchor_metadata.cpp
  external/glb/level/debug/fts.cpp
  external/glb/level/coordinates.cpp
  external/glb/level/debug/common.cpp
  external/glb/level/debug/navigation.cpp
  external/glb/level/debug/render_splits.cpp
  external/glb/level/debug/room_distances.cpp
  external/glb/level/discovery.cpp
  external/glb/level/entities.cpp
  external/glb/level/export.cpp
  external/glb/level/export/geometry.cpp
  external/glb/level/export/lighting.cpp
  external/glb/level/export/navigation.cpp
  external/glb/level/export/rooms.cpp
  external/glb/level/fogs.cpp
  external/glb/level/import.cpp
  external/glb/level/import/geometry.cpp
  external/glb/level/import/lighting.cpp
  external/glb/level/import/navigation.cpp
  external/glb/level/import/portals.cpp
  external/glb/level/lighting.cpp
  external/glb/level/material.cpp
  external/glb/level/minimap.cpp
  external/glb/level/names.cpp
  external/glb/level/normals.cpp
  external/glb/level/objects.cpp
  external/glb/level/palette.cpp
  external/glb/level/paths.cpp
  external/glb/level/player_spawn.cpp
  external/glb/level/topology.cpp
  external/glb/level/zones/export.cpp
  external/glb/level/zones/import.cpp
  external/glb/level/zones/mesh.cpp
  external/glb/level/zones/naming.cpp
  external/glb/level/zones/reconstruction.cpp
  external/glb/model/animation_import.cpp
  external/glb/model/animation_export.cpp
  external/glb/model/discovery.cpp
  external/glb/model/export.cpp
  external/glb/model/import.cpp
  external/glb/model/mesh_export.cpp
  external/glb/model/preview.cpp
  external/glb/node_graph.cpp
  external/glb/primitive_indices.cpp
  external/glb/utils/image.cpp
  external/glb/utils/names.cpp
  external/glb/utils/texture.cpp
  external/glb/utils/tokens.cpp
  external/glb/utils/transform.cpp
  external/glb/writer.cpp
  level/glb.cpp
  model/glb.cpp
)
target_link_libraries(arx_pistoris_glb PRIVATE arx_pistoris_cgltf nlohmann_json)

arx_add_internal_object_library(arx_pistoris_json
  external/json/amb.cpp
  external/json/dlf.cpp
  external/json/ftl.cpp
  external/json/fts.cpp
  external/json/llf.cpp
  external/json/tea.cpp
)
target_link_libraries(arx_pistoris_json PRIVATE nlohmann_json)

arx_add_internal_object_library(arx_pistoris_obj
  external/obj/model/export.cpp
  external/obj/model/import.cpp
  model/obj.cpp
)

arx_add_internal_object_library(arx_pistoris_build_info ${ARX_BUILD_INFO_CPP})
if(ARX_BUILD_INFO_LIVE)
  add_dependencies(arx_pistoris_build_info arx_build_info_gen)
endif()

set(ARX_PISTORIS_INTERNAL_TARGETS
  arx_pistoris_utils
  ${ARX_PISTORIS_MODULE_TARGETS}
  arx_pistoris_external
  arx_pistoris_native
  arx_pistoris_paths
  arx_pistoris_editing
  arx_pistoris_native_conversion
  arx_pistoris_glb
  arx_pistoris_json
  arx_pistoris_obj
  arx_pistoris_build_info
)

set(ARX_PISTORIS_INTERNAL_DEPENDENCIES
  nlohmann_json
  mapbox_earcut
  arx_pistoris_cgltf
  arx_pistoris_audio_codecs
  arx_pistoris_stb
  arx_pistoris_pkware
)

if(ARX_COVERAGE)
  set_source_files_properties(api/strerror.cpp PROPERTIES COMPILE_OPTIONS "-fno-coverage-mapping")
endif()

set(ARX_PISTORIS_C_API_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/binary.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/buffer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/ambiance/access.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/ambiance/core.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/ambiance/edit.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/ambiance/glb.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/animation/access.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/animation/core.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/animation/edit.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/animation/list.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/level/access.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/level/core.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/level/edit.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/level/generation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/level/images.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/model/access.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/model/core.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/model/edit.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/model/generation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/native/amb.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/native/dlf.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/native/ftl.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/native/fts.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/native/llf.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/native/tea.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/paths/ambiance.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/paths/animation.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/paths/cinematic.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/paths/common.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/paths/level.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/paths/model.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/runtime.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/sound/handles.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/api/c/texture/handles.cpp
)

add_library(arx_pistoris_c SHARED
  api/binary.cpp
  api/strerror.cpp
  ${ARX_PISTORIS_C_API_SOURCES}
)
target_compile_definitions(arx_pistoris_c PRIVATE ARX_PISTORIS_EXPORTS)
target_include_directories(arx_pistoris_c
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}
)
target_link_libraries(arx_pistoris_c PRIVATE ${ARX_PISTORIS_INTERNAL_TARGETS} ${ARX_PISTORIS_INTERNAL_DEPENDENCIES})
set_target_properties(arx_pistoris_c PROPERTIES
  OUTPUT_NAME arx_pistoris
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)
arx_target_defaults(arx_pistoris_c)

add_library(arx_pistoris_cpp STATIC
  api/binary.cpp
  api/cpp/level_debug.cpp
  api/cpp/native.cpp
  api/cpp/native_json.cpp
  api/cpp/runtime.cpp
  api/strerror.cpp
)
target_compile_definitions(arx_pistoris_cpp PUBLIC ARX_PISTORIS_CPP_API PRIVATE ARX_PISTORIS_ENABLE_LEVEL_DEBUG_API)
target_include_directories(arx_pistoris_cpp
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}
)
target_link_libraries(arx_pistoris_cpp PRIVATE ${ARX_PISTORIS_INTERNAL_TARGETS} ${ARX_PISTORIS_INTERNAL_DEPENDENCIES})
arx_target_defaults(arx_pistoris_cpp)

set(ARX_PISTORIS_INTERNAL_TARGETS ${ARX_PISTORIS_INTERNAL_TARGETS} PARENT_SCOPE)
set(ARX_PISTORIS_INTERNAL_DEPENDENCIES ${ARX_PISTORIS_INTERNAL_DEPENDENCIES} PARENT_SCOPE)
set(ARX_PISTORIS_C_API_SOURCES ${ARX_PISTORIS_C_API_SOURCES} PARENT_SCOPE)
