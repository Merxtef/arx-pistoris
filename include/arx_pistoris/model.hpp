// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#pragma once

#include "arx_pistoris/base/image.h"
#include "arx_pistoris/base/indices.h"
#include "arx_pistoris/base/status.h"
#include "arx_pistoris/model/types.h"
#include "arx_pistoris/native/text.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
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
struct NativeModelBundle;
struct NativeModelBakeOptions;
struct ModelGlbBundle;
struct ObjBundle;
struct ObjExportOptions;
struct ObjMaterialLibraryView;
class Animation;
class Level;

// Collection indices are current zero-based positions, not persistent identities
// Selection IDs remain stable within the current Model state until that selection is removed
// Non-const calls invalidate collection indices and borrowed views
class Model {
 public:
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
    bool snap_bone_origins = false;
    // Replace bone-origin memberships by selection name
    bool copy_bone_origin_selections = false;
    // Replace action-point memberships by name and occurrence
    bool copy_action_point_selections = false;
  };

  struct InventoryIconView {
    ArxEncodedImageView encoded_image;
    std::uint8_t width_slots = 0;
    std::uint8_t height_slots = 0;
  };

  struct InventoryIconSetOptions {
    // -1 derives, range [1, 3] explicit
    std::int8_t width_slots = -1;
    // -1 derives, range [1, 3] explicit
    std::int8_t height_slots = -1;
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
    // -1 derives, 0 uses stored value, range [1, 3] explicit
    std::int8_t width_slots = 0;
    // -1 derives, 0 uses stored value, range [1, 3] explicit
    std::int8_t height_slots = 0;
    InventoryIconLayout layout = InventoryIconLayout::kCenter;
  };

  // --- Lifetime ---

  Model();
  ~Model();

  Model(const Model& other);
  Model(Model&& other) = delete;
  Model& operator=(const Model& other);
  Model& operator=(Model&& other) = delete;

  void swap(Model& other) noexcept;
  friend void swap(Model& first, Model& second) noexcept { first.swap(second); }
  void reset();

  // --- Conversion ---

  [[nodiscard]] static ArxReturnCode importNative(Model& out, const ftl::Data& native,
                                                  std::vector<std::string>* texture_source_paths = nullptr,
                                                  NativeTextMode text_mode = NativeTextMode::kAuto) noexcept;
  [[nodiscard]] static ArxReturnCode importObj(Model& out, std::string_view obj, std::string_view mtl = {},
                                               std::vector<std::string>* texture_source_paths = nullptr) noexcept;
  [[nodiscard]] static ArxReturnCode importObj(Model& out, std::string_view obj,
                                               std::span<const ObjMaterialLibraryView> material_libraries,
                                               std::vector<std::string>* texture_source_paths = nullptr) noexcept;
  [[nodiscard]] static ArxReturnCode importGlb(Model& out, std::span<const std::uint8_t> data) noexcept;
  [[nodiscard]] static ArxReturnCode importGlb(Model& out, std::span<const std::uint8_t> data,
                                               const GlbImportOptions& options,
                                               std::vector<std::string>* texture_source_paths = nullptr) noexcept;
  [[nodiscard]] static ArxReturnCode importGlb(Model& out, std::vector<std::unique_ptr<Animation>>& out_animations,
                                               std::span<const std::uint8_t> data,
                                               ArxAnimationConversionReport* report = nullptr) noexcept;
  [[nodiscard]] static ArxReturnCode importGlb(
      Model& out, std::vector<std::unique_ptr<Animation>>& out_animations, std::span<const std::uint8_t> data,
      const GlbImportOptions& options, ArxAnimationConversionReport* report = nullptr,
      std::vector<std::string>* texture_source_paths = nullptr,
      std::vector<AnimationSoundSourceReference>* sound_sources = nullptr) noexcept;
  [[nodiscard]] ArxReturnCode exportGlb(std::vector<std::uint8_t>& out) const noexcept;
  [[nodiscard]] ArxReturnCode exportGlb(std::vector<std::uint8_t>& out, const GlbExportOptions& options) const noexcept;
  [[nodiscard]] ArxReturnCode exportGlb(std::vector<std::uint8_t>& out, std::span<const Animation* const> animations,
                                        ArxAnimationConversionReport* report = nullptr) const noexcept;
  [[nodiscard]] ArxReturnCode exportGlb(std::vector<std::uint8_t>& out, std::span<const Animation* const> animations,
                                        const GlbExportOptions& options,
                                        ArxAnimationConversionReport* report = nullptr) const noexcept;
  [[nodiscard]] ArxReturnCode exportGlbBundle(std::span<const Animation* const> animations,
                                              const GlbExportOptions& options, ArxAnimationConversionReport* report,
                                              ModelGlbBundle& out) const noexcept;
  [[nodiscard]] ArxReturnCode exportLevelPreviewGlb(std::vector<std::uint8_t>& out) const noexcept;
  [[nodiscard]] ArxReturnCode exportLevelPreviewGlb(std::vector<std::uint8_t>& out,
                                                    const LevelPreviewGlbOptions& options) const noexcept;
  [[nodiscard]] ArxReturnCode exportObj(std::string_view stem, ObjBundle& out) const noexcept;
  [[nodiscard]] ArxReturnCode exportObj(std::string_view stem, const ObjExportOptions& options,
                                        ObjBundle& out) const noexcept;
  [[nodiscard]] ArxReturnCode bakeNativeBundle(const NativeModelBakeOptions& options,
                                               NativeModelBundle& out) const noexcept;

  // --- Validation ---

  [[nodiscard]] ArxReturnCode validate() const noexcept;
  [[nodiscard]] ArxReturnCode validateMesh() const noexcept;
  [[nodiscard]] ArxReturnCode validateSkeleton() const noexcept;
  [[nodiscard]] ArxReturnCode validateActionPoints() const noexcept;
  [[nodiscard]] ArxReturnCode validateSelections() const noexcept;

  // --- Model operations ---

  [[nodiscard]] ArxReturnCode scale(float factor) noexcept;
  [[nodiscard]] ArxReturnCode rotate(ArxQuat rotation) noexcept;
  [[nodiscard]] ArxReturnCode translate(ArxVector3 offset) noexcept;
  [[nodiscard]] ArxReturnCode applyReference(const Model& reference, const ReferenceOptions& options) noexcept;
  [[nodiscard]] ArxReturnCode inferBoneOriginSelections() noexcept;

  // --- Resource data ---

  [[nodiscard]] std::string_view resourcePath() const noexcept;
  [[nodiscard]] ArxReturnCode setResourcePath(std::string_view resource_path) noexcept;
  [[nodiscard]] InventoryIconView inventoryIcon() const noexcept;
  [[nodiscard]] ArxReturnCode setInventoryIcon(ArxEncodedImageView encoded_image) noexcept;
  [[nodiscard]] ArxReturnCode setInventoryIcon(ArxEncodedImageView encoded_image,
                                               const InventoryIconSetOptions& options) noexcept;
  void clearInventoryIcon() noexcept;
  [[nodiscard]] ArxReturnCode renderIconPng(const InventoryIconRenderOptions& options,
                                            std::vector<std::uint8_t>& out) const noexcept;
  [[nodiscard]] ArxReturnCode renderIconBmp(const InventoryIconRenderOptions& options,
                                            std::vector<std::uint8_t>& out) const noexcept;

  // --- Inspection ---

  [[nodiscard]] std::size_t vertexCount() const noexcept;
  [[nodiscard]] std::size_t faceCount() const noexcept;
  [[nodiscard]] std::size_t textureCount() const noexcept;
  [[nodiscard]] std::size_t boneCount() const noexcept;
  [[nodiscard]] std::size_t actionPointCount() const noexcept;
  [[nodiscard]] std::size_t selectionCount() const noexcept;

  [[nodiscard]] ArxReturnCode copyVertices(std::size_t offset, std::size_t count,
                                           ArxModelVertex* out_vertices) const noexcept;
  [[nodiscard]] ArxReturnCode copyFaces(std::size_t offset, std::size_t count, ArxModelFace* out_faces) const noexcept;
  [[nodiscard]] ArxReturnCode copyTextureViews(std::size_t offset, std::size_t count,
                                               ArxTextureView* out_views) const noexcept;
  [[nodiscard]] ArxReturnCode copyBones(std::size_t offset, std::size_t count, ArxModelBone* out_bones) const noexcept;
  [[nodiscard]] ArxReturnCode copyActionPoints(std::size_t offset, std::size_t count,
                                               ArxModelActionPoint* out_points) const noexcept;
  [[nodiscard]] ArxModelOrigin origin() const noexcept;
  [[nodiscard]] ArxReturnCode copySelectionIds(std::size_t offset, std::size_t count,
                                               SelectionId* out_ids) const noexcept;
  [[nodiscard]] ArxReturnCode selection(SelectionId id, ArxModelSelection& out) const noexcept;
  [[nodiscard]] ArxReturnCode selectionVertexCount(SelectionId id, std::size_t& out_count) const noexcept;
  [[nodiscard]] ArxReturnCode selectionBoneCount(SelectionId id, std::size_t& out_count) const noexcept;
  [[nodiscard]] ArxReturnCode selectionActionPointCount(SelectionId id, std::size_t& out_count) const noexcept;
  [[nodiscard]] ArxReturnCode copySelectionVertices(SelectionId id, std::size_t offset, std::size_t count,
                                                    VertexIndex* out_vertices) const noexcept;
  [[nodiscard]] ArxReturnCode copySelectionBones(SelectionId id, std::size_t offset, std::size_t count,
                                                 BoneIndex* out_bones) const noexcept;
  [[nodiscard]] ArxReturnCode copySelectionActionPoints(SelectionId id, std::size_t offset, std::size_t count,
                                                        ActionPointIndex* out_points) const noexcept;
  [[nodiscard]] ArxReturnCode selectionIncludesOrigin(SelectionId id, bool& out_includes) const noexcept;

  // --- Mesh editing ---

  [[nodiscard]] ArxReturnCode setVertex(VertexIndex index, const ArxModelVertex& vertex) noexcept;
  [[nodiscard]] ArxReturnCode addVertex(const ArxModelVertex& vertex, VertexIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode addVertices(const ArxModelVertex* vertices, std::size_t count,
                                          VertexIndex& out_first_index) noexcept;
  [[nodiscard]] ArxReturnCode setFace(FaceIndex index, const ArxModelFace& face) noexcept;
  [[nodiscard]] ArxReturnCode addFace(const ArxModelFace& face, FaceIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeFace(FaceIndex index) noexcept;
  [[nodiscard]] ArxReturnCode compactVertices(std::size_t* removed = nullptr) noexcept;
  [[nodiscard]] ArxReturnCode compactTextures(std::size_t* removed = nullptr) noexcept;
  [[nodiscard]] ArxReturnCode rebaseTexturePaths(std::string_view directory) noexcept;
  [[nodiscard]] ArxReturnCode setTexture(TextureIndex index, const ArxTextureView& texture) noexcept;
  [[nodiscard]] ArxReturnCode addTexture(const ArxTextureView& texture, TextureIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode setTextureImage(TextureIndex index, ArxEncodedImageView encoded_image) noexcept;
  [[nodiscard]] ArxReturnCode clearTextureImage(TextureIndex index) noexcept;
  [[nodiscard]] ArxReturnCode replaceMesh(const ArxModelMeshInput& mesh) noexcept;
  void clearMesh() noexcept;

  // --- Skeleton editing ---

  [[nodiscard]] ArxReturnCode setBone(BoneIndex index, const ArxModelBone& bone) noexcept;
  [[nodiscard]] ArxReturnCode addBone(const ArxModelBone& bone, BoneIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeBone(BoneIndex index) noexcept;
  [[nodiscard]] ArxReturnCode replaceSkeleton(const ArxModelSkeletonInput& skeleton) noexcept;
  [[nodiscard]] ArxReturnCode setOrigin(ArxModelOrigin origin) noexcept;
  void clearSkeleton() noexcept;

  // --- Action points ---

  [[nodiscard]] ArxReturnCode setActionPoint(ActionPointIndex index, const ArxModelActionPoint& point) noexcept;
  [[nodiscard]] ArxReturnCode addActionPoint(const ArxModelActionPoint& point, ActionPointIndex& out_index) noexcept;
  [[nodiscard]] ArxReturnCode removeActionPoint(ActionPointIndex index) noexcept;
  [[nodiscard]] ArxReturnCode replaceActionPoints(const ArxModelActionPointsInput& points) noexcept;
  void clearActionPoints() noexcept;

  // --- Selections ---

  [[nodiscard]] ArxReturnCode addSelection(const ArxModelSelection& selection, SelectionId& out_id) noexcept;
  [[nodiscard]] ArxReturnCode setSelection(SelectionId id, const ArxModelSelection& selection) noexcept;
  [[nodiscard]] ArxReturnCode updateSelectionMembers(SelectionId id,
                                                     const ArxModelSelectionMembersInput& members) noexcept;
  [[nodiscard]] ArxReturnCode clearSelectionVertices(SelectionId id) noexcept;
  [[nodiscard]] ArxReturnCode clearSelectionBones(SelectionId id) noexcept;
  [[nodiscard]] ArxReturnCode clearSelectionActionPoints(SelectionId id) noexcept;
  [[nodiscard]] ArxReturnCode setSelectionIncludesOrigin(SelectionId id, bool includes) noexcept;
  [[nodiscard]] ArxReturnCode removeSelection(SelectionId id) noexcept;
  void clearSelections() noexcept;

 private:
  ArxReturnCode exportGlbInternal(std::span<const Animation* const> animations, const GlbExportOptions& options,
                                  ArxAnimationConversionReport* report, std::vector<std::uint8_t>& out,
                                  std::vector<AnimationSoundFile>* sound_files) const;

  struct Data;
  std::unique_ptr<Data> data_;

  friend class Ambiance;
  friend class Level;
};

}  // namespace pistoris
