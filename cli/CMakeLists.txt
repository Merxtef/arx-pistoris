# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

add_library(arx_pistoris_cli_support STATIC
  base/resource_path.cpp
  console/diagnostics.cpp
  console/logging.cpp
  console/style.cpp
  formats/classification.cpp
  formats/format.cpp
  io/files.cpp
  io/mounts.cpp
  io/native_path.cpp
  io/native_text.cpp
  io/paths.cpp
  io/service.cpp
  media/encoded.cpp
  modules/implication.cpp
  modules/module.cpp
  pipeline/options.cpp
  resources/animation_output.cpp
  resources/cinematic_sound_io.cpp
  resources/inventory_icon_io.cpp
  resources/input.cpp
  resources/level_image_io.cpp
  resources/output.cpp
  resources/read_diagnostics.cpp
  resources/resource_output.cpp
  resources/selector.cpp
  resources/sidecar_io.cpp
  resources/sound_io.cpp
  resources/texture_io.cpp
)
target_link_libraries(arx_pistoris_cli_support PRIVATE arx_pistoris_cpp nlohmann_json)
target_compile_definitions(arx_pistoris_cli_support PRIVATE ARX_PISTORIS_ENABLE_LEVEL_DEBUG_API)
target_include_directories(arx_pistoris_cli_support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
arx_target_defaults(arx_pistoris_cli_support)
if(WIN32)
  target_sources(arx_pistoris_cli_support PRIVATE console/terminal_windows.cpp io/default_mounts_windows.cpp)
  target_link_libraries(arx_pistoris_cli_support PRIVATE shell32 ole32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources(arx_pistoris_cli_support PRIVATE console/terminal_linux.cpp io/default_mounts_linux.cpp)
else()
  message(FATAL_ERROR "arx-pistor automatic mounts support only Windows and Linux")
endif()

add_executable(arx_pistoris_cli
  app.cpp
  console/help.cpp
  console/help_request.cpp
  conversion/spatial.cpp
  main.cpp
  modules/format/glb.cpp
  modules/output/formats.cpp
  modules/parsing.cpp
  modules/registry.cpp
  modules/root.cpp
  modules/system/control.cpp
  modules/system/io_policy.cpp
  modules/system/resource_listing.cpp
  modules/sounds/io.cpp
  modules/textures/io.cpp
  modules/transform/spatial.cpp
  modules/transform/sounds.cpp
  modules/transform/textures.cpp
  modules/validation.cpp
  pipeline/executor.cpp
  pipeline/parser.cpp
  pipeline/resolver.cpp
  resources/discovery.cpp
  resources/level_json.cpp
  resources/model_input.cpp
  resources/model_input_io.cpp
  routes/ambiance/load.cpp
  routes/ambiance/operations.cpp
  routes/ambiance/options/reference.cpp
  routes/ambiance/options/root.cpp
  routes/ambiance/options/timing.cpp
  routes/ambiance/resolution.cpp
  routes/ambiance/route.cpp
  routes/ambiance/save.cpp
  routes/animation/load.cpp
  routes/animation/operations.cpp
  routes/animation/resolution.cpp
  routes/animation/route.cpp
  routes/animation/save.cpp
  routes/cinematic/load.cpp
  routes/cinematic/options/rebase.cpp
  routes/cinematic/options/root.cpp
  routes/cinematic/resolution.cpp
  routes/cinematic/route.cpp
  routes/cinematic/save.cpp
  routes/level/load.cpp
  routes/level/native_carriers.cpp
  routes/level/operations.cpp
  routes/level/options/anchor_connections.cpp
  routes/level/options/anchor_generation.cpp
  routes/level/options/anchor_pruning.cpp
  routes/level/options/debug_exports.cpp
  routes/level/options/minimap_generation.cpp
  routes/level/options/native_bake.cpp
  routes/level/options/nav_surface_generation.cpp
  routes/level/options/nav_surface_pruning.cpp
  routes/level/options/output.cpp
  routes/level/options/previews.cpp
  routes/level/options/room_distances.cpp
  routes/level/options/root.cpp
  routes/level/options/static_lighting.cpp
  routes/level/options/vertex_welding.cpp
  routes/level/resolution.cpp
  routes/level/route.cpp
  routes/level/save.cpp
  routes/model/animation_sidecars.cpp
  routes/model/load.cpp
  routes/model/operations.cpp
  routes/model/options/animation_output.cpp
  routes/model/options/inventory_icon.cpp
  routes/model/options/root.cpp
  routes/model/options/preview.cpp
  routes/model/options/reference.cpp
  routes/model/options/reference_operations.cpp
  routes/model/options/selection_inference.cpp
  routes/model/resolution.cpp
  routes/model/route.cpp
  routes/model/save.cpp
  routes/registry.cpp
)
target_link_libraries(arx_pistoris_cli PRIVATE arx_pistoris_cli_support arx_pistoris_cpp)
target_compile_definitions(arx_pistoris_cli PRIVATE ARX_PISTORIS_ENABLE_LEVEL_DEBUG_API)
target_include_directories(arx_pistoris_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
arx_target_defaults(arx_pistoris_cli)
target_link_options(arx_pistoris_cli PRIVATE
  $<$<AND:$<CONFIG:Release>,$<PLATFORM_ID:Linux>>:-static-libstdc++>
  $<$<AND:$<CONFIG:Release>,$<PLATFORM_ID:Linux>>:-static-libgcc>
)
set_target_properties(arx_pistoris_cli PROPERTIES OUTPUT_NAME arx-pistor)
