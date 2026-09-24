// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "doctest/doctest.h"

#include "arx_pistoris/ambiance.h"
#include "arx_pistoris/ambiance.hpp"
#include "arx_pistoris/ambiance/bake.hpp"
#include "arx_pistoris/animation.h"
#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/animation/bake.hpp"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/math.h"
#include "arx_pistoris/cinematic.h"
#include "arx_pistoris/cinematic/bake.hpp"
#include "arx_pistoris/glb.hpp"
#include "arx_pistoris/level.h"
#include "arx_pistoris/level.hpp"
#include "arx_pistoris/level/images.h"
#include "arx_pistoris/level/images.hpp"
#include "arx_pistoris/model.h"
#include "arx_pistoris/model.hpp"
#include "arx_pistoris/model/bake.hpp"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/native.h"
#include "arx_pistoris/native.hpp"
#include "arx_pistoris/native/text.h"

TEST_SUITE("cpp_api") {
  TEST_CASE("GlbUnitRangeMatchesC") {
    CHECK(pistoris::glb::kMinArxUnitsPerUnit == ARX_GLB_MIN_ARX_UNITS_PER_UNIT);
    CHECK(pistoris::glb::kMaxArxUnitsPerUnit == ARX_GLB_MAX_ARX_UNITS_PER_UNIT);
  }

  TEST_CASE("CInitializersMatchCppDefaults") {
    const ArxQuat c_quat = ARX_QUAT_IDENTITY_INIT;
    const ArxQuat cpp_quat{};
    CHECK(c_quat.w == cpp_quat.w);
    CHECK(c_quat.x == cpp_quat.x);
    CHECK(c_quat.y == cpp_quat.y);
    CHECK(c_quat.z == cpp_quat.z);

    const ArxNativeModelBakeOptions c_model = ARX_NATIVE_MODEL_BAKE_OPTIONS_INIT;
    const pistoris::NativeModelBakeOptions cpp_model;
    CHECK((c_model.include_texture_files != 0) == cpp_model.include_texture_files);
    CHECK(c_model.text_mode == static_cast<ArxNativeTextMode>(cpp_model.text_mode));

    const ArxNativeAnimationBakeOptions c_animation = ARX_NATIVE_ANIMATION_BAKE_OPTIONS_INIT;
    const pistoris::NativeAnimationBakeOptions cpp_animation;
    CHECK((c_animation.include_sound_files != 0) == cpp_animation.include_sound_files);
    CHECK(c_animation.text_mode == static_cast<ArxNativeTextMode>(cpp_animation.text_mode));

    const ArxNativeAmbianceBakeOptions c_ambiance = ARX_NATIVE_AMBIANCE_BAKE_OPTIONS_INIT;
    const pistoris::NativeAmbianceBakeOptions cpp_ambiance;
    CHECK((c_ambiance.include_sound_files != 0) == cpp_ambiance.include_sound_files);
    CHECK(c_ambiance.text_mode == static_cast<ArxNativeTextMode>(cpp_ambiance.text_mode));

    const ArxNativeCinematicBakeOptions c_cinematic = ARX_NATIVE_CINEMATIC_BAKE_OPTIONS_INIT;
    const pistoris::NativeCinematicBakeOptions cpp_cinematic;
    CHECK((c_cinematic.include_illustration_files != 0) == cpp_cinematic.include_illustration_files);
    CHECK((c_cinematic.include_sound_files != 0) == cpp_cinematic.include_sound_files);
    CHECK(c_cinematic.illustration_format == static_cast<ArxImageFormat>(cpp_cinematic.illustration_format));
    CHECK(c_cinematic.text_mode == static_cast<ArxNativeTextMode>(cpp_cinematic.text_mode));

    const ArxDlfWriteOptions c_dlf = ARX_DLF_WRITE_OPTIONS_INIT;
    const pistoris::DlfWriteOptions cpp_dlf;
    CHECK(c_dlf.embedded_llf == nullptr);
    CHECK(cpp_dlf.embedded_lighting == nullptr);
    CHECK(c_dlf.signer.size == cpp_dlf.signer.size());

    const ArxLlfWriteOptions c_llf = ARX_LLF_WRITE_OPTIONS_INIT;
    const pistoris::LlfWriteOptions cpp_llf;
    CHECK(c_llf.signer.size == cpp_llf.signer.size());

    const ArxAmbianceGlbImportOptions c_ambiance_import = ARX_AMBIANCE_GLB_IMPORT_OPTIONS_INIT;
    const pistoris::Ambiance::GlbImportOptions cpp_ambiance_import;
    CHECK(c_ambiance_import.arx_units_per_glb_unit == cpp_ambiance_import.arx_units_per_glb_unit);

    const ArxAmbianceGlbExportOptions c_ambiance_export = ARX_AMBIANCE_GLB_EXPORT_OPTIONS_INIT;
    const pistoris::Ambiance::GlbExportOptions cpp_ambiance_export;
    CHECK(c_ambiance_export.arx_units_per_glb_unit == cpp_ambiance_export.arx_units_per_glb_unit);

    const ArxLevelVertexWeldOptions c_weld = ARX_LEVEL_VERTEX_WELD_OPTIONS_INIT;
    const pistoris::Level::VertexWeldOptions cpp_weld;
    CHECK(c_weld.radius == cpp_weld.radius);
    CHECK(c_weld.metric == static_cast<ArxLevelWeldMetric>(cpp_weld.metric));
    CHECK(c_weld.degenerate_faces == static_cast<ArxLevelDegenerateFacePolicy>(cpp_weld.degenerate_faces));

    const ArxLevelPortalSnapOptions c_portal_snap = ARX_LEVEL_PORTAL_SNAP_OPTIONS_INIT;
    const pistoris::Level::PortalSnapOptions cpp_portal_snap;
    CHECK(c_portal_snap.radius == cpp_portal_snap.radius);

    const ArxLevelNavSurfaceSourceOptions c_nav_source = ARX_LEVEL_NAV_SURFACE_SOURCE_OPTIONS_INIT;
    const pistoris::Level::NavSurfaceSourceOptions cpp_nav_source;
    CHECK(c_nav_source.clearance == cpp_nav_source.clearance);
    CHECK(c_nav_source.support_min_up_cos == cpp_nav_source.support_min_up_cos);
    CHECK(c_nav_source.support_ignore_flags == cpp_nav_source.support_ignore_flags);

    const ArxLevelNavSurfaceGenOptions c_nav_gen = ARX_LEVEL_NAV_SURFACE_GEN_OPTIONS_INIT;
    const pistoris::Level::NavSurfaceGenOptions cpp_nav_gen;
    CHECK(c_nav_gen.clearance == cpp_nav_gen.clearance);
    CHECK(c_nav_gen.support_min_up_cos == cpp_nav_gen.support_min_up_cos);
    CHECK(c_nav_gen.support_ignore_flags == cpp_nav_gen.support_ignore_flags);
    CHECK(c_nav_gen.radius == cpp_nav_gen.radius);
    CHECK(c_nav_gen.height == cpp_nav_gen.height);
    CHECK(c_nav_gen.max_step_up == cpp_nav_gen.max_step_up);

    const ArxLevelNavSurfacePruneOptions c_nav_prune = ARX_LEVEL_NAV_SURFACE_PRUNE_OPTIONS_INIT;
    const pistoris::Level::NavSurfacePruneOptions cpp_nav_prune;
    CHECK(c_nav_prune.min_component_area_ratio == cpp_nav_prune.min_component_area_ratio);
    CHECK(c_nav_prune.min_component_area == cpp_nav_prune.min_component_area);

    const ArxLevelAnchorGenOptions c_anchor_gen = ARX_LEVEL_ANCHOR_GEN_OPTIONS_INIT;
    const pistoris::Level::AnchorGenOptions cpp_anchor_gen;
    CHECK(c_anchor_gen.sample_spacing == cpp_anchor_gen.sample_spacing);
    CHECK(c_anchor_gen.radius == cpp_anchor_gen.radius);
    CHECK(c_anchor_gen.height == cpp_anchor_gen.height);

    const ArxLevelAnchorPruneOptions c_anchor_prune = ARX_LEVEL_ANCHOR_PRUNE_OPTIONS_INIT;
    const pistoris::Level::AnchorPruneOptions cpp_anchor_prune;
    CHECK(c_anchor_prune.min_component_anchor_ratio == cpp_anchor_prune.min_component_anchor_ratio);
    CHECK(c_anchor_prune.min_component_anchor_count == cpp_anchor_prune.min_component_anchor_count);

    const ArxLevelAnchorConnectionGenOptions c_connection = ARX_LEVEL_ANCHOR_CONNECTION_GEN_OPTIONS_INIT;
    const pistoris::Level::AnchorConnectionGenOptions cpp_connection;
    CHECK(c_connection.max_distance == cpp_connection.max_distance);
    CHECK(c_connection.max_step_distance == cpp_connection.max_step_distance);
    CHECK(c_connection.max_step_up == cpp_connection.max_step_up);
    CHECK(c_connection.radius_scale == cpp_connection.radius_scale);
    CHECK(c_connection.max_steps == cpp_connection.max_steps);

    const ArxLevelRoomDistanceGenOptions c_room_distance = ARX_LEVEL_ROOM_DISTANCE_GEN_OPTIONS_INIT;
    const pistoris::Level::RoomDistanceGenOptions cpp_room_distance;
    CHECK(c_room_distance.portal_side_offset == cpp_room_distance.portal_side_offset);
    CHECK(c_room_distance.sample_spacing == cpp_room_distance.sample_spacing);
    CHECK(c_room_distance.sample_height_offset == cpp_room_distance.sample_height_offset);
    CHECK(c_room_distance.max_link_distance == cpp_room_distance.max_link_distance);

    const ArxLevelStaticLightingGenOptions c_lighting = ARX_LEVEL_STATIC_LIGHTING_GEN_OPTIONS_INIT;
    const pistoris::Level::StaticLightingGenOptions cpp_lighting;
    CHECK(c_lighting.ambient_color.r == cpp_lighting.ambient_color.r);
    CHECK(c_lighting.ambient_color.g == cpp_lighting.ambient_color.g);
    CHECK(c_lighting.ambient_color.b == cpp_lighting.ambient_color.b);
    CHECK(c_lighting.global_factor == cpp_lighting.global_factor);
    CHECK((c_lighting.use_normals != 0) == cpp_lighting.use_normals);
    CHECK((c_lighting.use_shadows != 0) == cpp_lighting.use_shadows);

    const ArxLevelGlbImportOptions c_level_import = ARX_LEVEL_GLB_IMPORT_OPTIONS_INIT;
    const pistoris::Level::GlbImportOptions cpp_level_import;
    CHECK(c_level_import.arx_units_per_glb_unit == cpp_level_import.arx_units_per_glb_unit);
    CHECK(c_level_import.has_arx_offset == 0);
    CHECK_FALSE(cpp_level_import.arx_offset.has_value());

    const ArxLevelGlbExportOptions c_level_export = ARX_LEVEL_GLB_EXPORT_OPTIONS_INIT;
    const pistoris::Level::GlbExportOptions cpp_level_export;
    CHECK(c_level_export.arx_units_per_glb_unit == cpp_level_export.arx_units_per_glb_unit);
    CHECK(c_level_export.arx_offset.x == cpp_level_export.arx_offset.x);
    CHECK(c_level_export.arx_offset.y == cpp_level_export.arx_offset.y);
    CHECK(c_level_export.arx_offset.z == cpp_level_export.arx_offset.z);

    const ArxLevelGameMinimapRenderOptions c_game_minimap = ARX_LEVEL_GAME_MINIMAP_RENDER_OPTIONS_INIT;
    const pistoris::Level::GameMinimapRenderOptions cpp_game_minimap;
    CHECK(c_game_minimap.projection_offset.x == cpp_game_minimap.projection_offset.x);
    CHECK(c_game_minimap.projection_offset.y == cpp_game_minimap.projection_offset.y);
    CHECK(c_game_minimap.fill_color.r == cpp_game_minimap.fill_color.r);
    CHECK(c_game_minimap.fill_color.g == cpp_game_minimap.fill_color.g);
    CHECK(c_game_minimap.fill_color.b == cpp_game_minimap.fill_color.b);
    CHECK(c_game_minimap.border_color.r == cpp_game_minimap.border_color.r);
    CHECK(c_game_minimap.border_color.g == cpp_game_minimap.border_color.g);
    CHECK(c_game_minimap.border_color.b == cpp_game_minimap.border_color.b);

    const ArxLevelGameMinimapReprojectionOptions c_game_reprojection = ARX_LEVEL_GAME_MINIMAP_REPROJECTION_OPTIONS_INIT;
    const pistoris::level_images::GameMinimapReprojectionOptions cpp_game_reprojection;
    CHECK(c_game_reprojection.source_projection_offset.x == cpp_game_reprojection.source_projection_offset.x);
    CHECK(c_game_reprojection.source_projection_offset.y == cpp_game_reprojection.source_projection_offset.y);
    CHECK(c_game_reprojection.target_projection_offset.x == cpp_game_reprojection.target_projection_offset.x);
    CHECK(c_game_reprojection.target_projection_offset.y == cpp_game_reprojection.target_projection_offset.y);
    CHECK(c_game_reprojection.fill_color.r == cpp_game_reprojection.fill_color.r);
    CHECK(c_game_reprojection.fill_color.g == cpp_game_reprojection.fill_color.g);
    CHECK(c_game_reprojection.fill_color.b == cpp_game_reprojection.fill_color.b);
    CHECK(c_game_reprojection.border_color.r == cpp_game_reprojection.border_color.r);
    CHECK(c_game_reprojection.border_color.g == cpp_game_reprojection.border_color.g);
    CHECK(c_game_reprojection.border_color.b == cpp_game_reprojection.border_color.b);

    const ArxLevelMinimapGenerationOptions c_minimap = ARX_LEVEL_MINIMAP_GENERATION_OPTIONS_INIT;
    const pistoris::Level::MinimapGenerationOptions cpp_minimap;
    const auto check_sampler = [](const ArxLevelMinimapSampler& c, const pistoris::Level::MinimapSampler& cpp) {
      CHECK(c.image.data == cpp.image.data);
      CHECK(c.image.size == cpp.image.size);
      CHECK(c.color.r == cpp.color.r);
      CHECK(c.color.g == cpp.color.g);
      CHECK(c.color.b == cpp.color.b);
    };
    check_sampler(c_minimap.foreground, cpp_minimap.foreground);
    check_sampler(c_minimap.background, cpp_minimap.background);
    check_sampler(c_minimap.water, cpp_minimap.water);
    check_sampler(c_minimap.lava, cpp_minimap.lava);
    CHECK(c_minimap.halo_color.r == cpp_minimap.halo_color.r);
    CHECK(c_minimap.halo_color.g == cpp_minimap.halo_color.g);
    CHECK(c_minimap.halo_color.b == cpp_minimap.halo_color.b);
    CHECK(c_minimap.halo_radius == cpp_minimap.halo_radius);

    const ArxLevelNativeBakeOptions c_level_bake = ARX_LEVEL_NATIVE_BAKE_OPTIONS_INIT;
    const pistoris::Level::NativeBakeOptions cpp_level_bake;
    CHECK(c_level_bake.level_name.size == cpp_level_bake.level_name.size());
    CHECK((c_level_bake.include_texture_files != 0) == cpp_level_bake.include_texture_files);
    CHECK((c_level_bake.reconstruct_quads != 0) == cpp_level_bake.reconstruct_quads);
    CHECK(c_level_bake.dlf_scene_path.size == cpp_level_bake.dlf_scene_path.size());
    CHECK(c_level_bake.text_mode == static_cast<ArxNativeTextMode>(cpp_level_bake.text_mode));

    const ArxLevelDlfBakeOptions c_level_dlf = ARX_LEVEL_DLF_BAKE_OPTIONS_INIT;
    const pistoris::Level::DlfBakeOptions cpp_level_dlf;
    CHECK(c_level_dlf.level_name.size == cpp_level_dlf.level_name.size());
    CHECK(c_level_dlf.target_fts_offset.x == cpp_level_dlf.target_fts_offset.x);
    CHECK(c_level_dlf.target_fts_offset.y == cpp_level_dlf.target_fts_offset.y);
    CHECK(c_level_dlf.target_fts_offset.z == cpp_level_dlf.target_fts_offset.z);
    CHECK(c_level_dlf.dlf_scene_path.size == cpp_level_dlf.dlf_scene_path.size());
    CHECK(c_level_dlf.text_mode == static_cast<ArxNativeTextMode>(cpp_level_dlf.text_mode));

    const ArxModelGlbImportOptions c_model_import = ARX_MODEL_GLB_IMPORT_OPTIONS_INIT;
    const pistoris::Model::GlbImportOptions cpp_model_import;
    CHECK(c_model_import.arx_units_per_glb_unit == cpp_model_import.arx_units_per_glb_unit);

    const ArxModelGlbExportOptions c_model_export = ARX_MODEL_GLB_EXPORT_OPTIONS_INIT;
    const pistoris::Model::GlbExportOptions cpp_model_export;
    CHECK(c_model_export.arx_units_per_glb_unit == cpp_model_export.arx_units_per_glb_unit);

    const ArxModelLevelPreviewGlbOptions c_preview = ARX_MODEL_LEVEL_PREVIEW_GLB_OPTIONS_INIT;
    const pistoris::Model::LevelPreviewGlbOptions cpp_preview;
    CHECK(c_preview.arx_units_per_glb_unit == cpp_preview.arx_units_per_glb_unit);
    CHECK(c_preview.class_path.size == cpp_preview.class_path.size());
    CHECK(c_preview.asset_name.size == cpp_preview.asset_name.size());

    const ArxModelReferenceOptions c_reference = ARX_MODEL_REFERENCE_OPTIONS_INIT;
    const pistoris::Model::ReferenceOptions cpp_reference;
    CHECK((c_reference.snap_bone_origins != 0) == cpp_reference.snap_bone_origins);
    CHECK((c_reference.copy_bone_origin_selections != 0) == cpp_reference.copy_bone_origin_selections);
    CHECK((c_reference.copy_action_point_selections != 0) == cpp_reference.copy_action_point_selections);

    const ArxModelInventoryIconSetOptions c_icon_set = ARX_MODEL_INVENTORY_ICON_SET_OPTIONS_INIT;
    const pistoris::Model::InventoryIconSetOptions cpp_icon_set;
    CHECK(c_icon_set.width_slots == cpp_icon_set.width_slots);
    CHECK(c_icon_set.height_slots == cpp_icon_set.height_slots);

    const ArxModelInventoryIconRenderOptions c_icon_render = ARX_MODEL_INVENTORY_ICON_RENDER_OPTIONS_INIT;
    const pistoris::Model::InventoryIconRenderOptions cpp_icon_render;
    CHECK(c_icon_render.width_slots == cpp_icon_render.width_slots);
    CHECK(c_icon_render.height_slots == cpp_icon_render.height_slots);
    CHECK(c_icon_render.layout == ARX_MODEL_INVENTORY_ICON_LAYOUT_CENTER);
    CHECK(c_icon_render.layout == static_cast<ArxModelInventoryIconLayout>(cpp_icon_render.layout));

    const ArxObjExportOptions c_obj = ARX_OBJ_EXPORT_OPTIONS_INIT;
    const pistoris::ObjExportOptions cpp_obj;
    CHECK((c_obj.include_files != 0) == cpp_obj.include_files);
  }
}
