// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/arx_math.hpp"
#include "arx_pistoris/flags.h"
#include "arx_pistoris/level/types.h"
#include "arx_pistoris/pistoris_types.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
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

// Zero-based positions in the current collection, not persistent object identities
// Any non-const Level call invalidates previously acquired indices and borrowed views
inline constexpr FaceType kLevelFaceBitsAll = ARX_LEVEL_FACE_BITS_ALL;
inline constexpr float kLevelMinXZ = 0.0f;
inline constexpr float kLevelMaxXZ = 16000.0f;
inline constexpr float kMinArxUnitsPerGlbUnit = 1.0f;
inline constexpr float kMaxArxUnitsPerGlbUnit = 1000.0f;

enum class NativeTexturePathMode : std::uint8_t {
  kPreserve,
  kRebase,
};

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
    float radius = 1.0e-4f;
    PositionWeldMetric metric = PositionWeldMetric::kEuclidean;
    DegenerateFacePolicy degenerate_faces = DegenerateFacePolicy::kPreserve;
  };

  struct NavSurfaceSourceOptions {
    // Offset applied above the generated navigation surface, -Y is up
    float clearance = kDefaultNavSurfaceClearance;
    // Minimum cosine against the Arx up direction accepted as support
    float support_min_up_cos = kDefaultNavSurfaceSupportMinUpCos;
    // Face flags excluded from support selection; NOPATH is always excluded by the selector
    FaceType support_ignore_flags = kDefaultNavSurfaceIgnoreFlags;
  };

  struct NavSurfaceGenOptions : NavSurfaceSourceOptions {
    // Navigation probe cylinder radius
    float radius = kDefaultAnchorRadius;
    // Signed navigation probe cylinder height in Arx coordinates, -Y is up
    float height = kDefaultAnchorHeight;
    // Maximum support correction during navigation probing, -Y is up
    float max_step_up = kDefaultNavSurfaceMaxStepUp;
  };

  struct NavSurfacePruneOptions {
    float min_component_area_ratio = 0.05f;
    double min_component_area = 0.0;
  };

  struct AnchorGenOptions {
    // Distance between anchor sampling points
    float sample_spacing = 100.0f;
    // Generated anchor cylinder radius
    float radius = kDefaultAnchorRadius;
    // Signed generated anchor cylinder height in Arx coordinates, -Y is up
    float height = kDefaultAnchorHeight;
  };

  struct AnchorPruneOptions {
    float min_component_anchor_ratio = 0.05f;
    std::uint32_t min_component_anchor_count = 1;
  };

  struct AnchorConnectionGenOptions {
    // Maximum XZ-projected distance between anchors considered for linking
    float max_distance = 150.0f;
    // Maximum cylinder traversal segment length
    float max_step_distance = 40.0f;
    // Maximum support correction per traversal step, -Y is up
    float max_step_up = 55.0f;
    // Multiplier applied to anchor radius during traversal checks
    float radius_scale = 0.9f;
    // Maximum traversal steps per candidate link
    int max_steps = 100;
  };

  struct RoomDistanceGenOptions {
    // Offset from portal plane to the per-room access point
    float portal_side_offset = kDefaultRoomDistancePortalOffset;
    // Grid spacing for room-distance sample nodes
    float sample_spacing = kDefaultRoomDistanceSampleSpacing;
    // Vertical offset from support surface to visibility graph node, -Y is up
    float sample_height_offset = kDefaultRoomDistanceSampleHeight;
    float max_link_distance = 0.0f;  // = 1.5 * sample_spacing
  };

  struct StaticLightingGenOptions {
    // Minimum generated corner color
    ArxColor3 ambient_color = kDefaultStaticLightingAmbientColor;
    // Original editor static light multiplier
    float global_factor = kDefaultStaticLightingGlobalFactor;
    // Apply per-corner normal response
    bool use_normals = true;
    // Test static geometry visibility for shadow casting
    bool use_shadows = true;
  };

  struct GlbUnitOptions {
    // Inclusive [kMinArxUnitsPerGlbUnit, kMaxArxUnitsPerGlbUnit]
    float arx_units_per_glb_unit = 100.0f;
  };

  struct GlbImportOptions : GlbUnitOptions {
    // Missing offset selects automatic 100-unit-aligned XZ placement
    std::optional<ArxVector3> arx_offset;
  };

  struct GlbExportOptions : GlbUnitOptions {
    ArxVector3 arx_offset = {};
  };

  struct GlbImportInfo {
    ArxVector3 applied_arx_offset = {};
  };

  struct NativeBakeOptions {
    std::string_view level_name;
    std::string_view texture_folder;
    NativeTexturePathMode texture_path_mode = NativeTexturePathMode::kPreserve;
    bool reconstruct_quads = true;
    bool include_texture_files = true;
    std::string_view dlf_scene_path = {};
  };

  struct NativeDlfBakeOptions {
    std::string_view level_name;
    ArxVector3 target_fts_offset = {};
    std::string_view dlf_scene_path = {};
  };

  Level();
  ~Level();

  Level(const Level& other);
  Level(Level&& other) = delete;
  Level& operator=(const Level& other);
  Level& operator=(Level&& other) = delete;

  void swap(Level& other) noexcept;
  friend void swap(Level& first, Level& second) noexcept { first.swap(second); }
  void reset();

  [[nodiscard]] static ArxReturnCode fromNative(Level& out, const fts::Data& fts, const llf::Data* llf = nullptr,
                                                const dlf::Data* dlf = nullptr);
  [[nodiscard]] static ArxReturnCode fromGlb(Level& out, std::span<const std::uint8_t> data);
  [[nodiscard]] static ArxReturnCode fromGlb(Level& out, std::span<const std::uint8_t> data,
                                             const GlbImportOptions& options, GlbImportInfo* info = nullptr);

  [[nodiscard]] ArxReturnCode exportGlb(std::vector<std::uint8_t>& out) const;
  [[nodiscard]] ArxReturnCode exportGlb(std::vector<std::uint8_t>& out, const GlbExportOptions& options) const;
  [[nodiscard]] ArxReturnCode bakeNativeBundle(const NativeBakeOptions& options, NativeLevelBundle& out) const;
  [[nodiscard]] ArxReturnCode bakeNativeDlf(const NativeDlfBakeOptions& options, dlf::Data& out) const;
  [[nodiscard]] ArxReturnCode validate() const;
  [[nodiscard]] ArxReturnCode validateMesh() const;
  [[nodiscard]] ArxReturnCode validateVertices() const;
  [[nodiscard]] ArxReturnCode validateTextures() const;
  [[nodiscard]] ArxReturnCode validateFaces() const;
  [[nodiscard]] ArxReturnCode validateFaceRooms() const;
  [[nodiscard]] ArxReturnCode validateCornerColors() const;
  [[nodiscard]] ArxReturnCode validateRooms() const;
  [[nodiscard]] ArxReturnCode validatePortals() const;
  [[nodiscard]] ArxReturnCode validateRoomDistances() const;
  [[nodiscard]] ArxReturnCode validateNavSurface() const;
  [[nodiscard]] ArxReturnCode validateAnchors() const;
  [[nodiscard]] ArxReturnCode validateAnchorConnections() const;
  [[nodiscard]] ArxReturnCode validateLights() const;
  [[nodiscard]] ArxReturnCode validatePlayerSpawn() const;
  [[nodiscard]] ArxReturnCode validateEntities() const;
  [[nodiscard]] ArxReturnCode validateFogs() const;
  [[nodiscard]] ArxReturnCode validateZones() const;
  [[nodiscard]] ArxReturnCode validatePaths() const;
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
                                               ArxLevelTextureView* out_views) const noexcept;
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
  [[nodiscard]] ArxReturnCode getRoomDistance(RoomIndex room_a, RoomIndex room_b, std::uint8_t& out_has_distance,
                                              ArxLevelRoomDistance& out_distance) const noexcept;

  [[nodiscard]] ArxReturnCode setVertex(VertexIndex index, ArxLevelVertex vertex) noexcept;
  [[nodiscard]] ArxReturnCode addVertex(ArxLevelVertex vertex, VertexIndex& out_index);
  [[nodiscard]] ArxReturnCode addVertices(const ArxLevelVertex* vertices, std::size_t count,
                                          VertexIndex& out_first_index);
  [[nodiscard]] ArxReturnCode setFace(FaceIndex index, const ArxLevelFace& face);
  [[nodiscard]] ArxReturnCode addFace(const ArxLevelFace& face, FaceIndex& out_index);
  [[nodiscard]] ArxReturnCode removeFace(FaceIndex index);
  [[nodiscard]] ArxReturnCode compactVertices(std::size_t* removed = nullptr);
  [[nodiscard]] ArxReturnCode compactTextures(std::size_t* removed = nullptr);
  [[nodiscard]] ArxReturnCode weldVertices();
  [[nodiscard]] ArxReturnCode weldVertices(const VertexWeldOptions& options);
  [[nodiscard]] ArxReturnCode setTexture(TextureIndex index, const ArxLevelTextureView& texture);
  [[nodiscard]] ArxReturnCode addTexture(const ArxLevelTextureView& texture, TextureIndex& out_index);
  [[nodiscard]] ArxReturnCode setTextureImage(TextureIndex index, ArxEncodedImageView encoded_image);
  [[nodiscard]] ArxReturnCode clearTextureImage(TextureIndex index) noexcept;
  [[nodiscard]] ArxReturnCode setFaceRoom(FaceIndex face, RoomIndex room) noexcept;
  [[nodiscard]] ArxReturnCode setCornerColor(FaceIndex face, std::uint8_t corner, ArxColor3 color);
  void clearCornerColors() noexcept;
  [[nodiscard]] ArxReturnCode replaceMesh(const ArxLevelMeshInput& mesh);
  void clearMesh() noexcept;

  [[nodiscard]] ArxReturnCode setRoom(RoomIndex index, const ArxLevelRoom& room);
  [[nodiscard]] ArxReturnCode addRoom(const ArxLevelRoom& room, RoomIndex& out_index);
  [[nodiscard]] ArxReturnCode removeRoom(RoomIndex index);
  [[nodiscard]] ArxReturnCode setPortal(PortalIndex index, const ArxLevelPortal& portal);
  [[nodiscard]] ArxReturnCode addPortal(const ArxLevelPortal& portal, PortalIndex& out_index);
  [[nodiscard]] ArxReturnCode removePortal(PortalIndex index);
  [[nodiscard]] ArxReturnCode setRoomDistance(const ArxLevelRoomDistance& distance);
  [[nodiscard]] ArxReturnCode replaceRoomDistances(const ArxLevelRoomDistance* distances, std::size_t count);
  void clearRoomDistances() noexcept;

  [[nodiscard]] ArxReturnCode setAnchor(AnchorIndex index, const ArxLevelAnchor& anchor);
  [[nodiscard]] ArxReturnCode addAnchor(const ArxLevelAnchor& anchor, AnchorIndex& out_index);
  [[nodiscard]] ArxReturnCode removeAnchor(AnchorIndex index);
  [[nodiscard]] ArxReturnCode setAnchorConnection(AnchorConnectionIndex index, ArxLevelAnchorConnection connection);
  [[nodiscard]] ArxReturnCode addAnchorConnection(ArxLevelAnchorConnection connection,
                                                  AnchorConnectionIndex& out_index);
  [[nodiscard]] ArxReturnCode removeAnchorConnection(AnchorConnectionIndex index);
  [[nodiscard]] ArxReturnCode replaceAnchors(const ArxLevelAnchorsInput& anchors);
  void clearAnchors() noexcept;

  [[nodiscard]] ArxReturnCode setNavSurface(const ArxLevelNavSurfaceInput& surface);
  void clearNavSurface() noexcept;

  [[nodiscard]] ArxReturnCode setLight(LightIndex index, const ArxLevelLight& light);
  [[nodiscard]] ArxReturnCode addLight(const ArxLevelLight& light, LightIndex& out_index);
  [[nodiscard]] ArxReturnCode removeLight(LightIndex index);

  [[nodiscard]] ArxReturnCode setPlayerSpawn(const ArxLevelPlayerSpawn& spawn);
  void clearPlayerSpawn() noexcept;
  [[nodiscard]] ArxReturnCode setEntity(EntityIndex index, const ArxLevelEntity& entity);
  [[nodiscard]] ArxReturnCode addEntity(const ArxLevelEntity& entity, EntityIndex& out_index);
  [[nodiscard]] ArxReturnCode removeEntity(EntityIndex index);
  [[nodiscard]] ArxReturnCode setFog(FogIndex index, const ArxLevelFog& fog);
  [[nodiscard]] ArxReturnCode addFog(const ArxLevelFog& fog, FogIndex& out_index);
  [[nodiscard]] ArxReturnCode removeFog(FogIndex index);
  [[nodiscard]] ArxReturnCode setZone(ZoneIndex index, const ArxLevelZoneInput& zone);
  [[nodiscard]] ArxReturnCode addZone(const ArxLevelZoneInput& zone, ZoneIndex& out_index);
  [[nodiscard]] ArxReturnCode removeZone(ZoneIndex index);
  [[nodiscard]] ArxReturnCode setPath(PathIndex index, const ArxLevelPathInput& path);
  [[nodiscard]] ArxReturnCode addPath(const ArxLevelPathInput& path, PathIndex& out_index);
  [[nodiscard]] ArxReturnCode removePath(PathIndex index);

  [[nodiscard]] ArxReturnCode generateNavSurface();
  [[nodiscard]] ArxReturnCode generateNavSurface(const NavSurfaceGenOptions& options);
  [[nodiscard]] ArxReturnCode setNavSurfaceFromFloor();
  [[nodiscard]] ArxReturnCode setNavSurfaceFromFloor(const NavSurfaceSourceOptions& options);
  [[nodiscard]] ArxReturnCode pruneNavSurfaceIslands();
  [[nodiscard]] ArxReturnCode pruneNavSurfaceIslands(const NavSurfacePruneOptions& options);
  [[nodiscard]] ArxReturnCode generateAnchors();
  [[nodiscard]] ArxReturnCode generateAnchors(const AnchorGenOptions& options);
  [[nodiscard]] ArxReturnCode generateAnchorConnections();
  [[nodiscard]] ArxReturnCode generateAnchorConnections(const AnchorConnectionGenOptions& options);
  [[nodiscard]] ArxReturnCode pruneAnchorIslands();
  [[nodiscard]] ArxReturnCode pruneAnchorIslands(const AnchorPruneOptions& options);
  [[nodiscard]] ArxReturnCode generateRoomDistances();
  [[nodiscard]] ArxReturnCode generateRoomDistances(const RoomDistanceGenOptions& options);
  [[nodiscard]] ArxReturnCode generateStaticLighting();
  [[nodiscard]] ArxReturnCode generateStaticLighting(const StaticLightingGenOptions& options);

 private:
  struct Data;

  std::unique_ptr<Data> data_;

  friend struct level_debug::LevelDebugAccess;
};

}  // namespace pistoris
