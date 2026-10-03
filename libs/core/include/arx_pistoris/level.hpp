// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/image.hpp"
#include "arx_pistoris/base/indexed_view.hpp"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/level/bake.hpp"
#include "arx_pistoris/level/images.hpp"
#include "arx_pistoris/level/location.hpp"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/native/text.hpp"
#include "arx_pistoris/texture.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pistoris {

namespace dlf {
struct Data;
}

namespace fts {
struct Data;
}

namespace llf {
struct Data;
}

struct NativeLevelBundle;
class Model;

inline constexpr FaceType kLevelFaceBitsAll = ARX_LEVEL_FACE_BITS_ALL;
inline constexpr float kLevelMinXZ = 0.0f;
inline constexpr float kLevelMaxXZ = 16000.0f;
inline constexpr std::int16_t kAnchorFlagBlocked = 1 << 3;
inline constexpr std::int16_t kAnchorFlagsAll = kAnchorFlagBlocked;
inline constexpr float kDefaultAnchorRadius = 50.0f;
inline constexpr float kDefaultAnchorHeight = -165.0f;
inline constexpr float kDefaultPortalSnapRadius = 1.0f;

inline constexpr float kMinAnchorRadius = 5.0f;
inline constexpr float kMinAnchorSpacing = kMinAnchorRadius * 2.0f;
inline constexpr float kMinAnchorHeight = -20.0f;
inline constexpr float kDefaultNavSurfaceMaxStepUp = 55.0f;
inline constexpr float kDefaultNavSurfaceStepDistance = 40.0f;
inline constexpr float kDefaultNavSurfaceClearance = 5.0f;
inline constexpr float kDefaultNavSurfaceSupportMinUpCos = 0.5881716976750462f;
inline constexpr FaceType kDefaultNavSurfaceIgnoreFlags = kFaceBitWater | kFaceBitTrans | kFaceBitNocol | kFaceBitLava;

inline constexpr float kMinRoomDistanceSampleSpacing = 20.0f;
inline constexpr float kMaxRoomDistancePortalOffset = 50.0f;
inline constexpr float kMinRoomDistanceSampleHeight = 50.0f;
inline constexpr float kDefaultRoomDistancePortalOffset = 10.0f;
inline constexpr float kDefaultRoomDistanceSampleSpacing = 100.0f;
inline constexpr float kDefaultRoomDistanceSampleHeight = 82.5f;
inline constexpr float kRoomDistanceDefaultLinkDistanceSpacingFactor = 1.5f;
inline constexpr float kRoomDistanceMinLinkDistanceSpacingFactor = 1.1f;

inline constexpr ArxColor3 kDefaultStaticLightingAmbientColor = {0.25f, 0.25f, 0.25f};
inline constexpr float kDefaultStaticLightingGlobalFactor = 0.85f;

namespace level_debug {

struct LevelDebugAccess;

}  // namespace level_debug

// Collection indices are current zero-based positions, not persistent identities
// Non-const calls invalidate collection indices and borrowed views
class Level {
 public:
  struct VerticesViewTag;
  struct FacesViewTag;
  struct TexturesViewTag;
  struct RoomsViewTag;
  struct PortalsViewTag;
  struct RoomDistancesViewTag;
  struct AnchorsViewTag;
  struct AnchorConnectionsViewTag;
  struct NavSurfaceVerticesViewTag;
  struct NavSurfaceTrianglesViewTag;
  struct LightsViewTag;
  struct EntitiesViewTag;
  struct FogsViewTag;
  struct ZonesViewTag;
  struct ZonePerimeterViewTag;
  struct PathsViewTag;
  struct PathNodesViewTag;

  using VerticesView = IndexedView<ArxLevelVertex, VerticesViewTag>;
  using FacesView = IndexedView<ArxLevelFace, FacesViewTag>;
  using TexturesView = IndexedView<ArxTextureView, TexturesViewTag>;
  using RoomsView = IndexedView<ArxLevelRoom, RoomsViewTag>;
  using PortalsView = IndexedView<ArxLevelPortal, PortalsViewTag>;
  using RoomDistancesView = IndexedView<ArxLevelRoomDistance, RoomDistancesViewTag>;
  using AnchorsView = IndexedView<ArxLevelAnchor, AnchorsViewTag>;
  using AnchorConnectionsView = IndexedView<ArxLevelAnchorConnection, AnchorConnectionsViewTag>;
  using NavSurfaceVerticesView = IndexedView<ArxLevelVertex, NavSurfaceVerticesViewTag>;
  using NavSurfaceTrianglesView = IndexedView<ArxLevelNavSurfaceTriangle, NavSurfaceTrianglesViewTag>;
  using LightsView = IndexedView<ArxLevelLight, LightsViewTag>;
  using EntitiesView = IndexedView<ArxLevelEntity, EntitiesViewTag>;
  using FogsView = IndexedView<ArxLevelFog, FogsViewTag>;
  using ZonesView = IndexedView<ArxLevelZone, ZonesViewTag>;
  using ZonePerimeterView = IndexedView<ArxVector2, ZonePerimeterViewTag>;
  using PathsView = IndexedView<ArxLevelPath, PathsViewTag>;
  using PathNodesView = IndexedView<ArxLevelPathNode, PathNodesViewTag>;
  enum class PositionWeldMetric : std::uint8_t {
    kEuclidean,
    kAxisAligned,
  };

  enum class DegenerateFacePolicy : std::uint8_t {
    kPreserve,
    kReject,
    kDiscard,
  };

  struct VertexWeldOptions {
    // Positive weld tolerance
    float radius = 1.0e-4f;
    PositionWeldMetric metric = PositionWeldMetric::kEuclidean;
    DegenerateFacePolicy degenerate_faces = DegenerateFacePolicy::kPreserve;
  };

  struct PortalSnapOptions {
    // Positive finite maximum 3D geometry-to-portal distance in Arx units
    float radius = kDefaultPortalSnapRadius;
  };

  struct NavSurfaceSourceOptions {
    // Applied along Arx up (-Y)
    float clearance = kDefaultNavSurfaceClearance;
    // Minimum support-normal dot with Arx up
    float support_min_up_cos = kDefaultNavSurfaceSupportMinUpCos;
    // Ignored support flags; NOPATH always ignored
    FaceType support_ignore_flags = kDefaultNavSurfaceIgnoreFlags;
  };

  struct NavSurfaceGenOptions : NavSurfaceSourceOptions {
    float radius = kDefaultAnchorRadius;
    // Signed cylinder height, -Y up
    float height = kDefaultAnchorHeight;
    // Maximum upward support correction
    float max_step_up = kDefaultNavSurfaceMaxStepUp;
  };

  struct NavSurfacePruneOptions {
    // Fraction of largest component, range [0, 1]
    float min_component_area_ratio = 0.05f;
    // Absolute area floor in square Arx units
    double min_component_area = 0.0;
  };

  struct AnchorGenOptions {
    float sample_spacing = 100.0f;
    float radius = kDefaultAnchorRadius;
    // Signed cylinder height, -Y up
    float height = kDefaultAnchorHeight;
  };

  struct AnchorPruneOptions {
    // Fraction of largest component, range [0, 1]
    float min_component_anchor_ratio = 0.05f;
    // Absolute component count floor
    std::uint32_t min_component_anchor_count = 1;
  };

  struct AnchorConnectionGenOptions {
    // Maximum XZ candidate distance
    float max_distance = 150.0f;
    // Maximum traversal step length
    float max_step_distance = 40.0f;
    // Maximum upward correction per step
    float max_step_up = 55.0f;
    // Traversal radius factor, range [0.5, 1]
    float radius_scale = 0.9f;
    int max_steps = 100;
  };

  struct RoomDistanceGenOptions {
    // Portal-to-access-point offset
    float portal_side_offset = kDefaultRoomDistancePortalOffset;
    float sample_spacing = kDefaultRoomDistanceSampleSpacing;
    // Support-to-node offset, -Y up
    float sample_height_offset = kDefaultRoomDistanceSampleHeight;
    // 0 selects 1.5 * sample_spacing
    float max_link_distance = 0.0f;
  };

  struct StaticLightingGenOptions {
    // Minimum corner color, components [0, 1]
    ArxColor3 ambient_color = kDefaultStaticLightingAmbientColor;
    // Nonnegative legacy static-light multiplier
    float global_factor = kDefaultStaticLightingGlobalFactor;
    bool use_normals = true;
    bool use_shadows = true;
  };

  struct GlbUnitOptions {
    // Range [1, 1000]
    float arx_units_per_glb_unit = 100.0f;
  };

  struct GlbImportOptions : GlbUnitOptions {
    // Arx position mapped to GLB origin
    // Empty selects automatic 100-unit-aligned XZ placement
    std::optional<ArxVector3> arx_offset;
  };

  struct GlbExportOptions : GlbUnitOptions {
    // Arx position mapped to GLB origin
    ArxVector3 arx_offset = {};
  };

  struct MinimapView {
    ArxEncodedImageView encoded_image{};
    ArxRect world_xz_bounds{};
  };

  struct MinimapRenderOptions {
    level_images::MinimapRenderMode mode = level_images::MinimapRenderMode::kPlain;
    // Arx-unit projection offset
    // Empty derives compact placement; only valid in plain mode
    std::optional<ArxVector2> projection_offset;
    ArxColor3 fill_color{};
    // Only valid in game mode; empty selects white
    std::optional<ArxColor3> border_color;
    ImageFormat format = ImageFormat::kPng;
  };

  struct MinimapSampler {
    ArxEncodedImageView image{};
    // Constant color or image multiplier, components [0, 1]
    ArxColor3 color{};
  };

  struct MinimapGenerationOptions {
    MinimapSampler foreground{.color = {0.18f, 0.34f, 0.80f}};
    MinimapSampler background{.color = {0.56f, 0.68f, 0.90f}};
    MinimapSampler water{.color = {0.72f, 0.60f, 0.45f}};
    MinimapSampler lava{.color = {0.25f, 0.80f, 0.90f}};
    // Applied to background pixels nearest occupied geometry
    ArxColor3 halo_color{1.0f, 1.0f, 1.0f};
    // Chebyshev radius in pixels; 0 disables halo
    std::uint32_t halo_radius = 5;
  };

  struct RenderedMinimap {
    ArxVector2 projection_offset{};
    std::vector<std::uint8_t> encoded_image;
  };

  struct LoadingScreenRenderOptions {
    level_images::LoadingScreenLayout layout = level_images::LoadingScreenLayout::kOriginal;
    ImageFormat format = ImageFormat::kPng;
  };

  struct NativeBakeOptions {
    // Used when dlf_scene_path is empty
    std::string_view level_name = {};
    bool include_texture_files = true;
    NativeTextMode text_mode = NativeTextMode::kAuto;
    bool reconstruct_quads = true;
    // Overrides level_name-derived path
    std::string_view dlf_scene_path = {};
  };

  struct DlfBakeOptions {
    // Used when dlf_scene_path is empty
    std::string_view level_name;
    // Subtracted from DLF scene positions
    ArxVector3 target_fts_offset = {};
    // Overrides level_name-derived path
    std::string_view dlf_scene_path = {};
    NativeTextMode text_mode = NativeTextMode::kAuto;
  };

  // --- Lifetime ---

  Level();
  ~Level();

  Level(const Level& other);
  Level(Level&& other) noexcept;
  Level& operator=(const Level& other);
  Level& operator=(Level&& other) noexcept;

  void swap(Level& other) noexcept;
  friend void swap(Level& first, Level& second) noexcept { first.swap(second); }
  [[nodiscard]] LevelResult<void> reset() noexcept;

  // --- Conversion ---

  [[nodiscard]] static LevelNativeResult<Level> importNative(const fts::Data& fts, const llf::Data* llf = nullptr,
                                                             const dlf::Data* dlf = nullptr,
                                                             std::vector<std::string>* texture_source_paths = nullptr,
                                                             NativeTextMode text_mode = NativeTextMode::kAuto) noexcept;
  [[nodiscard]] static GlbResult<Level> importGlb(std::span<const std::uint8_t> data) noexcept;
  [[nodiscard]] static GlbResult<Level> importGlb(std::span<const std::uint8_t> data, const GlbImportOptions& options,
                                                  ArxLevelGlbImportInfo* info = nullptr,
                                                  std::vector<std::string>* texture_source_paths = nullptr) noexcept;

  [[nodiscard]] LevelGlbExportResult<std::vector<std::uint8_t>> exportGlb() const noexcept;
  [[nodiscard]] LevelGlbExportResult<std::vector<std::uint8_t>> exportGlb(
      const GlbExportOptions& options) const noexcept;
  [[nodiscard]] LevelGlbExportResult<std::vector<std::uint8_t>> exportGlb(
      std::span<const Model* const> model_previews, ArxLevelModelPreviewReport* report = nullptr) const noexcept;
  [[nodiscard]] LevelGlbExportResult<std::vector<std::uint8_t>> exportGlb(
      std::span<const Model* const> model_previews, const GlbExportOptions& options,
      ArxLevelModelPreviewReport* report = nullptr) const noexcept;
  [[nodiscard]] LevelResult<NativeLevelBundle> bakeNativeBundle(const NativeBakeOptions& options) const noexcept;
  [[nodiscard]] LevelResult<dlf::Data> bakeDlf(const DlfBakeOptions& options) const noexcept;

  // --- Validation ---

  [[nodiscard]] LevelResult<void> validate() const noexcept;
  [[nodiscard]] LevelResult<void> validateMesh() const noexcept;
  [[nodiscard]] LevelResult<void> validateVertices() const noexcept;
  [[nodiscard]] LevelResult<void> validateTextures() const noexcept;
  [[nodiscard]] LevelResult<void> validateFaces() const noexcept;
  [[nodiscard]] LevelResult<void> validateFaceRooms() const noexcept;
  [[nodiscard]] LevelResult<void> validateCornerColors() const noexcept;
  [[nodiscard]] LevelResult<void> validateRooms() const noexcept;
  [[nodiscard]] LevelResult<void> validatePortals() const noexcept;
  [[nodiscard]] LevelResult<void> validateRoomDistances() const noexcept;
  [[nodiscard]] LevelResult<void> validateNavSurface() const noexcept;
  [[nodiscard]] LevelResult<void> validateAnchors() const noexcept;
  [[nodiscard]] LevelResult<void> validateAnchorConnections() const noexcept;
  [[nodiscard]] LevelResult<void> validateLights() const noexcept;
  [[nodiscard]] LevelResult<void> validatePlayerSpawn() const noexcept;
  [[nodiscard]] LevelResult<void> validateEntities() const noexcept;
  [[nodiscard]] LevelResult<void> validateFogs() const noexcept;
  [[nodiscard]] LevelResult<void> validateZones() const noexcept;
  [[nodiscard]] LevelResult<void> validatePaths() const noexcept;
  [[nodiscard]] LevelResult<void> validateMinimap() const noexcept;
  [[nodiscard]] LevelResult<void> validateLoadingScreen() const noexcept;

  // --- Resource data ---

  [[nodiscard]] std::string_view resourcePath() const noexcept;
  [[nodiscard]] LevelResult<void> setResourcePath(std::string_view resource_path) noexcept;

  // --- Images ---

  [[nodiscard]] MinimapView minimap() const noexcept;
  [[nodiscard]] ArxEncodedImageView loadingScreen() const noexcept;
  [[nodiscard]] LevelResult<void> setMinimap(ArxEncodedImageView encoded_image, ArxRect world_xz_bounds) noexcept;
  [[nodiscard]] LevelResult<void> setMinimapFromProjection(ArxEncodedImageView encoded_image,
                                                           ArxVector2 projection_offset) noexcept;
  void clearMinimap() noexcept;
  [[nodiscard]] LevelResult<RenderedMinimap> renderMinimap() const noexcept;
  [[nodiscard]] LevelResult<RenderedMinimap> renderMinimap(const MinimapRenderOptions& options) const noexcept;
  [[nodiscard]] LevelResult<void> setLoadingScreen(ArxEncodedImageView encoded_image) noexcept;
  void clearLoadingScreen() noexcept;
  [[nodiscard]] LevelResult<std::vector<std::uint8_t>> renderLoadingScreen() const noexcept;
  [[nodiscard]] LevelResult<std::vector<std::uint8_t>> renderLoadingScreen(
      const LoadingScreenRenderOptions& options) const noexcept;

  // --- Inspection ---

  [[nodiscard]] std::optional<ArxAabb> bounds() const noexcept;
  [[nodiscard]] std::optional<ArxAabb> referencedBounds() const noexcept;

  [[nodiscard]] std::size_t vertexCount() const noexcept;
  [[nodiscard]] std::size_t faceCount() const noexcept;
  [[nodiscard]] std::size_t textureCount() const noexcept;
  [[nodiscard]] std::size_t roomCount() const noexcept;
  [[nodiscard]] std::size_t portalCount() const noexcept;
  // One entry per unordered pair of distinct rooms. Unavailable pairs use the default sentinel value.
  [[nodiscard]] std::size_t roomDistanceCount() const noexcept;
  [[nodiscard]] std::size_t anchorCount() const noexcept;
  [[nodiscard]] std::size_t anchorConnectionCount() const noexcept;
  [[nodiscard]] std::size_t lightCount() const noexcept;
  [[nodiscard]] std::size_t entityCount() const noexcept;
  [[nodiscard]] std::size_t fogCount() const noexcept;
  [[nodiscard]] std::size_t zoneCount() const noexcept;
  [[nodiscard]] std::size_t pathCount() const noexcept;

  [[nodiscard]] VerticesView vertices() const noexcept;
  [[nodiscard]] FacesView faces() const noexcept;
  [[nodiscard]] TexturesView textures() const noexcept;
  [[nodiscard]] RoomsView rooms() const noexcept;
  [[nodiscard]] PortalsView portals() const noexcept;
  [[nodiscard]] RoomDistancesView roomDistances() const noexcept;
  [[nodiscard]] AnchorsView anchors() const noexcept;
  [[nodiscard]] AnchorConnectionsView anchorConnections() const noexcept;
  [[nodiscard]] ArxLevelNavSurfaceInfo navSurfaceInfo() const noexcept;
  [[nodiscard]] NavSurfaceVerticesView navSurfaceVertices() const noexcept;
  [[nodiscard]] NavSurfaceTrianglesView navSurfaceTriangles() const noexcept;
  [[nodiscard]] LightsView lights() const noexcept;
  [[nodiscard]] ArxLevelPlayerSpawn playerSpawn() const noexcept;
  [[nodiscard]] EntitiesView entities() const noexcept;
  [[nodiscard]] FogsView fogs() const noexcept;
  [[nodiscard]] ZonesView zones() const noexcept;
  [[nodiscard]] LevelResult<ZonePerimeterView> zonePerimeter(ZoneIndex zone) const noexcept;
  [[nodiscard]] PathsView paths() const noexcept;
  [[nodiscard]] LevelResult<PathNodesView> pathNodes(PathIndex path) const noexcept;
  // Returns the canonical room order and the default sentinel when no distance is available.
  [[nodiscard]] LevelResult<ArxLevelRoomDistance> roomDistance(RoomIndex room_a, RoomIndex room_b) const noexcept;

  // --- Mesh editing ---

  [[nodiscard]] LevelResult<void> setVertex(VertexIndex index, ArxLevelVertex vertex) noexcept;
  [[nodiscard]] LevelResult<VertexIndex> addVertex(ArxLevelVertex vertex) noexcept;
  [[nodiscard]] LevelResult<VertexIndex> addVertices(std::span<const ArxLevelVertex> vertices) noexcept;
  [[nodiscard]] LevelResult<void> setFace(FaceIndex index, const ArxLevelFace& face) noexcept;
  [[nodiscard]] LevelResult<FaceIndex> addFace(const ArxLevelFace& face) noexcept;
  [[nodiscard]] LevelResult<void> removeFace(FaceIndex index) noexcept;
  [[nodiscard]] LevelResult<std::size_t> compactVertices() noexcept;
  [[nodiscard]] LevelResult<std::size_t> compactTextures() noexcept;
  [[nodiscard]] LevelResult<void> rebaseTexturePaths(std::string_view directory) noexcept;
  [[nodiscard]] LevelResult<void> weldVertices() noexcept;
  [[nodiscard]] LevelResult<void> weldVertices(const VertexWeldOptions& options) noexcept;
  [[nodiscard]] LevelResult<void> snapGeometryToPortals() noexcept;
  [[nodiscard]] LevelResult<void> snapGeometryToPortals(const PortalSnapOptions& options) noexcept;
  [[nodiscard]] LevelResult<void> setTexture(TextureIndex index, const ArxTextureView& texture) noexcept;
  [[nodiscard]] LevelResult<TextureIndex> addTexture(const ArxTextureView& texture) noexcept;
  [[nodiscard]] LevelResult<void> setTexturePath(TextureIndex index, std::string_view path) noexcept;
  [[nodiscard]] LevelResult<void> setTextureExternalImageExtension(TextureIndex index,
                                                                   std::string_view extension) noexcept;
  [[nodiscard]] LevelResult<void> setTextureImage(TextureIndex index, ArxEncodedImageView encoded_image) noexcept;
  [[nodiscard]] LevelResult<void> clearTextureImage(TextureIndex index) noexcept;
  [[nodiscard]] LevelResult<void> setFaceRoom(FaceIndex face, RoomIndex room) noexcept;
  [[nodiscard]] LevelResult<void> setCornerColor(FaceIndex face, std::uint8_t corner, ArxColor3 color) noexcept;
  void resetCornerColors() noexcept;
  [[nodiscard]] LevelResult<void> replaceMesh(const ArxLevelMeshInput& mesh) noexcept;
  void clearMesh() noexcept;

  // --- Rooms ---

  [[nodiscard]] LevelResult<void> setRoom(RoomIndex index, const ArxLevelRoom& room) noexcept;
  [[nodiscard]] LevelResult<RoomIndex> addRoom(const ArxLevelRoom& room) noexcept;
  [[nodiscard]] LevelResult<void> removeRoom(RoomIndex index) noexcept;
  [[nodiscard]] LevelResult<void> setPortal(PortalIndex index, const ArxLevelPortal& portal) noexcept;
  [[nodiscard]] LevelResult<PortalIndex> addPortal(const ArxLevelPortal& portal) noexcept;
  [[nodiscard]] LevelResult<void> removePortal(PortalIndex index) noexcept;
  [[nodiscard]] LevelResult<void> flattenPortals() noexcept;
  [[nodiscard]] LevelResult<void> setRoomDistance(const ArxLevelRoomDistance& distance) noexcept;
  [[nodiscard]] LevelResult<void> replaceRoomDistances(std::span<const ArxLevelRoomDistance> distances) noexcept;
  void clearRoomDistances() noexcept;

  // --- Navigation ---

  [[nodiscard]] LevelResult<void> setAnchor(AnchorIndex index, const ArxLevelAnchor& anchor) noexcept;
  [[nodiscard]] LevelResult<AnchorIndex> addAnchor(const ArxLevelAnchor& anchor) noexcept;
  [[nodiscard]] LevelResult<void> removeAnchor(AnchorIndex index) noexcept;
  [[nodiscard]] LevelResult<void> setAnchorConnection(AnchorConnectionIndex index,
                                                      ArxLevelAnchorConnection connection) noexcept;
  [[nodiscard]] LevelResult<AnchorConnectionIndex> addAnchorConnection(ArxLevelAnchorConnection connection) noexcept;
  [[nodiscard]] LevelResult<void> removeAnchorConnection(AnchorConnectionIndex index) noexcept;
  [[nodiscard]] LevelResult<void> replaceAnchors(const ArxLevelAnchorsInput& anchors) noexcept;
  void clearAnchors() noexcept;

  [[nodiscard]] LevelResult<void> setNavSurface(const ArxLevelNavSurfaceInput& surface) noexcept;
  void clearNavSurface() noexcept;

  // --- Scene ---

  [[nodiscard]] LevelResult<void> setLight(LightIndex index, const ArxLevelLight& light) noexcept;
  [[nodiscard]] LevelResult<LightIndex> addLight(const ArxLevelLight& light) noexcept;
  [[nodiscard]] LevelResult<void> removeLight(LightIndex index) noexcept;

  [[nodiscard]] LevelResult<void> setPlayerSpawn(const ArxLevelPlayerSpawn& spawn) noexcept;
  void clearPlayerSpawn() noexcept;
  [[nodiscard]] LevelResult<void> setEntity(EntityIndex index, const ArxLevelEntity& entity) noexcept;
  [[nodiscard]] LevelResult<EntityIndex> addEntity(const ArxLevelEntity& entity) noexcept;
  [[nodiscard]] LevelResult<void> removeEntity(EntityIndex index) noexcept;
  [[nodiscard]] LevelResult<void> setFog(FogIndex index, const ArxLevelFog& fog) noexcept;
  [[nodiscard]] LevelResult<FogIndex> addFog(const ArxLevelFog& fog) noexcept;
  [[nodiscard]] LevelResult<void> removeFog(FogIndex index) noexcept;
  [[nodiscard]] LevelResult<void> setZone(ZoneIndex index, const ArxLevelZoneInput& zone) noexcept;
  [[nodiscard]] LevelResult<ZoneIndex> addZone(const ArxLevelZoneInput& zone) noexcept;
  [[nodiscard]] LevelResult<void> removeZone(ZoneIndex index) noexcept;
  [[nodiscard]] LevelResult<void> setPath(PathIndex index, const ArxLevelPathInput& path) noexcept;
  [[nodiscard]] LevelResult<PathIndex> addPath(const ArxLevelPathInput& path) noexcept;
  [[nodiscard]] LevelResult<void> removePath(PathIndex index) noexcept;

  // --- Generation ---

  [[nodiscard]] LevelResult<void> generateNavSurface() noexcept;
  [[nodiscard]] LevelResult<void> generateNavSurface(const NavSurfaceGenOptions& options) noexcept;
  [[nodiscard]] LevelResult<void> setNavSurfaceFromFloor() noexcept;
  [[nodiscard]] LevelResult<void> setNavSurfaceFromFloor(const NavSurfaceSourceOptions& options) noexcept;
  [[nodiscard]] LevelResult<void> pruneNavSurfaceIslands() noexcept;
  [[nodiscard]] LevelResult<void> pruneNavSurfaceIslands(const NavSurfacePruneOptions& options) noexcept;
  [[nodiscard]] LevelResult<void> generateAnchors() noexcept;
  [[nodiscard]] LevelResult<void> generateAnchors(const AnchorGenOptions& options) noexcept;
  [[nodiscard]] LevelResult<void> generateAnchorConnections() noexcept;
  [[nodiscard]] LevelResult<void> generateAnchorConnections(const AnchorConnectionGenOptions& options) noexcept;
  [[nodiscard]] LevelResult<void> pruneAnchorIslands() noexcept;
  [[nodiscard]] LevelResult<void> pruneAnchorIslands(const AnchorPruneOptions& options) noexcept;
  [[nodiscard]] LevelResult<void> generateRoomDistances() noexcept;
  [[nodiscard]] LevelResult<void> generateRoomDistances(const RoomDistanceGenOptions& options) noexcept;
  [[nodiscard]] LevelResult<void> generateStaticLighting() noexcept;
  [[nodiscard]] LevelResult<void> generateStaticLighting(const StaticLightingGenOptions& options) noexcept;
  [[nodiscard]] LevelResult<void> generateMinimap() noexcept;
  [[nodiscard]] LevelResult<void> generateMinimap(const MinimapGenerationOptions& options) noexcept;

 private:
  [[nodiscard]] static ArxLevelVertex vertexAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxLevelFace faceAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxTextureView textureAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxLevelRoom roomAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxLevelPortal portalAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxLevelRoomDistance roomDistanceAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxLevelAnchor anchorAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxLevelAnchorConnection anchorConnectionAt(const void* owner, std::size_t,
                                                                   std::size_t index) noexcept;
  [[nodiscard]] static ArxLevelVertex navSurfaceVertexAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxLevelNavSurfaceTriangle navSurfaceTriangleAt(const void* owner, std::size_t,
                                                                       std::size_t index) noexcept;
  [[nodiscard]] static ArxLevelLight lightAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxLevelEntity entityAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxLevelFog fogAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxLevelZone zoneAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxVector2 zonePerimeterAt(const void* owner, std::size_t zone, std::size_t index) noexcept;
  [[nodiscard]] static ArxLevelPath pathAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxLevelPathNode pathNodeAt(const void* owner, std::size_t path, std::size_t index) noexcept;

  struct Data;

  std::unique_ptr<Data> data_;

  friend struct level_debug::LevelDebugAccess;
};

}  // namespace pistoris
