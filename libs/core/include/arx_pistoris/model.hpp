// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/animation.hpp"
#include "arx_pistoris/base/image.hpp"
#include "arx_pistoris/base/indexed_view.hpp"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/glb/location.hpp"
#include "arx_pistoris/model/bake.hpp"
#include "arx_pistoris/model/glb.hpp"
#include "arx_pistoris/model/location.hpp"
#include "arx_pistoris/model/obj.hpp"
#include "arx_pistoris/model/obj_location.hpp"
#include "arx_pistoris/model/types.h"
#include "arx_pistoris/native/location.hpp"
#include "arx_pistoris/native/text.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

struct ArxAnimationConversionReport;

namespace pistoris {

inline constexpr FaceType kModelFaceBitsAll = ARX_MODEL_FACE_BITS_ALL;

namespace ftl {
struct Data;
}

struct AnimationSoundFile;
struct AnimationSoundSourceReference;
struct ModelGlbImport;
class Level;

// Collection indices are current zero-based positions, not persistent identities
// Selection IDs remain stable within the current Model state until that selection is removed
// Non-const calls invalidate collection indices and borrowed views
class Model {
 public:
  struct VerticesViewTag;
  struct FacesViewTag;
  struct TexturesViewTag;
  struct BonesViewTag;
  struct ActionPointsViewTag;
  struct SelectionIdsViewTag;
  struct SelectionVerticesViewTag;
  struct SelectionBonesViewTag;
  struct SelectionActionPointsViewTag;

  using VerticesView = IndexedView<ArxModelVertex, VerticesViewTag>;
  using FacesView = IndexedView<ArxModelFace, FacesViewTag>;
  using TexturesView = IndexedView<ArxTextureView, TexturesViewTag>;
  using BonesView = IndexedView<ArxModelBone, BonesViewTag>;
  using ActionPointsView = IndexedView<ArxModelActionPoint, ActionPointsViewTag>;
  using SelectionIdsView = IndexedView<SelectionId, SelectionIdsViewTag>;
  using SelectionVerticesView = IndexedView<VertexIndex, SelectionVerticesViewTag>;
  using SelectionBonesView = IndexedView<BoneIndex, SelectionBonesViewTag>;
  using SelectionActionPointsView = IndexedView<ActionPointIndex, SelectionActionPointsViewTag>;
  struct GlbImportOptions {
    // Range [1, 1000]
    float arx_units_per_glb_unit = 10.0f;
  };

  struct GlbExportOptions {
    // Range [1, 1000]
    float arx_units_per_glb_unit = 10.0f;
  };

  struct LevelPreviewGlbOptions {
    // Range [1, 1000]
    float arx_units_per_glb_unit = 100.0f;
    // Empty exports literal <class-path>
    std::string_view class_path;
    // Empty exports literal asset
    std::string_view asset_name;
  };

  // Enable at least one operation
  struct ReferenceOptions {
    // Copy exact bone positions by index
    bool snap_bone_positions = false;
    // Replace bone selection memberships by selection name
    bool copy_bone_selection_memberships = false;
    // Replace action-point memberships by name and occurrence
    bool copy_action_point_selections = false;
  };

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

  struct InventoryIconView {
    ArxEncodedImageView encoded_image;
    std::uint8_t width_slots = 0;
    std::uint8_t height_slots = 0;
  };

  struct InventoryIconSetOptions {
    // Missing dimensions derive from the image; range [1, 3] explicit
    std::optional<std::uint8_t> width_slots;
    std::optional<std::uint8_t> height_slots;
  };

  enum class InventoryIconLayout : std::uint8_t {
    kCenter = 0,
    kTopLeft,
    kTopRight,
    kBottomLeft,
    kBottomRight,
    kStretch,
  };

  struct InventoryIconRenderOptions {
    // Both missing use the stored footprint; one missing derives from the explicit dimension
    std::optional<std::uint8_t> width_slots;
    std::optional<std::uint8_t> height_slots;
    InventoryIconLayout layout = InventoryIconLayout::kCenter;
    ImageFormat format = ImageFormat::kPng;
  };

  struct FacesOutput {
    std::optional<std::span<std::uint32_t>> vertex_indices = {};
    std::optional<std::span<float>> uvs = {};
    std::optional<std::span<float>> corner_normals = {};
    std::optional<std::span<TextureIndex>> textures = {};
    std::optional<std::span<float>> transvals = {};
    std::optional<std::span<float>> face_normals = {};
    std::optional<std::span<FaceType>> flags = {};
  };

  // --- Lifetime ---

  Model();
  ~Model();

  Model(const Model& other);
  Model(Model&& other) noexcept;
  Model& operator=(const Model& other);
  Model& operator=(Model&& other) noexcept;

  void swap(Model& other) noexcept;
  friend void swap(Model& first, Model& second) noexcept { first.swap(second); }
  [[nodiscard]] ModelResult<void> reset() noexcept;

  // --- Conversion ---

  [[nodiscard]] static FtlResult<Model> importNative(const ftl::Data& native,
                                                     std::vector<std::string>* texture_source_paths = nullptr,
                                                     NativeTextMode text_mode = NativeTextMode::kAuto) noexcept;
  [[nodiscard]] static ObjResult<Model> importObj(std::string_view obj, std::string_view mtl = {},
                                                  std::vector<std::string>* texture_source_paths = nullptr) noexcept;
  [[nodiscard]] static ObjResult<Model> importObj(std::string_view obj,
                                                  std::span<const ObjMaterialLibraryView> material_libraries,
                                                  std::vector<std::string>* texture_source_paths = nullptr) noexcept;
  [[nodiscard]] static GlbResult<Model> importGlb(std::span<const std::uint8_t> data) noexcept;
  [[nodiscard]] static GlbResult<Model> importGlb(std::span<const std::uint8_t> data, const GlbImportOptions& options,
                                                  std::vector<std::string>* texture_source_paths = nullptr) noexcept;
  [[nodiscard]] static GlbResult<ModelGlbImport> importGlbWithAnimations(
      std::span<const std::uint8_t> data, ArxAnimationConversionReport* report = nullptr) noexcept;
  [[nodiscard]] static GlbResult<ModelGlbImport> importGlbWithAnimations(
      std::span<const std::uint8_t> data, const GlbImportOptions& options,
      ArxAnimationConversionReport* report = nullptr, std::vector<std::string>* texture_source_paths = nullptr,
      std::vector<AnimationSoundSourceReference>* sound_sources = nullptr) noexcept;
  [[nodiscard]] ModelGlbExportResult<std::vector<std::uint8_t>> exportGlb() const noexcept;
  [[nodiscard]] ModelGlbExportResult<std::vector<std::uint8_t>> exportGlb(
      const GlbExportOptions& options) const noexcept;
  [[nodiscard]] ModelGlbExportResult<std::vector<std::uint8_t>> exportGlb(
      std::span<const Animation* const> animations, ArxAnimationConversionReport* report = nullptr) const noexcept;
  [[nodiscard]] ModelGlbExportResult<std::vector<std::uint8_t>> exportGlb(
      std::span<const Animation* const> animations, const GlbExportOptions& options,
      ArxAnimationConversionReport* report = nullptr) const noexcept;
  [[nodiscard]] ModelGlbExportResult<ModelGlbBundle> exportGlbBundle(
      std::span<const Animation* const> animations, const GlbExportOptions& options,
      ArxAnimationConversionReport* report = nullptr) const noexcept;
  [[nodiscard]] ModelResult<std::vector<std::uint8_t>> exportLevelPreviewGlb() const noexcept;
  [[nodiscard]] ModelResult<std::vector<std::uint8_t>> exportLevelPreviewGlb(
      const LevelPreviewGlbOptions& options) const noexcept;
  [[nodiscard]] ModelResult<ObjBundle> exportObj(std::string_view stem) const noexcept;
  [[nodiscard]] ModelResult<ObjBundle> exportObj(std::string_view stem, const ObjExportOptions& options) const noexcept;
  [[nodiscard]] ModelResult<NativeModelBundle> bakeNativeBundle(const NativeModelBakeOptions& options) const noexcept;

  // --- Validation ---

  [[nodiscard]] ModelResult<void> validate() const noexcept;
  [[nodiscard]] ModelResult<void> validateGeometry() const noexcept;
  [[nodiscard]] ModelResult<void> validateSkeleton() const noexcept;
  [[nodiscard]] ModelResult<void> validateActionPoints() const noexcept;
  [[nodiscard]] ModelResult<void> validateSelections() const noexcept;

  // --- Model operations ---

  [[nodiscard]] ModelResult<void> scale(float factor) noexcept;
  [[nodiscard]] ModelResult<void> rotate(ArxQuat rotation) noexcept;
  [[nodiscard]] ModelResult<void> translate(ArxVector3 offset) noexcept;
  [[nodiscard]] ModelResult<void> applyReference(const Model& reference, const ReferenceOptions& options) noexcept;
  [[nodiscard]] ModelResult<void> inferBoneSelectionMemberships() noexcept;

  // --- Resource data ---

  [[nodiscard]] std::string_view resourcePath() const noexcept;
  [[nodiscard]] ModelResult<void> setResourcePath(std::string_view resource_path) noexcept;
  [[nodiscard]] InventoryIconView inventoryIcon() const noexcept;
  [[nodiscard]] ModelResult<void> setInventoryIcon(ArxEncodedImageView encoded_image) noexcept;
  [[nodiscard]] ModelResult<void> setInventoryIcon(ArxEncodedImageView encoded_image,
                                                   const InventoryIconSetOptions& options) noexcept;
  void clearInventoryIcon() noexcept;
  [[nodiscard]] ModelResult<std::vector<std::uint8_t>> renderIcon(
      const InventoryIconRenderOptions& options) const noexcept;

  // --- Inspection ---

  [[nodiscard]] std::size_t vertexCount() const noexcept;
  [[nodiscard]] std::size_t faceCount() const noexcept;
  [[nodiscard]] std::size_t textureCount() const noexcept;
  [[nodiscard]] std::size_t boneCount() const noexcept;
  [[nodiscard]] std::size_t actionPointCount() const noexcept;
  [[nodiscard]] std::size_t selectionCount() const noexcept;

  [[nodiscard]] VerticesView vertices() const noexcept;
  [[nodiscard]] FacesView faces() const noexcept;
  [[nodiscard]] TexturesView textures() const noexcept;
  [[nodiscard]] BonesView bones() const noexcept;
  [[nodiscard]] ActionPointsView actionPoints() const noexcept;
  [[nodiscard]] ArxModelOrigin origin() const noexcept;
  [[nodiscard]] SelectionIdsView selectionIds() const noexcept;
  [[nodiscard]] ModelResult<ArxModelSelection> selection(SelectionId id) const noexcept;
  [[nodiscard]] ModelResult<SelectionVerticesView> selectionVertices(SelectionId id) const noexcept;
  [[nodiscard]] ModelResult<SelectionBonesView> selectionBones(SelectionId id) const noexcept;
  [[nodiscard]] ModelResult<SelectionActionPointsView> selectionActionPoints(SelectionId id) const noexcept;
  [[nodiscard]] ModelResult<bool> selectionIncludesOrigin(SelectionId id) const noexcept;

  // --- Geometry editing ---

  [[nodiscard]] ModelResult<void> setVertex(VertexIndex index, const ArxModelVertex& vertex) noexcept;
  [[nodiscard]] ModelResult<VertexIndex> addVertex(const ArxModelVertex& vertex) noexcept;
  [[nodiscard]] ModelResult<VertexIndex> addVertices(std::span<const ArxModelVertex> vertices) noexcept;
  [[nodiscard]] ModelResult<void> setFace(FaceIndex index, const ArxModelFace& face) noexcept;
  [[nodiscard]] ModelResult<FaceIndex> addFace(const ArxModelFace& face) noexcept;
  [[nodiscard]] ModelResult<void> removeFace(FaceIndex index) noexcept;
  [[nodiscard]] ModelResult<std::size_t> compactVertices() noexcept;
  [[nodiscard]] ModelResult<void> weldVertices() noexcept;
  [[nodiscard]] ModelResult<void> weldVertices(const VertexWeldOptions& options) noexcept;
  [[nodiscard]] ModelResult<std::size_t> compactTextures() noexcept;
  [[nodiscard]] ModelResult<void> rebaseTexturePaths(std::string_view directory) noexcept;
  [[nodiscard]] ModelResult<void> setTexture(TextureIndex index, const ArxTextureView& texture) noexcept;
  [[nodiscard]] ModelResult<TextureIndex> addTexture(const ArxTextureView& texture) noexcept;
  [[nodiscard]] ModelResult<void> setTexturePath(TextureIndex index, std::string_view path) noexcept;
  [[nodiscard]] ModelResult<void> setTextureExternalImageExtension(TextureIndex index,
                                                                   std::string_view extension) noexcept;
  [[nodiscard]] ModelResult<void> setTextureImage(TextureIndex index, ArxEncodedImageView encoded_image) noexcept;
  [[nodiscard]] ModelResult<void> clearTextureImage(TextureIndex index) noexcept;
  [[nodiscard]] ModelResult<void> replaceVertices(std::span<const float> positions) noexcept;
  void clearVertices() noexcept;
  [[nodiscard]] ModelResult<void> replaceFaces(std::span<const std::uint32_t> vertex_indices,
                                               std::span<const float> uvs, std::span<const float> corner_normals,
                                               std::span<const TextureIndex> textures, std::span<const float> transvals,
                                               std::span<const float> face_normals = {},
                                               std::span<const FaceType> flags = {}) noexcept;
  [[nodiscard]] ModelResult<void> copyFaces(const FacesOutput& output) const noexcept;
  [[nodiscard]] ModelResult<void> copyVertexPositions(std::span<float> output) const noexcept;
  [[nodiscard]] ModelResult<void> copyFaceTextures(std::span<TextureIndex> output) const noexcept;
  [[nodiscard]] ModelResult<void> copyVertexBones(std::span<BoneIndex> output) const noexcept;
  [[nodiscard]] ModelResult<void> copyActionPointBones(std::span<BoneIndex> output) const noexcept;
  [[nodiscard]] ModelResult<void> copyVertexSelectionMasks(std::span<std::uint64_t> output) const noexcept;
  [[nodiscard]] ModelResult<void> copyBoneSelectionMasks(std::span<std::uint64_t> output) const noexcept;
  [[nodiscard]] ModelResult<void> copyActionPointSelectionMasks(std::span<std::uint64_t> output) const noexcept;
  void clearFaces() noexcept;
  void clearTextures() noexcept;
  [[nodiscard]] ModelResult<void> replaceFaceTextures(std::span<const TextureIndex> textures) noexcept;
  [[nodiscard]] ModelResult<void> replaceVertexBones(std::span<const BoneIndex> bones) noexcept;
  [[nodiscard]] ModelResult<void> replaceActionPointBones(std::span<const BoneIndex> bones) noexcept;
  [[nodiscard]] ModelResult<void> replaceVertexSelectionMasks(std::span<const std::uint64_t> masks) noexcept;
  [[nodiscard]] ModelResult<void> replaceBoneSelectionMasks(std::span<const std::uint64_t> masks) noexcept;
  [[nodiscard]] ModelResult<void> replaceActionPointSelectionMasks(std::span<const std::uint64_t> masks) noexcept;
  [[nodiscard]] std::uint64_t activeSelectionMask() const noexcept;
  [[nodiscard]] ModelResult<std::uint64_t> selectionMask(SelectionId id) const noexcept;

  // --- Skeleton editing ---

  [[nodiscard]] ModelResult<void> setBone(BoneIndex index, const ArxModelBone& bone) noexcept;
  [[nodiscard]] ModelResult<BoneIndex> addBone(const ArxModelBone& bone) noexcept;
  [[nodiscard]] ModelResult<void> removeBone(BoneIndex index) noexcept;
  [[nodiscard]] ModelResult<void> setOrigin(ArxModelOrigin origin) noexcept;
  void clearBones() noexcept;

  // --- Action points ---

  [[nodiscard]] ModelResult<void> setActionPoint(ActionPointIndex index, const ArxModelActionPoint& point) noexcept;
  [[nodiscard]] ModelResult<ActionPointIndex> addActionPoint(const ArxModelActionPoint& point) noexcept;
  [[nodiscard]] ModelResult<void> removeActionPoint(ActionPointIndex index) noexcept;
  void clearActionPoints() noexcept;

  // --- Selections ---

  [[nodiscard]] ModelResult<SelectionId> addSelection(const ArxModelSelection& selection) noexcept;
  [[nodiscard]] ModelResult<void> setSelection(SelectionId id, const ArxModelSelection& selection) noexcept;
  [[nodiscard]] ModelResult<void> updateSelectionMembers(SelectionId id,
                                                         const ArxModelSelectionMembersInput& members) noexcept;
  [[nodiscard]] ModelResult<void> clearSelectionVertices(SelectionId id) noexcept;
  [[nodiscard]] ModelResult<void> clearSelectionBones(SelectionId id) noexcept;
  [[nodiscard]] ModelResult<void> clearSelectionActionPoints(SelectionId id) noexcept;
  [[nodiscard]] ModelResult<void> setSelectionIncludesOrigin(SelectionId id, bool includes) noexcept;
  [[nodiscard]] ModelResult<void> removeSelection(SelectionId id) noexcept;
  void clearSelections() noexcept;

 private:
  ArxReturnCode exportGlbInternal(std::span<const Animation* const> animations, const GlbExportOptions& options,
                                  ArxAnimationConversionReport* report, std::vector<std::uint8_t>& out,
                                  std::vector<AnimationSoundFile>* sound_files,
                                  ModelGlbExportLocation* failure_location) const;

  [[nodiscard]] static ArxModelVertex vertexAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxModelFace faceAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxTextureView textureAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxModelBone boneAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static ArxModelActionPoint actionPointAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static SelectionId selectionIdAt(const void* owner, std::size_t, std::size_t index) noexcept;
  [[nodiscard]] static VertexIndex selectionVertexAt(const void* owner, std::size_t selection,
                                                     std::size_t index) noexcept;
  [[nodiscard]] static BoneIndex selectionBoneAt(const void* owner, std::size_t selection, std::size_t index) noexcept;
  [[nodiscard]] static ActionPointIndex selectionActionPointAt(const void* owner, std::size_t selection,
                                                               std::size_t index) noexcept;

  struct Data;
  std::unique_ptr<Data> data_;

  friend class Ambiance;
  friend class Level;
};

struct ModelGlbImport {
  Model model;
  std::vector<Animation> animations;
};

}  // namespace pistoris
