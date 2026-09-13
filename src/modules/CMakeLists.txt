# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

arx_add_internal_object_library(arx_pistoris_module_action_points
  action_points/operations.cpp
  action_points/transformation.cpp
  action_points/validation.cpp
)

arx_add_internal_object_library(arx_pistoris_module_ambiance
  ambiance/operations.cpp
  ambiance/timing.cpp
  ambiance/validation.cpp
)

arx_add_internal_object_library(arx_pistoris_module_animation
  animation/operations.cpp
  animation/queries.cpp
  animation/transformation.cpp
  animation/validation.cpp
)

arx_add_internal_object_library(arx_pistoris_module_geometry
  geometry/index.cpp
  geometry/operations.cpp
  geometry/position_index.cpp
  geometry/raycast.cpp
  geometry/transformation.cpp
  geometry/surface_support.cpp
  geometry/triangle.cpp
  geometry/validation.cpp
  geometry/vertex_weld.cpp
)

arx_add_internal_object_library(arx_pistoris_module_textures
  textures/operations.cpp
  textures/paths.cpp
  textures/preparation.cpp
  textures/validation.cpp
)

arx_add_internal_object_library(arx_pistoris_module_lights
  lights/operations.cpp
  lights/static_generation.cpp
  lights/validation.cpp
)

arx_add_internal_object_library(arx_pistoris_module_inventory_icon
  inventory_icon/operations.cpp
  inventory_icon/rendering.cpp
  inventory_icon/validation.cpp
)

arx_add_internal_object_library(arx_pistoris_module_loading_screen
  loading_screen/operations.cpp
  loading_screen/rendering.cpp
  loading_screen/validation.cpp
)

arx_add_internal_object_library(arx_pistoris_module_minimap
  minimap/generation.cpp
  minimap/operations.cpp
  minimap/rendering.cpp
  minimap/validation.cpp
)

arx_add_internal_object_library(arx_pistoris_module_navigation
  navigation/anchor/connection.cpp
  navigation/anchor/generation.cpp
  navigation/anchor/pruning.cpp
  navigation/collision/contact.cpp
  navigation/collision/index.cpp
  navigation/collision/placement.cpp
  navigation/collision/scene.cpp
  navigation/generation.cpp
  navigation/operations.cpp
  navigation/repair.cpp
  navigation/sampling.cpp
  navigation/support.cpp
  navigation/surface/common.cpp
  navigation/surface/floor.cpp
  navigation/surface/generation.cpp
  navigation/surface/lattice.cpp
  navigation/surface/pruning.cpp
  navigation/surface/repair.cpp
  navigation/traversal.cpp
  navigation/validation.cpp
)

arx_add_internal_object_library(arx_pistoris_module_resource
  resource/operations.cpp
  resource/validation.cpp
)

arx_add_internal_object_library(arx_pistoris_module_rooms
  rooms/distance_generation.cpp
  rooms/distance_graph.cpp
  rooms/distance_path.cpp
  rooms/distance_sampling.cpp
  rooms/distance_storage.cpp
  rooms/distance_visibility.cpp
  rooms/operations.cpp
  rooms/portal.cpp
  rooms/portal_distance.cpp
  rooms/room_geometry_index.cpp
  rooms/validation.cpp
  rooms/vertex_weld_segments.cpp
)

arx_add_internal_object_library(arx_pistoris_module_scene
  scene/operations.cpp
  scene/validation.cpp
)

arx_add_internal_object_library(arx_pistoris_module_selections
  selections/generation.cpp
  selections/operations.cpp
  selections/transformation.cpp
  selections/validation.cpp
)

arx_add_internal_object_library(arx_pistoris_module_skeleton
  skeleton/operations.cpp
  skeleton/transformation.cpp
  skeleton/validation.cpp
)

arx_add_internal_object_library(arx_pistoris_module_sounds
  sounds/inspection.cpp
  sounds/operations.cpp
  sounds/preparation.cpp
  sounds/validation.cpp
)

set(ARX_PISTORIS_MODULE_TARGETS
  arx_pistoris_module_action_points
  arx_pistoris_module_ambiance
  arx_pistoris_module_animation
  arx_pistoris_module_geometry
  arx_pistoris_module_inventory_icon
  arx_pistoris_module_lights
  arx_pistoris_module_loading_screen
  arx_pistoris_module_minimap
  arx_pistoris_module_navigation
  arx_pistoris_module_resource
  arx_pistoris_module_rooms
  arx_pistoris_module_scene
  arx_pistoris_module_selections
  arx_pistoris_module_skeleton
  arx_pistoris_module_sounds
  arx_pistoris_module_textures
  PARENT_SCOPE
)
