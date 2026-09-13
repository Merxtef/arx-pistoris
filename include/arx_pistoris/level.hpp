// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/flags.h"
#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/math.hpp"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/level/types.h"
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
    // Arx-unit projection offset
    ArxVector2 projection_offset{};
    ArxColor3 fill_color{};
  };

  struct GameMinimapRenderOptions {
    // Arx-unit projection offset
    ArxVector2 projection_offset{};
    ArxColor3 fill_color{};
    ArxColor3 border_color{1.0f, 1.0f, 1.0f};
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

  struct NativeBakeOptions {
    // Used when dlf_scene_path is empty
    std::string_view level_name = {};
    NativeTextureBakeOptions textures = {};
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
  };

  // --- Lifetime ---

  Level();
  ~Level();

  Level(const Level& other);
  Level(Level&& other) = delete;
  Level& operator=(const Level& other);
  Level& operator=(Level&& other) = delete;

  void swap(Level& other) noexcept;
  friend void swap(Level& first, Level& second) noexcept { first.swap(second); }
  void reset();

  // --- Conversion ---

  [[nodiscard]] static ArxReturnCode importNative(Level& out, const fts::Data& fts, const llf::Data* llf = nullptr,
                                                  const dlf::Data* dlf = nullptr,
                                                  std::vector<std::string>* texture_source_paths = nullptr) noexcept;
  [[nodiscard]] static ArxReturnCode importGlb(Level& out, std::span<const std::uint8_t> data) noexcept;
  [[nodiscard]] static ArxReturnCode importGlb(Level& out, std::span<const std::uint8_t> data,
                                               const GlbImportOptions& options, ArxLevelGlbImportInfo* info = nullptr,
                                               std::vector<std::string>* texture_source_paths = nullptr) noexcept;

  [[nodiscard]] ArxReturnCode exportGlb(std::vector<std::uint8_t>& out) const noexcept;
  [[nodiscard]] ArxReturnCode exportGlb(std::vector<std::uint8_t>& out, const GlbExportOptions& options) const noexcept;
  [[nodiscard]] ArxReturnCode exportGlb(std::vector<std::uint8_t>& out, std::span<const Model* const> model_previews,
                                        ArxLevelModelPreviewReport* report = nullptr) const noexcept;
  [[nodiscard]] ArxReturnCode exportGlb(std::vector<std::uint8_t>& out, std::span<const Model* const> model_previews,
                                        const GlbExportOptions& options,
                                        ArxLevelModelPreviewReport* report = nullptr) const noexcept;
  [[nodiscard]] ArxReturnCode bakeNativeBundle(const NativeBakeOptions& options, NativeLevelBundle& out) const noexcept;
  [[nodiscard]] ArxReturnCode bakeDlf(const DlfBakeOptions& options, dlf::Data& out) const noexcept;

  // --- Validation ---

  [[nodiscard]] ArxReturnCode validate() const noexcept;
  [[nodiscard]] ArxReturnCode validateMesh() const noexcept;
  [[nodiscard]] ArxReturnCode validateVertices() const noexcept;
  [[nodiscard]] ArxReturnCode validateTextures() const noexcept;
  [[nodiscard]] ArxReturnCode validateFaces() const noexcept;
  [[nodiscard]] ArxReturnCode validateFaceRooms() const noexcept;
  [[nodiscard]] ArxReturnCode validateCornerColors() const noexcept;
  [[nodiscard]] ArxReturnCode validateRooms() const noexcept;
  [[nodiscard]] ArxReturnCode validatePortals() const noexcept;
  [[nodiscard]] ArxReturnCode validateRoomDistances() const noexcept;
  [[nodiscard]] ArxReturnCode validateNavSurface() const noexcept;
  [[nodiscard]] ArxReturnCode validateAnchors() const noexcept;
  [[nodiscard]] ArxReturnCode validateAnchorConnections() const noexcept;
  [[nodiscard]] ArxReturnCode validateLights() const noexcept;
  [[nodiscard]] ArxReturnCode validatePlayerSpawn() const noexcept;
  [[nodiscard]] ArxReturnCode validateEntities() const noexcept;
  [[nodiscard]] ArxReturnCode validateFogs() const noexcept;
  [[nodiscard]] ArxReturnCode validateZones() const noexcept;
  [[nodiscard]] ArxReturnCode validatePaths() const noexcept;
  [[nodiscard]] ArxReturnCode validateMinimap() const noexcept;
  [[nodiscard]] ArxReturnCode validateLoadingScreen() const noexcept;

  // --- Resource data ---

  [[nodiscard]] std::string_view resourcePath() const noexcept;
  [[nodiscard]] ArxReturnCode setResourcePath(std::string_view resource_path) noexcept;

  // --- Images ---

  [[nodiscard]] MinimapView minimap() const noexcept;
  [[nodiscard]] ArxEncodedImageView loadingScreen() const noexcept;
  [[nodiscard]] ArxReturnCode setMinimap(ArxEncodedImageView encoded_image, ArxRect world_xz_bounds) noexcept;
  [[nodiscard]] ArxReturnCode setMinimapFromProjection(ArxEncodedImageView encoded_image,
                                                       ArxVector2 projection_offset) noexcept;
  void clearMinimap() noexcept;
  [[nodiscard]] ArxReturnCode renderMinimapPng(const MinimapRenderOptions& options,
                                               std::vector<std::uint8_t>& out) const noexcept;
  [[nodiscard]] ArxReturnCode renderGameMinimapPng(const GameMinimapRenderOptions& options,
                                                   std::vector<std::uint8_t>& out) const noexcept;
  [[nodiscard]] ArxReturnCode renderCompactMinimapPng(ArxVector2& out_projection_offset,
                                                      std::vector<std::uint8_t>& out) const noexcept;
  [[nodiscard]] ArxReturnCode setLoadingScreen(ArxEncodedImageView encoded_image) noexcept;
  void clearLoadingScreen() noexcept;
  [[nodiscard]] ArxReturnCode renderLoadingScreenPng(std::vector<std::uint8_t>& out) const noexcept;
  [[nodiscard]] ArxReturnCode renderFullscreenLoadingScreenPng(std::vector<std::uint8_t>& out) const noexcept;
  [[nodiscard]] ArxReturnCode transcodeLoadingScreenPng(std::vector<std::uint8_t>& out) const noexcept;

  // --- Inspection ---

  [[nodiscard]] std::optional<ArxAabb> bounds() const;
  [[nodiscard]] std::optional<ArxAabb> referencedBounds() const;

  [[nodiscard]] std::size_t vertexCount() const noexcept;
  [[nodiscard]] std::size_t faceCount() const noexcept;
  [[nodiscard]] std::size_t textureCount() const noexcept;
  [[nodiscard]] std::size_t roomCount() const noexcept;
  [[nodiscard]] std::size_t portalCount() const noexcept;
  [[nodiscard]] std::size_t roomDistanceCount() const noexcept;
  [[nodiscard]] std::size_t anchorCount() const noexcept;
  [[nodiscard]] std::size_t anchorConnectionCount() const noexcept;
  [[nodiscard]] std::size_t lightCount() const noexcept;
  [[nodiscard]] std::size_t entityCount() const noexcept;
  [[nodiscard]] std::size_t fogCount() const noexcept;
  [[nodiscard]] std::size_t zoneCount() const noexcept;
  [[nodiscard]] std::size_t pathCount() const noexcept;

  [[nodiscard]] ArxReturnCode copyVertices(std::size_t offset, std::size_t count,
                                           ArxLevelVertex* out_vertices) const noexcept;
  [[nodiscard]] ArxReturnCode copyFaces(std::size_t offset, std::size_t count, ArxLevelFace* out_faces) const noexcept;
  [[nodiscard]] ArxReturnCode copyTextureViews(std::size_t offset, std::size_t count,
                                               ArxTextureView* out_views) const noexcept;
  [[nodiscard]] ArxReturnCode copyRooms(std::size_t offset, std::size_t count, ArxLevelRoom* out_rooms) const noexcept;
  [[nodiscard]] ArxReturnCode copyPortals(std::size_t offset, std::size_t count,
                                          ArxLevelPortal* out_portals) const noexcept;
  [[nodiscard]] ArxReturnCode copyRoomDistances(std::size_t offset, std::size_t count,
                                                ArxLevelRoomDistance* out_distances) const noexcept;
  [[nodiscard]] ArxReturnCode copyAnchors(std::size_t offset, std::size_t count,
                                          ArxLevelAnchor* out_anchors) const noexcept;
  [[nodiscard]] ArxReturnCode copyAnchorConnections(std::size_t offset, std::size_t count,
                                                    ArxLevelAnchorConnection* out_connections) const noexcept;
  [[nodiscard]] ArxLevelNavSurfaceInfo navSurfaceInfo() const noexcept;
  [[nodiscard]] ArxReturnCode copyNavSurfaceVertices(std::size_t offset, std::size_t count,
                                                     ArxLevelVertex* out_vertices) const noexcept;
  [[nodiscard]] ArxReturnCode copyNavSurfaceTriangles(std::size_t offset, std::size_t count,
                                                      ArxLevelNavSurfaceTriangle* out_triangles) const noexcept;
  [[nodiscard]] ArxReturnCode copyLights(std::size_t offset, std::size_t count,
                                         ArxLevelLight* out_lights) const noexcept;
  [[nodiscard]] ArxLevelPlayerSpawn playerSpawn() const noexcept;
  [[nodiscard]] ArxReturnCode copyEntities(std::size_t offset, std::size_t count,
                                           ArxLevelEntity* out_entities) const noexcept;
  [[nodiscard]] ArxReturnCode copyFogs(std::size_t offset, std::size_t count, ArxLevelFog* out_fogs) const noexcept;
  [[nodiscard]] ArxReturnCode copyZones(std::size_t offset, std::size_t count, ArxLevelZone* out_zones) const noexcept;
  [[nodiscard]] ArxReturnCode copyZonePerimeter(ZoneIndex zone, std::size_t offset, std::size_t count,
                                                ArxVector2* out_points) const noexcept;
  [[nodiscard]] ArxReturnCode copyPaths(std::size_t offset, std::size_t count, ArxLevelPath* out_paths) const noexcept;
  [[nodiscard]] ArxReturnCode copyPathNodes(PathIndex path, std::size_t offset, std::size_t count,
                                            ArxLevelPathNode* out_nodes) const noexcept;
  [[nodiscard]] ArxReturnCode roomDistance(RoomIndex room_a, RoomIndex room_b, std::uint8_t& out_has_distance,
                                           ArxLevelRoomDistance& out_distance) const noexcept;

  // --- Mesh editing ---

  [[nodiscard]] ArxReturnCode setVertex(VertexIndex index, ArxLevelVertex vertex) noexcept;
  [[nodiscard]] ArxReturnCode addVertex(ArxLevelVertex vertex, VertexIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode addVertices(const ArxLevelVertex* vertices, std::size_t count,
                                          VertexIndex& out_first_index) noexcept;
  [[nodiscard]] ArxReturnCode setFace(FaceIndex index, const ArxLevelFace& face) noexcept;
  [[nodiscard]] ArxReturnCode addFace(const ArxLevelFace& face, FaceIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeFace(FaceIndex index) noexcept;
  [[nodiscard]] ArxReturnCode compactVertices(std::size_t* removed = nullptr) noexcept;
  [[nodiscard]] ArxReturnCode compactTextures(std::size_t* removed = nullptr) noexcept;
  [[nodiscard]] ArxReturnCode rebaseTexturePaths(std::string_view directory) noexcept;
  [[nodiscard]] ArxReturnCode weldVertices() noexcept;
  [[nodiscard]] ArxReturnCode weldVertices(const VertexWeldOptions& options) noexcept;
  [[nodiscard]] ArxReturnCode setTexture(TextureIndex index, const ArxTextureView& texture) noexcept;
  [[nodiscard]] ArxReturnCode addTexture(const ArxTextureView& texture, TextureIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode setTextureImage(TextureIndex index, ArxEncodedImageView encoded_image) noexcept;
  [[nodiscard]] ArxReturnCode clearTextureImage(TextureIndex index) noexcept;
  [[nodiscard]] ArxReturnCode setFaceRoom(FaceIndex face, RoomIndex room) noexcept;
  [[nodiscard]] ArxReturnCode setCornerColor(FaceIndex face, std::uint8_t corner, ArxColor3 color) noexcept;
  void clearCornerColors() noexcept;
  [[nodiscard]] ArxReturnCode replaceMesh(const ArxLevelMeshInput& mesh) noexcept;
  void clearMesh() noexcept;

  // --- Rooms ---

  [[nodiscard]] ArxReturnCode setRoom(RoomIndex index, const ArxLevelRoom& room) noexcept;
  [[nodiscard]] ArxReturnCode addRoom(const ArxLevelRoom& room, RoomIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeRoom(RoomIndex index) noexcept;
  [[nodiscard]] ArxReturnCode setPortal(PortalIndex index, const ArxLevelPortal& portal) noexcept;
  [[nodiscard]] ArxReturnCode addPortal(const ArxLevelPortal& portal, PortalIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removePortal(PortalIndex index) noexcept;
  [[nodiscard]] ArxReturnCode setRoomDistance(const ArxLevelRoomDistance& distance) noexcept;
  [[nodiscard]] ArxReturnCode replaceRoomDistances(const ArxLevelRoomDistance* distances, std::size_t count) noexcept;
  void clearRoomDistances() noexcept;

  // --- Navigation ---

  [[nodiscard]] ArxReturnCode setAnchor(AnchorIndex index, const ArxLevelAnchor& anchor) noexcept;
  [[nodiscard]] ArxReturnCode addAnchor(const ArxLevelAnchor& anchor, AnchorIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeAnchor(AnchorIndex index) noexcept;
  [[nodiscard]] ArxReturnCode setAnchorConnection(AnchorConnectionIndex index,
                                                  ArxLevelAnchorConnection connection) noexcept;
  [[nodiscard]] ArxReturnCode addAnchorConnection(ArxLevelAnchorConnection connection,
                                                  AnchorConnectionIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeAnchorConnection(AnchorConnectionIndex index) noexcept;
  [[nodiscard]] ArxReturnCode replaceAnchors(const ArxLevelAnchorsInput& anchors) noexcept;
  void clearAnchors() noexcept;

  [[nodiscard]] ArxReturnCode setNavSurface(const ArxLevelNavSurfaceInput& surface) noexcept;
  void clearNavSurface() noexcept;

  // --- Scene ---

  [[nodiscard]] ArxReturnCode setLight(LightIndex index, const ArxLevelLight& light) noexcept;
  [[nodiscard]] ArxReturnCode addLight(const ArxLevelLight& light, LightIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeLight(LightIndex index) noexcept;

  [[nodiscard]] ArxReturnCode setPlayerSpawn(const ArxLevelPlayerSpawn& spawn) noexcept;
  void clearPlayerSpawn() noexcept;
  [[nodiscard]] ArxReturnCode setEntity(EntityIndex index, const ArxLevelEntity& entity) noexcept;
  [[nodiscard]] ArxReturnCode addEntity(const ArxLevelEntity& entity, EntityIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeEntity(EntityIndex index) noexcept;
  [[nodiscard]] ArxReturnCode setFog(FogIndex index, const ArxLevelFog& fog) noexcept;
  [[nodiscard]] ArxReturnCode addFog(const ArxLevelFog& fog, FogIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeFog(FogIndex index) noexcept;
  [[nodiscard]] ArxReturnCode setZone(ZoneIndex index, const ArxLevelZoneInput& zone) noexcept;
  [[nodiscard]] ArxReturnCode addZone(const ArxLevelZoneInput& zone, ZoneIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeZone(ZoneIndex index) noexcept;
  [[nodiscard]] ArxReturnCode setPath(PathIndex index, const ArxLevelPathInput& path) noexcept;
  [[nodiscard]] ArxReturnCode addPath(const ArxLevelPathInput& path, PathIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removePath(PathIndex index) noexcept;

  // --- Generation ---

  [[nodiscard]] ArxReturnCode generateNavSurface() noexcept;
  [[nodiscard]] ArxReturnCode generateNavSurface(const NavSurfaceGenOptions& options) noexcept;
  [[nodiscard]] ArxReturnCode setNavSurfaceFromFloor() noexcept;
  [[nodiscard]] ArxReturnCode setNavSurfaceFromFloor(const NavSurfaceSourceOptions& options) noexcept;
  [[nodiscard]] ArxReturnCode pruneNavSurfaceIslands() noexcept;
  [[nodiscard]] ArxReturnCode pruneNavSurfaceIslands(const NavSurfacePruneOptions& options) noexcept;
  [[nodiscard]] ArxReturnCode generateAnchors() noexcept;
  [[nodiscard]] ArxReturnCode generateAnchors(const AnchorGenOptions& options) noexcept;
  [[nodiscard]] ArxReturnCode generateAnchorConnections() noexcept;
  [[nodiscard]] ArxReturnCode generateAnchorConnections(const AnchorConnectionGenOptions& options) noexcept;
  [[nodiscard]] ArxReturnCode pruneAnchorIslands() noexcept;
  [[nodiscard]] ArxReturnCode pruneAnchorIslands(const AnchorPruneOptions& options) noexcept;
  [[nodiscard]] ArxReturnCode generateRoomDistances() noexcept;
  [[nodiscard]] ArxReturnCode generateRoomDistances(const RoomDistanceGenOptions& options) noexcept;
  [[nodiscard]] ArxReturnCode generateStaticLighting() noexcept;
  [[nodiscard]] ArxReturnCode generateStaticLighting(const StaticLightingGenOptions& options) noexcept;
  [[nodiscard]] ArxReturnCode generateMinimap() noexcept;
  [[nodiscard]] ArxReturnCode generateMinimap(const MinimapGenerationOptions& options) noexcept;

 private:
  ArxReturnCode renderMinimapProjectionPng(const MinimapRenderOptions& options, std::optional<ArxColor3> border_color,
                                           std::vector<std::uint8_t>& out) const;

  struct Data;

  std::unique_ptr<Data> data_;

  friend struct level_debug::LevelDebugAccess;
};

}  // namespace pistoris
