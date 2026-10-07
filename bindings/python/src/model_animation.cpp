// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "binding_utils.h"
#include "bindings.h"
#include "resource_state.h"
#include "resource_types.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace pistoris::python {
namespace {

enum class ModelSelectionMember : std::uint8_t { kVertex, kBone, kActionPoint, kOrigin };

Model::PositionWeldMetric modelWeldMetric(Level::PositionWeldMetric metric) {
  switch (metric) {
    case Level::PositionWeldMetric::kEuclidean:
      return Model::PositionWeldMetric::kEuclidean;
    case Level::PositionWeldMetric::kAxisAligned:
      return Model::PositionWeldMetric::kAxisAligned;
  }
  throw nb::value_error("invalid weld metric");
}

Model::DegenerateFacePolicy modelDegenerateFacePolicy(Level::DegenerateFacePolicy policy) {
  switch (policy) {
    case Level::DegenerateFacePolicy::kPreserve:
      return Model::DegenerateFacePolicy::kPreserve;
    case Level::DegenerateFacePolicy::kReject:
      return Model::DegenerateFacePolicy::kReject;
    case Level::DegenerateFacePolicy::kDiscard:
      return Model::DegenerateFacePolicy::kDiscard;
  }
  throw nb::value_error("invalid degenerate-face policy");
}

std::optional<std::string> modelBoneName(const Model& model, BoneIndex bone) {
  if (bone == kInvalidBoneIndex) return std::nullopt;
  return copyString(model.bones()[bone].name);
}

BoneIndex modelBoneIndex(const Model& model, const std::optional<std::string>& name) {
  if (!name) return kInvalidBoneIndex;
  const std::string canonical = canonicalModelIdentifier(*name);
  const auto bones = model.bones();
  for (std::size_t index = 0; index < bones.size(); ++index) {
    if (copyString(bones[index].name) == canonical) return static_cast<BoneIndex>(index);
  }
  throwMissingKey(canonical);
}

std::optional<std::string> modelTexturePath(const Model& model, TextureIndex texture) {
  if (texture == kNoTexture) return std::nullopt;
  return copyString(model.textures()[texture].path);
}

TextureIndex modelTextureIndex(const Model& model, const std::optional<std::string>& path) {
  if (!path) return kNoTexture;
  const std::string canonical = canonicalResourcePath(*path);
  const auto textures = model.textures();
  for (std::size_t index = 0; index < textures.size(); ++index) {
    if (copyString(textures[index].path) == canonical) return static_cast<TextureIndex>(index);
  }
  throwMissingKey(canonical);
}

SelectionId modelSelectionId(const Model& model, std::string_view name) {
  const std::string canonical = canonicalSelectionIdentifier(name);
  const auto ids = model.selectionIds();
  for (std::size_t position = 0; position < ids.size(); ++position) {
    const SelectionId id = ids[position];
    if (copyString(unwrap(model.selection(id)).name) == canonical) return id;
  }
  throwMissingKey(canonical);
}

bool modelSelectionContains(const Model& model, SelectionId id, ModelSelectionMember member, std::size_t index) {
  if (member == ModelSelectionMember::kOrigin) return unwrap(model.selectionIncludesOrigin(id));
  if (member == ModelSelectionMember::kVertex) {
    const auto values = unwrap(model.selectionVertices(id));
    return std::ranges::find(values, static_cast<VertexIndex>(index)) != values.end();
  }
  if (member == ModelSelectionMember::kBone) {
    const auto values = unwrap(model.selectionBones(id));
    return std::ranges::find(values, static_cast<BoneIndex>(index)) != values.end();
  }
  const auto values = unwrap(model.selectionActionPoints(id));
  return std::ranges::find(values, static_cast<ActionPointIndex>(index)) != values.end();
}

std::vector<ModelSelection> modelSelectionNames(const Model& model, ModelSelectionMember member, std::size_t index) {
  std::vector<ModelSelection> result;
  const auto ids = model.selectionIds();
  for (std::size_t position = 0; position < ids.size(); ++position) {
    const SelectionId id = ids[position];
    if (modelSelectionContains(model, id, member, index))
      result.emplace_back(copyString(unwrap(model.selection(id)).name));
  }
  return result;
}

ModelVertex modelVertexValue(const Model& model, VertexIndex index) {
  const ArxModelVertex value = model.vertices()[index];
  return {value.position,
          modelBoneName(model, value.bone),
          modelSelectionNames(model, ModelSelectionMember::kVertex, index)};
}

using ModelSelectionMask = std::uint64_t;

struct ResolvedModelVertex {
  ArxVector3 position{};
  BoneIndex bone = kInvalidBoneIndex;
  ModelSelectionMask selections = 0;
};

ModelSelectionMask modelSelectionMask(const Model& model, const std::vector<ModelSelection>& requested) {
  ModelSelectionMask result = 0;
  for (const ModelSelection& selection : requested) {
    const SelectionId id = modelSelectionId(model, selection.name);
    const ModelSelectionMask bit = ModelSelectionMask{1} << id;
    if ((result & bit) != 0) throw nb::value_error("selection names must be unique");
    result |= bit;
  }
  return result;
}

ModelSelectionMask modelVertexSelectionMask(const Model& model, VertexIndex vertex) {
  ModelSelectionMask result = 0;
  for (const SelectionId id : model.selectionIds()) {
    if (modelSelectionContains(model, id, ModelSelectionMember::kVertex, vertex)) {
      result |= ModelSelectionMask{1} << id;
    }
  }
  return result;
}

ResolvedModelVertex resolveModelVertex(const Model& model, const ModelVertex& value) {
  return {value.position, modelBoneIndex(model, value.bone), modelSelectionMask(model, value.selections)};
}

bool sameModelVertex(const Model& model, VertexIndex index, const ResolvedModelVertex& value) {
  const ArxModelVertex current = model.vertices()[index];
  return current.position.x == value.position.x && current.position.y == value.position.y &&
         current.position.z == value.position.z && current.bone == value.bone &&
         modelVertexSelectionMask(model, index) == value.selections;
}

void setModelSelectionMember(Model& model, SelectionId id, ModelSelectionMember member, std::size_t index,
                             bool included) {
  if (member == ModelSelectionMember::kOrigin) {
    if (unwrap(model.selectionIncludesOrigin(id)) != included) unwrap(model.setSelectionIncludesOrigin(id, included));
    return;
  }

  std::vector<VertexIndex> values;
  if (member == ModelSelectionMember::kVertex) {
    const auto current = unwrap(model.selectionVertices(id));
    values.assign(current.begin(), current.end());
  } else if (member == ModelSelectionMember::kBone) {
    const auto current = unwrap(model.selectionBones(id));
    values.assign(current.begin(), current.end());
  } else {
    const auto current = unwrap(model.selectionActionPoints(id));
    values.assign(current.begin(), current.end());
  }
  const auto value = static_cast<VertexIndex>(index);
  const auto found = std::ranges::find(values, value);
  if (included == (found != values.end())) return;
  if (included) {
    values.push_back(value);
    std::ranges::sort(values);
  } else {
    values.erase(found);
  }
  VertexIndex empty{};
  const VertexIndex* data = values.empty() ? &empty : values.data();
  ArxModelSelectionMembersInput input;
  if (member == ModelSelectionMember::kVertex) {
    input.vertices = data;
    input.vertex_count = values.size();
  } else if (member == ModelSelectionMember::kBone) {
    input.bones = data;
    input.bone_count = values.size();
  } else {
    input.action_points = data;
    input.action_point_count = values.size();
  }
  unwrap(model.updateSelectionMembers(id, input));
}

void setModelSelectionMembership(Model& model, ModelSelectionMember member, std::size_t index,
                                 ModelSelectionMask requested) {
  const auto ids = model.selectionIds();
  for (std::size_t position = 0; position < ids.size(); ++position) {
    const SelectionId id = ids[position];
    const bool included = (requested & (ModelSelectionMask{1} << id)) != 0;
    setModelSelectionMember(model, id, member, index, included);
  }
}

void setModelSelectionMembership(Model& model, ModelSelectionMember member, std::size_t index,
                                 const std::vector<ModelSelection>& requested) {
  setModelSelectionMembership(model, member, index, modelSelectionMask(model, requested));
}

void commitModel(PythonModel& owner, Model&& updated) { static_cast<Model&>(owner) = std::move(updated); }

VertexIndex addModelVertex(Model& model, const ResolvedModelVertex& value) {
  const VertexIndex index = unwrap(model.addVertex({value.position, value.bone}));
  setModelSelectionMembership(model, ModelSelectionMember::kVertex, index, value.selections);
  return index;
}

ArxModelFace modelFaceValue(Model& model, const ModelFace& value,
                            const std::optional<ArxModelFace>& current = std::nullopt) {
  ArxModelFace result{};
  result.normal = value.normal;
  result.texture = modelTextureIndex(model, value.texture);
  result.flags = value.flags;
  result.transval = value.transval;
  for (std::size_t corner = 0; corner < value.corners.size(); ++corner) {
    const ModelCorner& source = value.corners[corner];
    const ResolvedModelVertex resolved = resolveModelVertex(model, source.vertex);
    VertexIndex vertex = kInvalidVertexIndex;
    if (current && sameModelVertex(model, current->corners[corner].vertex, resolved)) {
      vertex = current->corners[corner].vertex;
    } else {
      vertex = addModelVertex(model, resolved);
    }
    result.corners[corner] = {vertex, source.normal, source.u, source.v};
  }
  return result;
}

struct ModelVertexAccess {
  using Owner = PythonModel;
  using Value = ModelVertex;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.vertexCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.vertices; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) {
    return modelVertexValue(owner, static_cast<VertexIndex>(index));
  }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    Model updated(owner);
    const ArxModelVertex converted{value.position, modelBoneIndex(updated, value.bone)};
    unwrap(updated.setVertex(static_cast<VertexIndex>(index), converted));
    setModelSelectionMembership(updated, ModelSelectionMember::kVertex, index, value.selections);
    commitModel(owner, std::move(updated));
  }
  static void append(Owner& owner, std::size_t, const Value& value) {
    Model updated(owner);
    const ArxModelVertex converted{value.position, modelBoneIndex(updated, value.bone)};
    const VertexIndex index = unwrap(updated.addVertex(converted));
    setModelSelectionMembership(updated, ModelSelectionMember::kVertex, index, value.selections);
    commitModel(owner, std::move(updated));
  }
  static void extend(Owner& owner, std::size_t, const std::vector<Value>& values) {
    Model updated(owner);
    for (const Value& value : values) {
      const ArxModelVertex converted{value.position, modelBoneIndex(updated, value.bone)};
      const VertexIndex index = unwrap(updated.addVertex(converted));
      setModelSelectionMembership(updated, ModelSelectionMember::kVertex, index, value.selections);
    }
    commitModel(owner, std::move(updated));
  }
  static std::size_t compact(Owner& owner, std::size_t) {
    const std::size_t removed = unwrap(owner.compactVertices());
    owner.tracking.vertices.invalidate();
    return removed;
  }
};

struct ModelTextureAccess {
  using Owner = PythonModel;
  using Value = Texture;
  static constexpr const char* collection_value_name =  // NOLINT(readability-identifier-naming)
      "pistoris.Texture";
  static std::size_t size(const Owner& owner, std::size_t) { return owner.textureCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.textures; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) { return Value(owner.textures()[index]); }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    unwrap(owner.setTexture(static_cast<TextureIndex>(index), value.asView()));
  }
  static void append(Owner& owner, std::size_t, const Value& value) { (void)unwrap(owner.addTexture(value.asView())); }
  static ElementDisplayLabel displayLabel(const Owner& owner, std::size_t, std::size_t index) {
    return {"path", copyString(owner.textures()[index].path)};
  }
  static std::size_t compact(Owner& owner, std::size_t) {
    const std::size_t removed = unwrap(owner.compactTextures());
    owner.tracking.textures.invalidate();
    return removed;
  }
  static void rebase(Owner& owner, std::size_t, std::string_view path) { unwrap(owner.rebaseTexturePaths(path)); }
};

struct ModelBoneAccess {
  using Owner = PythonModel;
  using Value = ModelBone;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.boneCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.bones; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) {
    const ArxModelBone value = owner.bones()[index];
    Value result;
    result.name = copyString(value.name);
    result.position = value.position;
    result.parent = modelBoneName(owner, value.parent);
    result.blob_shadow_size = value.blob_shadow_size;
    result.selections = modelSelectionNames(owner, ModelSelectionMember::kBone, index);
    return result;
  }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    Model updated(owner);
    const std::string name = value.name;
    unwrap(
        updated.setBone(static_cast<BoneIndex>(index),
                        {view(name), value.position, modelBoneIndex(updated, value.parent), value.blob_shadow_size}));
    setModelSelectionMembership(updated, ModelSelectionMember::kBone, index, value.selections);
    commitModel(owner, std::move(updated));
  }
  static void append(Owner& owner, std::size_t, const Value& value) {
    Model updated(owner);
    const std::string name = value.name;
    const BoneIndex index = unwrap(
        updated.addBone({view(name), value.position, modelBoneIndex(updated, value.parent), value.blob_shadow_size}));
    setModelSelectionMembership(updated, ModelSelectionMember::kBone, index, value.selections);
    commitModel(owner, std::move(updated));
  }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    unwrap(owner.removeBone(static_cast<BoneIndex>(index)));
    owner.tracking.bones.remove(index);
  }
  static ElementDisplayLabel displayLabel(const Owner& owner, std::size_t, std::size_t index) {
    return {"name", copyString(owner.bones()[index].name)};
  }
  static std::size_t positionForName(const Owner& owner, std::size_t, std::string_view name) {
    const std::string canonical = canonicalModelIdentifier(name);
    const auto bones = owner.bones();
    for (std::size_t index = 0; index < bones.size(); ++index)
      if (copyString(bones[index].name) == canonical) return index;
    throwMissingKey(canonical);
  }
};

struct ModelActionPointAccess {
  using Owner = PythonModel;
  using Value = ModelActionPoint;
  static std::size_t size(const Owner& owner, std::size_t) { return owner.actionPointCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.action_points; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) {
    const ArxModelActionPoint value = owner.actionPoints()[index];
    Value result;
    result.name = copyString(value.name);
    result.position = value.position;
    result.bone = modelBoneName(owner, value.bone);
    result.selections = modelSelectionNames(owner, ModelSelectionMember::kActionPoint, index);
    return result;
  }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    Model updated(owner);
    const std::string name = value.name;
    unwrap(updated.setActionPoint(static_cast<ActionPointIndex>(index),
                                  {view(name), value.position, modelBoneIndex(updated, value.bone)}));
    setModelSelectionMembership(updated, ModelSelectionMember::kActionPoint, index, value.selections);
    commitModel(owner, std::move(updated));
  }
  static void append(Owner& owner, std::size_t, const Value& value) {
    Model updated(owner);
    const std::string name = value.name;
    const ActionPointIndex index =
        unwrap(updated.addActionPoint({view(name), value.position, modelBoneIndex(updated, value.bone)}));
    setModelSelectionMembership(updated, ModelSelectionMember::kActionPoint, index, value.selections);
    commitModel(owner, std::move(updated));
  }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    unwrap(owner.removeActionPoint(static_cast<ActionPointIndex>(index)));
    owner.tracking.action_points.remove(index);
  }
  static void clear(Owner& owner, std::size_t) {
    owner.clearActionPoints();
    owner.tracking.action_points.invalidate();
  }
  static ElementDisplayLabel displayLabel(const Owner& owner, std::size_t, std::size_t index) {
    return {"name", copyString(owner.actionPoints()[index].name)};
  }
  static void validate(const Owner& owner, std::size_t) { unwrap(owner.validateActionPoints()); }
};

struct ModelSelectionAccess {
  using Owner = PythonModel;
  using Value = ModelSelection;
  using AddValue = ModelSelection;
  using Key = std::string;
  static constexpr bool hashable = true;  // NOLINT(readability-identifier-naming)
  static std::size_t size(const Owner& owner, std::size_t) { return owner.selectionCount(); }
  static std::size_t elementIndex(const Owner& owner, std::size_t, std::size_t position) {
    return owner.selectionIds()[position];
  }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.selections; }
  static Value get(const Owner& owner, std::size_t, std::size_t id) {
    const ArxModelSelection value = unwrap(owner.selection(static_cast<SelectionId>(id)));
    return ModelSelection(copyString(value.name));
  }
  static void set(Owner& owner, std::size_t, std::size_t id, const Value& value) {
    const ArxModelSelection current = unwrap(owner.selection(static_cast<SelectionId>(id)));
    const std::string name = value.name;
    unwrap(
        owner.setSelection(static_cast<SelectionId>(id),
                           {view(name), current.has_leading_vertex, current.leading_position, current.leading_bone}));
  }
  static ElementDisplayLabel displayLabel(const Owner& owner, std::size_t, std::size_t id) {
    return {"name", copyString(unwrap(owner.selection(static_cast<SelectionId>(id))).name)};
  }
  static Key add(Owner& owner, std::size_t, const AddValue& value) {
    const std::string name = value.name;
    const SelectionId id = unwrap(owner.addSelection({view(name), 0U, {}, kInvalidBoneIndex}));
    return copyString(unwrap(owner.selection(id)).name);
  }
  static std::size_t positionForKey(const Owner& owner, std::size_t, const Key& name) {
    const SelectionId id = modelSelectionId(owner, name);
    const auto ids = owner.selectionIds();
    for (std::size_t position = 0; position < ids.size(); ++position) {
      if (ids[position] == id) return position;
    }
    throwMissingKey(name);
  }
  static void removeKey(Owner& owner, std::size_t, const Key& name) {
    const SelectionId id = modelSelectionId(owner, name);
    unwrap(owner.removeSelection(id));
    owner.tracking.selections.invalidateIndex(id);
    owner.tracking.selection_leading_vertices.invalidateIndex(id);
  }
  static void clear(Owner& owner, std::size_t) {
    owner.clearSelections();
    owner.tracking.selections.invalidate();
    owner.tracking.selection_leading_vertices.invalidate();
  }
  static void validate(const Owner& owner, std::size_t) { unwrap(owner.validateSelections()); }
};

using ModelMemberRef = std::variant<std::monostate, ElementRef<ModelVertexAccess>, ElementRef<ModelBoneAccess>,
                                    ElementRef<ModelActionPointAccess>>;

class ModelSelectionSet {
 public:
  ModelSelectionSet(std::shared_ptr<PythonModel> owner, ModelSelectionMember member, ModelMemberRef reference = {})
      : owner_(std::move(owner)), member_(member), reference_(std::move(reference)) {}

  [[nodiscard]] std::size_t size() const {
    validateMember();
    std::size_t result = 0;
    for (const SelectionId id : owner_->selectionIds()) {
      if (modelSelectionContains(*owner_, id, member_, memberIndex())) ++result;
    }
    return result;
  }

  [[nodiscard]] bool contains(nb::handle value) const {
    if (!nb::isinstance<ElementRef<ModelSelectionAccess>>(value)) return false;
    const auto& selection = nb::cast<const ElementRef<ModelSelectionAccess>&>(value);
    if (!selection.resourceBacked() || &selection.owner() != owner_.get()) return false;
    validateMember();
    try {
      return modelSelectionContains(*owner_, static_cast<SelectionId>(selection.index()), member_, memberIndex());
    } catch (const nb::python_error&) {
      PyErr_Clear();
      return false;
    }
  }

  void add(const ElementRef<ModelSelectionAccess>& selection) {
    validateSelectionOwner(selection);
    update(static_cast<SelectionId>(selection.index()), true);
  }

  void discard(const ElementRef<ModelSelectionAccess>& selection) {
    validateSelectionOwner(selection);
    update(static_cast<SelectionId>(selection.index()), false);
  }

  void remove(const ElementRef<ModelSelectionAccess>& selection) {
    validateSelectionOwner(selection);
    const auto id = static_cast<SelectionId>(selection.index());
    validateMember();
    if (!modelSelectionContains(*owner_, id, member_, memberIndex())) throwMissingKey(id);
    update(id, false);
  }

  void clear() {
    validateMember();
    for (const SelectionId id : owner_->selectionIds()) update(id, false);
  }

  [[nodiscard]] nb::tuple values() const {
    validateMember();
    nb::list result;
    const auto ids = owner_->selectionIds();
    ElementCollection<ModelSelectionAccess> selections(owner_);
    for (std::size_t position = 0; position < ids.size(); ++position) {
      if (modelSelectionContains(*owner_, ids[position], member_, memberIndex())) {
        result.append(selections.at(static_cast<std::int64_t>(position)));
      }
    }
    return nb::tuple(result);
  }

 private:
  [[nodiscard]] std::size_t memberIndex() const {
    return std::visit(
        [](const auto& reference) -> std::size_t {
          using Reference = std::decay_t<decltype(reference)>;
          if constexpr (std::is_same_v<Reference, std::monostate>) {
            return 0;
          } else {
            return reference.index();
          }
        },
        reference_);
  }

  void validateMember() const { (void)memberIndex(); }

  void validateSelectionOwner(const ElementRef<ModelSelectionAccess>& selection) const {
    if (&selection.owner() != owner_.get()) throw nb::value_error("selection belongs to a different model");
  }

  void update(SelectionId id, bool included) {
    validateMember();
    setModelSelectionMember(*owner_, id, member_, memberIndex(), included);
  }

  std::shared_ptr<PythonModel> owner_;
  ModelSelectionMember member_;
  ModelMemberRef reference_;
};

using ModelFaceCornerVertexRef = NestedElementMemberRef<ModelFaceCornerAccess, ModelVertex>;

class ModelFaceCornerVertexSelectionSet {
 public:
  explicit ModelFaceCornerVertexSelectionSet(ModelFaceCornerVertexRef vertex) : vertex_(std::move(vertex)) {}

  [[nodiscard]] std::size_t size() const { return vertex_.copy().selections.size(); }
  [[nodiscard]] bool contains(nb::handle value) const {
    if (!nb::isinstance<ModelSelection>(value)) return false;
    const ModelSelection& selection = nb::cast<const ModelSelection&>(value);
    const auto selections = vertex_.copy().selections;
    return std::ranges::find(selections, selection) != selections.end();
  }
  [[nodiscard]] nb::set snapshot() const {
    nb::set result;
    for (const ModelSelection& selection : vertex_.copy().selections) result.add(nb::cast(selection));
    return result;
  }
  void assign(const nb::iterable& values) {
    auto vertex = vertex_.copy();
    vertex.selections.clear();
    for (nb::handle value : values) {
      ModelSelection selection = nb::cast<ModelSelection>(value);
      if (std::ranges::find(vertex.selections, selection) == vertex.selections.end()) {
        vertex.selections.push_back(std::move(selection));
      }
    }
    vertex_.set(vertex);
  }
  void add(const ModelSelection& selection) {
    auto vertex = vertex_.copy();
    if (std::ranges::find(vertex.selections, selection) == vertex.selections.end()) {
      vertex.selections.push_back(selection);
      vertex_.set(vertex);
    }
  }
  void discard(const ModelSelection& selection) {
    auto vertex = vertex_.copy();
    const auto found = std::ranges::find(vertex.selections, selection);
    if (found != vertex.selections.end()) {
      vertex.selections.erase(found);
      vertex_.set(vertex);
    }
  }
  void remove(const ModelSelection& selection) {
    auto vertex = vertex_.copy();
    const auto found = std::ranges::find(vertex.selections, selection);
    if (found == vertex.selections.end()) throw nb::key_error(selection.name.c_str());
    vertex.selections.erase(found);
    vertex_.set(vertex);
  }
  void clear() {
    auto vertex = vertex_.copy();
    if (vertex.selections.empty()) return;
    vertex.selections.clear();
    vertex_.set(vertex);
  }

 private:
  ModelFaceCornerVertexRef vertex_;
};

class ModelOriginRef {
 public:
  explicit ModelOriginRef(std::shared_ptr<PythonModel> owner) : owner_(std::move(owner)) {}

  [[nodiscard]] ModelOrigin copy() const {
    return {modelBoneName(*owner_, owner_->origin().bone),
            modelSelectionNames(*owner_, ModelSelectionMember::kOrigin, 0)};
  }

  [[nodiscard]] const std::shared_ptr<PythonModel>& owner() const noexcept { return owner_; }

 private:
  std::shared_ptr<PythonModel> owner_;
};

nb::object modelBoneReference(const std::shared_ptr<PythonModel>& owner, const std::optional<std::string>& name) {
  if (!name) return nb::none();
  const BoneIndex index = modelBoneIndex(*owner, name);
  return nb::cast(ElementCollection<ModelBoneAccess>(owner).at(static_cast<std::int64_t>(index)));
}

std::optional<std::string> modelBoneReferenceName(const PythonModel& owner, nb::handle value) {
  if (value.is_none()) return std::nullopt;
  const auto& bone = nb::cast<const ElementRef<ModelBoneAccess>&>(value);
  if (&bone.owner() != &owner) throw nb::value_error("bone belongs to a different model");
  return copyString(owner.bones()[bone.index()].name);
}

class ModelSelectionLeadingVertexRef {
 public:
  ModelSelectionLeadingVertexRef(ElementRef<ModelSelectionAccess> selection, std::shared_ptr<ElementToken> token)
      : selection_(std::move(selection)), token_(std::move(token)) {}

  [[nodiscard]] ElementRef<ModelSelectionAccess> selection() const {
    (void)value();
    return selection_;
  }

  [[nodiscard]] ModelSelectionLeadingVertex copy() const {
    const ArxModelSelection current = value();
    return {current.leading_position, modelBoneName(selection_.owner(), current.leading_bone)};
  }

  [[nodiscard]] ArxVector3 position() const { return value().leading_position; }
  void setPosition(ArxVector3 position) {
    ModelSelectionLeadingVertex updated = copy();
    updated.position = position;
    set(updated);
  }

  [[nodiscard]] nb::object bone() const {
    return modelBoneReference(selection_.owner().shared_from_this(), copy().bone);
  }
  void setBone(nb::handle bone) {
    const std::optional<std::string> name = modelBoneReferenceName(selection_.owner(), bone);
    ModelSelectionLeadingVertex updated = copy();
    updated.bone = name;
    set(updated);
  }

  [[nodiscard]] bool sameIdentity(const ModelSelectionLeadingVertexRef& other) const {
    return selection_.sameIdentity(other.selection_) && token_ == other.token_;
  }

 private:
  [[nodiscard]] ArxModelSelection value() const {
    const std::size_t id = selection_.index();
    if (!token_->index || *token_->index != id) throwInvalidReference();
    const ArxModelSelection current = unwrap(selection_.owner().selection(static_cast<SelectionId>(id)));
    if (current.has_leading_vertex == 0U) throwInvalidReference();
    return current;
  }

  void set(const ModelSelectionLeadingVertex& value) {
    const auto id = static_cast<SelectionId>(selection_.index());
    const ArxModelSelection current = this->value();
    const std::string name = copyString(current.name);
    unwrap(selection_.owner().setSelection(
        id, {view(name), 1U, value.position, modelBoneIndex(selection_.owner(), value.bone)}));
  }

  ElementRef<ModelSelectionAccess> selection_;
  std::shared_ptr<ElementToken> token_;
};

nb::object modelSelectionLeadingVertex(const ElementRef<ModelSelectionAccess>& selection) {
  const auto id = static_cast<SelectionId>(selection.index());
  const ArxModelSelection current = unwrap(selection.owner().selection(id));
  if (current.has_leading_vertex == 0U) return nb::none();
  auto owner = selection.owner().shared_from_this();
  auto token = owner->tracking.selection_leading_vertices.trackCanonical(id);
  return nb::cast(ModelSelectionLeadingVertexRef(selection, std::move(token)));
}

void setModelSelectionLeadingVertex(ElementRef<ModelSelectionAccess>& selection,
                                    const std::optional<ModelSelectionLeadingVertex>& value) {
  const auto id = static_cast<SelectionId>(selection.index());
  const ArxModelSelection current = unwrap(selection.owner().selection(id));
  const std::string name = copyString(current.name);
  if (!value) {
    if (current.has_leading_vertex == 0U) return;
    unwrap(selection.owner().setSelection(id, {view(name), 0U, {}, kInvalidBoneIndex}));
    selection.owner().tracking.selection_leading_vertices.invalidateIndex(id);
    return;
  }
  unwrap(selection.owner().setSelection(
      id, {view(name), 1U, value->position, modelBoneIndex(selection.owner(), value->bone)}));
}

nb::object modelTextureReference(const std::shared_ptr<PythonModel>& owner, const std::optional<std::string>& path) {
  if (!path) return nb::none();
  const TextureIndex index = modelTextureIndex(*owner, path);
  return nb::cast(ElementCollection<ModelTextureAccess>(owner).at(static_cast<std::int64_t>(index)));
}

std::optional<std::string> modelTextureReferencePath(const PythonModel& owner, nb::handle value) {
  if (value.is_none()) return std::nullopt;
  const auto& texture = nb::cast<const ElementRef<ModelTextureAccess>&>(value);
  if (&texture.owner() != &owner) throw nb::value_error("texture belongs to a different model");
  return copyString(owner.textures()[texture.index()].path);
}

template <class Access, ModelSelectionMember Member>
ModelSelectionSet modelSelectionSet(const ElementRef<Access>& reference) {
  auto owner = reference.owner().shared_from_this();
  return {std::move(owner), Member, reference};
}

template <class Access, class Range>
nb::tuple modelElementReferences(const ElementRef<ModelSelectionAccess>& selection, const Range& indices) {
  nb::list result;
  auto owner = selection.owner().shared_from_this();
  ElementCollection<Access> collection(owner);
  for (const auto index : indices) result.append(collection.at(static_cast<std::int64_t>(index)));
  return nb::tuple(result);
}

void replaceModelMesh(PythonModel& owner, const std::vector<ModelVertex>& vertices, const std::vector<ModelFace>& faces,
                      const nb::sequence& textures) {
  Model updated(owner);
  std::vector<ModelVertex> all_vertices = vertices;
  all_vertices.reserve(vertices.size() + faces.size() * 3);
  std::vector<ArxModelVertex> vertex_values;
  vertex_values.reserve(all_vertices.capacity());
  for (const ModelVertex& vertex : vertices)
    vertex_values.push_back({vertex.position, modelBoneIndex(updated, vertex.bone)});
  const auto owned_textures = materializeSequence(textures);
  std::vector<ArxTextureView> texture_views;
  texture_views.reserve(nb::len(owned_textures));
  for (std::size_t index = 0; index < nb::len(owned_textures); ++index) {
    texture_views.push_back(nb::cast<const Texture&>(owned_textures[index]).asView());
  }
  const auto texture_index = [&owned_textures](const std::optional<std::string>& path) {
    if (!path) return kNoTexture;
    const std::string canonical = canonicalResourcePath(*path);
    for (std::size_t index = 0; index < nb::len(owned_textures); ++index) {
      if (nb::cast<const Texture&>(owned_textures[index]).path == canonical) return static_cast<TextureIndex>(index);
    }
    throwMissingKey(canonical);
  };
  std::vector<ArxModelFace> face_values;
  face_values.reserve(faces.size());
  for (const ModelFace& face : faces) {
    ArxModelFace converted{};
    converted.normal = face.normal;
    converted.texture = texture_index(face.texture);
    converted.flags = face.flags;
    converted.transval = face.transval;
    for (std::size_t corner = 0; corner < face.corners.size(); ++corner) {
      const ModelCorner& source = face.corners[corner];
      const VertexIndex vertex = static_cast<VertexIndex>(vertex_values.size());
      all_vertices.push_back(source.vertex);
      vertex_values.push_back({source.vertex.position, modelBoneIndex(updated, source.vertex.bone)});
      converted.corners[corner] = {vertex, source.normal, source.u, source.v};
    }
    face_values.push_back(converted);
  }
  unwrap(updated.replaceMesh({vertex_values.data(),
                              vertex_values.size(),
                              face_values.data(),
                              face_values.size(),
                              texture_views.data(),
                              texture_views.size()}));
  for (std::size_t index = 0; index < all_vertices.size(); ++index) {
    setModelSelectionMembership(updated, ModelSelectionMember::kVertex, index, all_vertices[index].selections);
  }
  commitModel(owner, std::move(updated));
  owner.tracking.vertices.invalidate();
  owner.tracking.faces.invalidate();
  owner.tracking.textures.invalidate();
}

void replaceModelSkeleton(PythonModel& owner, const std::vector<ModelBone>& bones) {
  std::vector<ArxModelBone> values;
  values.reserve(bones.size());
  std::vector<std::string> names;
  names.reserve(bones.size());
  for (const ModelBone& bone : bones) names.push_back(canonicalModelIdentifier(bone.name));
  auto resolve = [&names](const std::optional<std::string>& name) {
    if (!name) return kInvalidBoneIndex;
    const std::string canonical = canonicalModelIdentifier(*name);
    for (std::size_t index = 0; index < names.size(); ++index) {
      if (names[index] == canonical) return static_cast<BoneIndex>(index);
    }
    throwMissingKey(canonical);
  };
  for (std::size_t index = 0; index < bones.size(); ++index) {
    for (std::size_t previous = 0; previous < index; ++previous) {
      if (names[previous] == names[index]) throw nb::value_error("bone names must be unique");
    }
    values.push_back(
        {view(names[index]), bones[index].position, resolve(bones[index].parent), bones[index].blob_shadow_size});
  }
  Model updated(owner);
  unwrap(updated.replaceSkeleton({values.data(), values.size()}));
  for (std::size_t index = 0; index < bones.size(); ++index) {
    setModelSelectionMembership(updated, ModelSelectionMember::kBone, index, bones[index].selections);
  }
  commitModel(owner, std::move(updated));
  owner.tracking.bones.invalidate();
}

class ModelMeshView {
 public:
  explicit ModelMeshView(std::shared_ptr<PythonModel> owner) : owner_(std::move(owner)) {}
  [[nodiscard]] const std::shared_ptr<PythonModel>& owner() const noexcept { return owner_; }

 private:
  std::shared_ptr<PythonModel> owner_;
};

class ModelSkeletonView {
 public:
  explicit ModelSkeletonView(std::shared_ptr<PythonModel> owner) : owner_(std::move(owner)) {}
  [[nodiscard]] const std::shared_ptr<PythonModel>& owner() const noexcept { return owner_; }

 private:
  std::shared_ptr<PythonModel> owner_;
};

std::optional<std::string> animationSoundPath(const Animation& animation, SoundIndex sound) {
  if (sound == kNoSound) return std::nullopt;
  return copyString(animation.sounds()[sound].path);
}

SoundIndex animationSoundIndex(const Animation& animation, const std::optional<std::string>& path) {
  if (!path) return kNoSound;
  const std::string canonical = canonicalResourcePath(*path);
  const auto sounds = animation.sounds();
  for (std::size_t index = 0; index < sounds.size(); ++index) {
    if (copyString(sounds[index].path) == canonical) return static_cast<SoundIndex>(index);
  }
  throwMissingKey(canonical);
}

AnimationKeyframeValue animationKeyframeValue(const Animation& animation, const ArxAnimationKeyframe& value) {
  return {value.frame,
          value.root_translation,
          value.root_rotation,
          value.footstep != 0,
          animationSoundPath(animation, value.sound)};
}

ArxAnimationKeyframe animationKeyframeNative(const Animation& animation, const AnimationKeyframeValue& value) {
  return {value.frame,
          value.root_translation,
          value.root_rotation,
          static_cast<std::uint8_t>(value.footstep),
          animationSoundIndex(animation, value.sound)};
}

struct AnimationKeyframeAccess {
  using Owner = PythonAnimation;
  using Value = AnimationKeyframeValue;
  using CollectionValue = AnimationFrame;
  static constexpr const char* collection_value_name =  // NOLINT(readability-identifier-naming)
      "pistoris.animation.Frame";
  static std::size_t size(const Owner& owner, std::size_t) { return owner.keyframeCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.keyframes; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) {
    return animationKeyframeValue(owner, owner.keyframes()[index]);
  }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    const auto transforms = unwrap(owner.groupTransforms(index));
    std::vector<ArxAnimationGroupTransform> copied;
    copied.reserve(transforms.size());
    for (std::size_t group = 0; group < transforms.size(); ++group) copied.push_back(transforms[group]);
    unwrap(owner.setKeyframe(index, {animationKeyframeNative(owner, value), copied.data(), copied.size()}));
  }
  static CollectionValue snapshot(const Owner& owner, std::size_t, std::size_t index) {
    CollectionValue result;
    result.keyframe = get(owner, 0, index);
    const auto transforms = unwrap(owner.groupTransforms(index));
    result.group_transforms.assign(transforms.begin(), transforms.end());
    return result;
  }
  static void setCollection(Owner& owner, std::size_t, std::size_t index, const CollectionValue& value) {
    const auto previous_group_count = owner.groupCount();
    unwrap(owner.setKeyframe(index,
                             {animationKeyframeNative(owner, value.keyframe),
                              value.group_transforms.data(),
                              value.group_transforms.size()}));
    if (owner.groupCount() != previous_group_count) owner.tracking.groups.invalidate();
    owner.tracking.group_transforms.invalidate(index);
  }
  static void appendCollection(Owner& owner, std::size_t, const CollectionValue& value) {
    const auto previous_group_count = owner.groupCount();
    (void)unwrap(owner.addKeyframe({animationKeyframeNative(owner, value.keyframe),
                                    value.group_transforms.data(),
                                    value.group_transforms.size()}));
    if (owner.groupCount() != previous_group_count) owner.tracking.groups.invalidate();
  }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    const auto previous_group_count = owner.groupCount();
    unwrap(owner.removeKeyframe(index));
    if (owner.groupCount() != previous_group_count) owner.tracking.groups.invalidate();
    owner.tracking.keyframes.remove(index);
    owner.tracking.group_transforms.removeParent(index);
  }
  static void clear(Owner& owner, std::size_t) {
    owner.clearKeyframes();
    owner.tracking.groups.invalidate();
    owner.tracking.keyframes.invalidate();
    owner.tracking.group_transforms.invalidate();
  }
};

struct AnimationGroupTransformAccess {
  using Owner = PythonAnimation;
  using Value = ArxAnimationGroupTransform;
  using DetachedParent = AnimationFrame;
  static std::size_t size(const Owner& owner, std::size_t keyframe) {
    return unwrap(owner.groupTransforms(keyframe)).size();
  }
  static CollectionTracker& tracker(Owner& owner, std::size_t keyframe) {
    return owner.tracking.group_transforms[keyframe];
  }
  static Value get(const Owner& owner, std::size_t keyframe, std::size_t group) {
    return unwrap(owner.groupTransforms(keyframe))[group];
  }
  static void set(Owner& owner, std::size_t keyframe, std::size_t group, const Value& value) {
    const auto current = unwrap(owner.groupTransforms(keyframe));
    std::vector<ArxAnimationGroupTransform> transforms;
    transforms.reserve(current.size());
    for (std::size_t index = 0; index < current.size(); ++index) transforms.push_back(current[index]);
    transforms[group] = value;
    unwrap(owner.setKeyframe(keyframe, {owner.keyframes()[keyframe], transforms.data(), transforms.size()}));
  }
  static std::size_t size(const DetachedParent& frame) { return frame.group_transforms.size(); }
  static Value get(const DetachedParent& frame, std::size_t group) { return frame.group_transforms[group]; }
  static void set(DetachedParent& frame, std::size_t group, const Value& value) {
    frame.group_transforms[group] = value;
  }
  static std::size_t revision(const DetachedParent& frame) { return frame.group_transforms_revision; }
};

struct AnimationSoundAccess {
  using Owner = PythonAnimation;
  using Value = Sound;
  static constexpr const char* collection_value_name =  // NOLINT(readability-identifier-naming)
      "pistoris.Sound";
  static std::size_t size(const Owner& owner, std::size_t) { return owner.soundCount(); }
  static CollectionTracker& tracker(Owner& owner, std::size_t) { return owner.tracking.sounds; }
  static Value get(const Owner& owner, std::size_t, std::size_t index) { return Value(owner.sounds()[index]); }
  static void set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
    unwrap(owner.setSound(static_cast<SoundIndex>(index), value.asView()));
  }
  static void append(Owner& owner, std::size_t, const Value& value) { (void)unwrap(owner.addSound(value.asView())); }
  static void remove(Owner& owner, std::size_t, std::size_t index) {
    unwrap(owner.removeSound(static_cast<SoundIndex>(index)));
    owner.tracking.sounds.remove(index);
  }
  static ElementDisplayLabel displayLabel(const Owner& owner, std::size_t, std::size_t index) {
    return {"path", copyString(owner.sounds()[index].path)};
  }
  static std::size_t positionForPath(const Owner& owner, std::size_t, std::string_view path) {
    return animationSoundIndex(owner, canonicalResourcePath(path));
  }
  static std::size_t compact(Owner& owner, std::size_t) {
    const std::size_t removed = unwrap(owner.compactSounds());
    owner.tracking.sounds.invalidate();
    return removed;
  }
  static void rebase(Owner& owner, std::size_t, std::string_view path) { unwrap(owner.rebaseSoundPaths(path)); }
};

nb::object animationSoundReference(const std::shared_ptr<PythonAnimation>& owner,
                                   const std::optional<std::string>& path) {
  if (!path) return nb::none();
  return nb::cast(ElementCollection<AnimationSoundAccess>(owner).at(animationSoundIndex(*owner, path)));
}

std::optional<std::string> animationSoundReferencePath(const PythonAnimation& owner, nb::handle value) {
  if (value.is_none()) return std::nullopt;
  const auto& sound = nb::cast<const ElementRef<AnimationSoundAccess>&>(value);
  if (&sound.owner() != &owner) throw nb::value_error("sound belongs to a different animation");
  return copyString(owner.sounds()[sound.index()].path);
}

class AnimationGroupRef {
 public:
  AnimationGroupRef(std::shared_ptr<PythonAnimation> owner, std::shared_ptr<ElementToken> token)
      : owner_(std::move(owner)), token_(std::move(token)) {}

  [[nodiscard]] std::size_t index() const {
    if (!token_->index) throwInvalidReference();
    return *token_->index;
  }

  [[nodiscard]] bool claimed() const { return unwrap(owner_->isGroupClaimed(index())); }
  void setClaimed(bool value) {
    if (value) {
      unwrap(owner_->claimGroup(index()));
    } else {
      unwrap(owner_->unclaimGroup(index()));
    }
  }
  [[nodiscard]] bool isVoid() const { return unwrap(owner_->isGroupVoid(index())); }
  void makeVoid() { unwrap(owner_->voidGroup(index())); }

  [[nodiscard]] bool sameIdentity(const AnimationGroupRef& other) const {
    return owner_.get() == other.owner_.get() && index() == other.index();
  }

 private:
  std::shared_ptr<PythonAnimation> owner_;
  std::shared_ptr<ElementToken> token_;
};

class AnimationGroupCollection {
 public:
  explicit AnimationGroupCollection(std::shared_ptr<PythonAnimation> owner) : owner_(std::move(owner)) {}

  [[nodiscard]] std::size_t size() const noexcept { return owner_->groupCount(); }
  [[nodiscard]] AnimationGroupRef at(std::int64_t requested) const {
    const auto index = sequenceIndex(requested, size());
    return {owner_, owner_->tracking.groups.track(index)};
  }

 private:
  std::shared_ptr<PythonAnimation> owner_;
};

class AnimationGroupIterator {
 public:
  explicit AnimationGroupIterator(AnimationGroupCollection collection) : collection_(std::move(collection)) {}

  [[nodiscard]] AnimationGroupRef next() {
    if (index_ >= collection_.size()) throw nb::stop_iteration();
    return collection_.at(static_cast<std::int64_t>(index_++));
  }

 private:
  AnimationGroupCollection collection_;
  std::size_t index_ = 0;
};

struct ModelNativeOutput {
  Ftl ftl;
  ReadOnlySequence<TextureFile> texture_files;
};

struct ModelBytesOutput {
  std::vector<std::uint8_t> ftl;
  ReadOnlySequence<TextureFile> texture_files;
};

struct AnimationNativeOutput {
  Tea tea;
  ReadOnlySequence<SoundFileValue> sound_files;
};

struct AnimationBytesOutput {
  std::vector<std::uint8_t> tea;
  ReadOnlySequence<SoundFileValue> sound_files;
};

struct AnimationImportOutput {
  std::shared_ptr<PythonAnimation> animation;
  std::vector<SoundSourceReference> sound_sources;
};

struct AnimationSoundSourceValue {
  std::shared_ptr<PythonAnimation> animation;
  SoundSourceReferenceValue source;
};

struct AnimationSoundFileValue {
  std::shared_ptr<PythonAnimation> animation;
  SoundFileValue file;
};

struct ModelGlbOutput {
  std::vector<std::uint8_t> glb;
  ReadOnlySequence<AnimationSoundFileValue> sound_files;
  ArxAnimationConversionReport animation_report{};
};

struct InventoryIconValue {
  std::vector<std::uint8_t> encoded_image;
  std::optional<std::uint8_t> width_slots;
  std::optional<std::uint8_t> height_slots;
};

class ModelInventoryIconRef {
 public:
  explicit ModelInventoryIconRef(std::shared_ptr<PythonModel> owner) : owner_(std::move(owner)) {}
  [[nodiscard]] const std::shared_ptr<PythonModel>& owner() const noexcept { return owner_; }

 private:
  std::shared_ptr<PythonModel> owner_;
};

struct ObjOutput {
  std::string obj;
  std::string mtl;
  ReadOnlySequence<TextureFile> texture_files;
};

TextureFile textureFile(const Model& model, const NativeTextureFile& value) {
  return {copyString(model.textures()[value.source_texture].path), value.resource_path, value.encoded_image};
}

SoundFileValue soundFile(const Animation& animation, const SoundFile& value) {
  return {copyString(animation.sounds()[value.source_sound].path), value.path, value.encoded_audio};
}

SoundSourceReferenceValue soundSource(const Animation& animation, const SoundSourceReference& value) {
  return {copyString(animation.sounds()[value.sound].path), value.path};
}

std::optional<std::uint8_t> iconSlot(std::optional<int> value) {
  if (!value) return std::nullopt;
  if (*value < 1 || *value > 3) throw nb::value_error("inventory icon slot count must be inside [1, 3]");
  return static_cast<std::uint8_t>(*value);
}

std::optional<InventoryIconValue> inventoryIconValue(const Model& model) {
  const auto icon = model.inventoryIcon();
  if (!icon.encoded_image.data || icon.encoded_image.size == 0) return std::nullopt;
  InventoryIconValue result;
  result.encoded_image.assign(icon.encoded_image.data, icon.encoded_image.data + icon.encoded_image.size);
  result.width_slots = icon.width_slots;
  result.height_slots = icon.height_slots;
  return result;
}

void setInventoryIconValue(Model& model, const InventoryIconValue& icon) {
  const Model::InventoryIconSetOptions options{.width_slots = icon.width_slots, .height_slots = icon.height_slots};
  unwrap(model.setInventoryIcon({icon.encoded_image.data(), icon.encoded_image.size()}, options));
}

struct MaterialLibraryViews {
  nb::list owners;
  std::vector<ObjMaterialLibraryView> values;
};

MaterialLibraryViews materialLibraryViews(const nb::sequence& libraries) {
  MaterialLibraryViews result;
  result.owners = materializeSequence(libraries);
  result.values.reserve(nb::len(result.owners));
  for (std::size_t index = 0; index < nb::len(result.owners); ++index) {
    result.values.push_back(nb::cast<const ObjMaterialLibrary&>(result.owners[index]).asView());
  }
  return result;
}

template <class T>
const T* specifiedData(const std::vector<T>& values, const T& empty_value) {
  return values.empty() ? &empty_value : values.data();
}

template <class T>
const T* optionalData(const std::optional<std::vector<T>>& values, const T& empty_value) {
  if (!values) return nullptr;
  return specifiedData(*values, empty_value);
}

void bindOutputs(nb::module_& module) {
  nb::class_<ArxAnimationConversionReport>(module, "AnimationConversionReport")
      .def_ro("converted", &ArxAnimationConversionReport::converted)
      .def_ro("skipped", &ArxAnimationConversionReport::skipped);
  nb::class_<ModelNativeOutput>(module, "ModelNativeOutput")
      .def_ro("ftl", &ModelNativeOutput::ftl)
      .def_ro("texture_files", &ModelNativeOutput::texture_files);
  nb::class_<ModelBytesOutput>(module, "ModelBytesOutput")
      .def_prop_ro("ftl", [](const ModelBytesOutput& value) { return toBytes(value.ftl); })
      .def_ro("texture_files", &ModelBytesOutput::texture_files);
  nb::class_<AnimationNativeOutput>(module, "AnimationNativeOutput")
      .def_ro("tea", &AnimationNativeOutput::tea)
      .def_ro("sound_files", &AnimationNativeOutput::sound_files);
  nb::class_<AnimationBytesOutput>(module, "AnimationBytesOutput")
      .def_prop_ro("tea", [](const AnimationBytesOutput& value) { return toBytes(value.tea); })
      .def_ro("sound_files", &AnimationBytesOutput::sound_files);
  nb::class_<ModelImportOutput>(module, "ModelImport")
      .def_ro("model", &ModelImportOutput::model)
      .def_prop_ro(
          "animations",
          [](const ModelImportOutput& value) { return snapshot(value.animations); },
          nb::sig("def animations(self) -> tuple[Animation, ...]"))
      .def_prop_ro(
          "texture_source_paths",
          [](const ModelImportOutput& value) { return snapshot(value.texture_source_paths); },
          nb::sig("def texture_source_paths(self) -> tuple[str, ...]"))
      .def_prop_ro(
          "sound_sources",
          [](const ModelImportOutput& value) {
            nb::list result;
            for (const auto& source : value.sound_sources) {
              auto animation = value.animations.at(source.animation_index);
              result.append(AnimationSoundSourceValue{animation, soundSource(*animation, source.reference)});
            }
            return nb::tuple(result);
          },
          nb::sig("def sound_sources(self) -> tuple[AnimationSoundSource, ...]"))
      .def_ro("animation_report", &ModelImportOutput::animation_report);
  nb::class_<AnimationImportOutput>(module, "AnimationImport")
      .def_ro("animation", &AnimationImportOutput::animation)
      .def_prop_ro(
          "sound_sources",
          [](const AnimationImportOutput& value) {
            nb::list result;
            for (const auto& source : value.sound_sources) result.append(soundSource(*value.animation, source));
            return nb::tuple(result);
          },
          nb::sig("def sound_sources(self) -> tuple[SoundSourceReference, ...]"));
  nb::class_<AnimationSoundSourceValue>(module, "AnimationSoundSource")
      .def_ro("animation", &AnimationSoundSourceValue::animation)
      .def_ro("source", &AnimationSoundSourceValue::source);
  nb::class_<AnimationSoundFileValue>(module, "AnimationSoundFile")
      .def_ro("animation", &AnimationSoundFileValue::animation)
      .def_ro("file", &AnimationSoundFileValue::file);
  bindReadOnlySequence<AnimationSoundFileValue>(
      module, "AnimationSoundFileSequence", "AnimationSoundFile", "pistoris.animation.SoundFileSequence");
  nb::class_<ModelGlbOutput>(module, "ModelGlbOutput")
      .def_prop_ro("glb", [](const ModelGlbOutput& value) { return toBytes(value.glb); })
      .def_ro("sound_files", &ModelGlbOutput::sound_files)
      .def_ro("animation_report", &ModelGlbOutput::animation_report);
  nb::class_<ObjOutput>(module, "ObjOutput")
      .def_ro("obj", &ObjOutput::obj)
      .def_ro("mtl", &ObjOutput::mtl)
      .def_ro("texture_files", &ObjOutput::texture_files);
  auto inventory_icon = nb::class_<InventoryIconValue>(module, "InventoryIcon");
  inventory_icon
      .def(nb::new_([](nb::handle encoded_image, std::optional<int> width_slots, std::optional<int> height_slots) {
             const auto bytes = byteSpan(encoded_image);
             if (bytes.size() == 0) throw nb::value_error("encoded_image cannot be empty");
             return new InventoryIconValue{
                 std::vector<std::uint8_t>(bytes.begin(), bytes.end()), iconSlot(width_slots), iconSlot(height_slots)};
           }),
           nb::kw_only(),
           nb::arg("encoded_image"),
           nb::arg("width_slots") = nb::none(),
           nb::arg("height_slots") = nb::none())
      .def_prop_ro("encoded_image", [](const InventoryIconValue& value) { return toBytes(value.encoded_image); })
      .def_ro("width_slots", &InventoryIconValue::width_slots)
      .def_ro("height_slots", &InventoryIconValue::height_slots);
  bindRecordMediaField(inventory_icon, "encoded_image", &InventoryIconValue::encoded_image, false);
}

void bindModelReferences(nb::module_& module) {
  nb::class_<ModelInventoryIconRef>(module, "ModelInventoryIconRef", "The model inventory icon and its operations.")
      .def("copy", [](const ModelInventoryIconRef& self) { return inventoryIconValue(*self.owner()); })
      .def(
          "set",
          [](const ModelInventoryIconRef& self, const InventoryIconValue& icon) {
            setInventoryIconValue(*self.owner(), icon);
          },
          nb::arg("icon"))
      .def("clear", [](const ModelInventoryIconRef& self) { self.owner()->clearInventoryIcon(); })
      .def(
          "render",
          [](const ModelInventoryIconRef& self,
             ImageFormat format,
             std::optional<int>
                 width_slots,
             std::optional<int>
                 height_slots,
             Model::InventoryIconLayout layout) {
            const Model::InventoryIconRenderOptions options{
                .width_slots = iconSlot(width_slots),
                .height_slots = iconSlot(height_slots),
                .layout = layout,
                .format = format,
            };
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.owner()->renderIcon(options);
            }();
            return toBytes(unwrap(std::move(result)));
          },
          nb::kw_only(),
          nb::arg("format") = ImageFormat::kPng,
          nb::arg("width_slots") = nb::none(),
          nb::arg("height_slots") = nb::none(),
          nb::arg("layout") = Model::InventoryIconLayout::kCenter)
      .def("__repr__", [](const ModelInventoryIconRef& self) {
        return std::string("<pistoris.model.InventoryIconRef present=") +
               (self.owner()->inventoryIcon().encoded_image.size != 0 ? "True>" : "False>");
      });

  auto vertex = bindElementCollection<ModelVertexAccess>(
      module, "ModelVertexRef", "ModelVertexCollection", "pistoris.model.VertexRef", {.reference_property = nullptr});
  bindElementField(vertex, "position", &ModelVertex::position);
  vertex
      .def_prop_rw(
          "bone",
          [](const ElementRef<ModelVertexAccess>& self) {
            return modelBoneReference(self.owner().shared_from_this(), self.copy().bone);
          },
          [](ElementRef<ModelVertexAccess>& self, nb::handle bone) {
            auto value = self.copy();
            value.bone = modelBoneReferenceName(self.owner(), bone);
            self.set(value);
          },
          nb::for_getter(nb::sig("def bone(self) -> BoneRef | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def bone(self, value: BoneRef | None, /) -> None")))
      .def_prop_ro(
          "selections",
          [](const ElementRef<ModelVertexAccess>& self) {
            return modelSelectionSet<ModelVertexAccess, ModelSelectionMember::kVertex>(self);
          },
          nb::sig("def selections(self) -> ModelSelectionSet"));

  auto face =
      bindElementCollection<ModelFaceAccess>(module, "ModelFaceRef", "ModelFaceCollection", "pistoris.model.FaceRef");
  auto detached_vertex = bindNestedElementMemberRef<ModelFaceCornerAccess, ModelVertex>(
      module, "ModelFaceCornerVertexRef", "pistoris.model.FaceCornerVertexRef");
  bindNestedElementMemberField(detached_vertex, "position", &ModelVertex::position);
  detached_vertex
      .def_prop_rw(
          "bone",
          [](const ModelFaceCornerVertexRef& self) { return self.copy().bone; },
          [](ModelFaceCornerVertexRef& self, const std::optional<std::string>& bone) {
            auto value = self.copy();
            value.bone = canonicalOptionalModelIdentifier(bone);
            self.set(value);
          })
      .def_prop_rw(
          "selections",
          [](const ModelFaceCornerVertexRef& self) { return ModelFaceCornerVertexSelectionSet(self); },
          [](ModelFaceCornerVertexRef& self, const nb::iterable& selections) {
            ModelFaceCornerVertexSelectionSet(self).assign(selections);
          },
          nb::for_getter(nb::sig("def selections(self) -> ModelFaceCornerVertexSelectionSet")),
          nb::for_setter(nb::sig("def selections(self, value: Iterable[ModelSelection], /) -> None")));
  auto detached_vertex_selections = nb::class_<ModelFaceCornerVertexSelectionSet>(
      module, "ModelFaceCornerVertexSelectionSet", "A live mutable set of selections on a detached face vertex.");
  detached_vertex_selections.def("__len__", &ModelFaceCornerVertexSelectionSet::size)
      .def("__contains__", &ModelFaceCornerVertexSelectionSet::contains, nb::arg("selection"))
      .def("add", &ModelFaceCornerVertexSelectionSet::add, nb::arg("selection"))
      .def("discard", &ModelFaceCornerVertexSelectionSet::discard, nb::arg("selection"))
      .def("remove", &ModelFaceCornerVertexSelectionSet::remove, nb::arg("selection"))
      .def("clear", &ModelFaceCornerVertexSelectionSet::clear)
      .def(
          "__iter__",
          [](const ModelFaceCornerVertexSelectionSet& self) { return self.snapshot().attr("__iter__")(); },
          nb::sig("def __iter__(self) -> Iterator[ModelSelection]"))
      .def("__repr__", [](const ModelFaceCornerVertexSelectionSet& self) {
        return std::string(nb::repr(self.snapshot()).c_str());
      });
  detached_vertex_selections.attr("__hash__") = nb::none();
  registerMutableSet(detached_vertex_selections);
  auto corner = bindNestedElementCollection<ModelFaceCornerAccess>(
      module, "ModelFaceCornerRef", "ModelFaceCornerCollection", "pistoris.model.FaceCornerRef");
  corner.def_prop_rw(
      "vertex",
      [](const NestedElementRef<ModelFaceCornerAccess>& self) -> nb::object {
        if (!self.resourceBacked()) return nb::cast(ModelFaceCornerVertexRef(self, &ModelCorner::vertex));
        const VertexIndex index = self.owner().faces()[self.parentIndex()].corners[self.index()].vertex;
        return nb::cast(
            ElementCollection<ModelVertexAccess>(self.owner().shared_from_this()).at(static_cast<std::int64_t>(index)));
      },
      [](NestedElementRef<ModelFaceCornerAccess>& self, nb::handle value) {
        if (!self.resourceBacked()) {
          auto corner_value = self.copy();
          corner_value.vertex = nb::cast<ModelVertex>(value);
          self.set(corner_value);
          return;
        }
        const auto& vertex = nb::cast<const ElementRef<ModelVertexAccess>&>(value);
        if (&vertex.owner() != &self.owner()) throw nb::value_error("vertex belongs to a different model");
        ArxModelFace face_value = self.owner().faces()[self.parentIndex()];
        face_value.corners[self.index()].vertex = static_cast<VertexIndex>(vertex.index());
        unwrap(self.owner().setFace(static_cast<FaceIndex>(self.parentIndex()), face_value));
      },
      nb::for_getter(nb::sig("def vertex(self) -> ModelVertexRef | ModelFaceCornerVertexRef")),
      nb::for_setter(nb::sig("def vertex(self, value: ModelVertexRef | ModelVertex, /) -> None")));
  bindNestedElementField(corner, "normal", &ModelCorner::normal);
  bindNestedElementField(corner, "u", &ModelCorner::u);
  bindNestedElementField(corner, "v", &ModelCorner::v);
  bindNestedCollection<ModelFaceCornerAccess>(face, "corners");
  bindElementField(face, "normal", &ModelFace::normal);
  face.def_prop_rw(
      "texture",
      [](const ElementRef<ModelFaceAccess>& self) {
        return modelTextureReference(self.owner().shared_from_this(), self.copy().texture);
      },
      [](ElementRef<ModelFaceAccess>& self, nb::handle texture) {
        auto value = self.copy();
        value.texture = modelTextureReferencePath(self.owner(), texture);
        self.set(value);
      },
      nb::for_getter(nb::sig("def texture(self) -> TextureRef | None")),
      nb::for_setter(nb::arg("value").none()),
      nb::for_setter(nb::sig("def texture(self, value: TextureRef | None, /) -> None")));
  face.def_prop_rw(
      "flags",
      [](const ElementRef<ModelFaceAccess>& self) { return static_cast<FaceTypeBitmask>(self.copy().flags); },
      [](ElementRef<ModelFaceAccess>& self, FaceTypeBitmask flags) {
        auto value = self.copy();
        value.flags = flags;
        self.set(value);
      });
  bindElementField(face, "transval", &ModelFace::transval);

  auto texture = bindElementCollection<ModelTextureAccess>(
      module, "ModelTextureRef", "ModelTextureCollection", "pistoris.model.TextureRef");
  texture
      .def_prop_rw(
          "path",
          [](const ElementRef<ModelTextureAccess>& self) {
            return copyString(self.owner().textures()[self.index()].path);
          },
          [](ElementRef<ModelTextureAccess>& self, std::string_view path) {
            unwrap(self.owner().setTexturePath(static_cast<TextureIndex>(self.index()), path));
          })
      .def_prop_rw(
          "encoded_image",
          [](const ElementRef<ModelTextureAccess>& self) {
            const auto image = self.owner().textures()[self.index()].encoded_image;
            return toOptionalBytes({image.data, image.size});
          },
          [](ElementRef<ModelTextureAccess>& self, nb::handle data) {
            if (data.is_none()) {
              unwrap(self.owner().clearTextureImage(static_cast<TextureIndex>(self.index())));
              return;
            }
            const auto image = byteSpan(data);
            unwrap(self.owner().setTextureImage(static_cast<TextureIndex>(self.index()), {image.data(), image.size()}));
          },
          nb::for_getter(nb::sig("def encoded_image(self) -> bytes | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def encoded_image(self, value: object | None, /) -> None")))
      .def_prop_rw(
          "external_image_extension",
          [](const ElementRef<ModelTextureAccess>& self) {
            return copyString(self.owner().textures()[self.index()].external_image_extension);
          },
          [](ElementRef<ModelTextureAccess>& self, std::string_view extension) {
            unwrap(self.owner().setTextureExternalImageExtension(static_cast<TextureIndex>(self.index()), extension));
          });

  auto bone = bindElementCollection<ModelBoneAccess>(
      module, "ModelBoneRef", "ModelBoneCollection", "pistoris.model.BoneRef", {.reference_property = nullptr});
  bindElementField(bone, "name", &ModelBone::name);
  bindElementField(bone, "position", &ModelBone::position);
  bindElementField(bone, "blob_shadow_size", &ModelBone::blob_shadow_size);
  bone.def_prop_rw(
          "parent",
          [](const ElementRef<ModelBoneAccess>& self) {
            return modelBoneReference(self.owner().shared_from_this(), self.copy().parent);
          },
          [](ElementRef<ModelBoneAccess>& self, nb::handle parent) {
            auto value = self.copy();
            value.parent = modelBoneReferenceName(self.owner(), parent);
            self.set(value);
          },
          nb::for_getter(nb::sig("def parent(self) -> BoneRef | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def parent(self, value: BoneRef | None, /) -> None")))
      .def_prop_ro(
          "selections",
          [](const ElementRef<ModelBoneAccess>& self) {
            return modelSelectionSet<ModelBoneAccess, ModelSelectionMember::kBone>(self);
          },
          nb::sig("def selections(self) -> ModelSelectionSet"));

  auto point = bindElementCollection<ModelActionPointAccess>(
      module, "ModelActionPointRef", "ModelActionPointCollection", "pistoris.model.ActionPointRef");
  bindElementField(point, "name", &ModelActionPoint::name);
  bindElementField(point, "position", &ModelActionPoint::position);
  point
      .def_prop_rw(
          "bone",
          [](const ElementRef<ModelActionPointAccess>& self) {
            return modelBoneReference(self.owner().shared_from_this(), self.copy().bone);
          },
          [](ElementRef<ModelActionPointAccess>& self, nb::handle bone) {
            auto value = self.copy();
            value.bone = modelBoneReferenceName(self.owner(), bone);
            self.set(value);
          },
          nb::for_getter(nb::sig("def bone(self) -> BoneRef | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def bone(self, value: BoneRef | None, /) -> None")))
      .def_prop_ro(
          "selections",
          [](const ElementRef<ModelActionPointAccess>& self) {
            return modelSelectionSet<ModelActionPointAccess, ModelSelectionMember::kActionPoint>(self);
          },
          nb::sig("def selections(self) -> ModelSelectionSet"));
  const auto action_point_collection = module.attr("ModelActionPointCollection");
  action_point_collection.attr("replace") = nb::cpp_function(
      [](const ElementCollection<ModelActionPointAccess>& self, const std::vector<ModelActionPoint>& points) {
        Model updated(*self.owner());
        updated.clearActionPoints();
        for (const ModelActionPoint& point_value : points) {
          const ActionPointIndex index = unwrap(updated.addActionPoint(
              {view(point_value.name), point_value.position, modelBoneIndex(updated, point_value.bone)}));
          setModelSelectionMembership(updated, ModelSelectionMember::kActionPoint, index, point_value.selections);
        }
        commitModel(*self.owner(), std::move(updated));
        self.owner()->tracking.action_points.invalidate();
      },
      nb::is_method(),
      nb::arg("action_points"));

  auto selection = bindElementCollection<ModelSelectionAccess>(module,
                                                               "ModelSelectionRef",
                                                               "ModelSelectionCollection",
                                                               "pistoris.model.SelectionRef",
                                                               {.reference_property = nullptr});
  bindElementField(selection, "name", &ModelSelection::name);
  auto leading_vertex = nb::class_<ModelSelectionLeadingVertexRef>(
      module, "ModelSelectionLeadingVertexRef", "A live reference to a selection's optional leading vertex.");
  leading_vertex.def_prop_ro("selection", &ModelSelectionLeadingVertexRef::selection)
      .def_prop_rw("position", &ModelSelectionLeadingVertexRef::position, &ModelSelectionLeadingVertexRef::setPosition)
      .def_prop_rw("bone",
                   &ModelSelectionLeadingVertexRef::bone,
                   &ModelSelectionLeadingVertexRef::setBone,
                   nb::for_getter(nb::sig("def bone(self) -> ModelBoneRef | None")),
                   nb::for_setter(nb::sig("def bone(self, value: ModelBoneRef | None, /) -> None")))
      .def("copy", &ModelSelectionLeadingVertexRef::copy, "Return an independent copy of the leading vertex.")
      .def(
          "__eq__",
          [](const ModelSelectionLeadingVertexRef& self, nb::handle other) {
            if (!nb::isinstance<ModelSelectionLeadingVertexRef>(other)) return false;
            return self.sameIdentity(nb::cast<const ModelSelectionLeadingVertexRef&>(other));
          },
          nb::is_operator())
      .def("__repr__", [](const ModelSelectionLeadingVertexRef& self) {
        try {
          return std::string("<pistoris.model.SelectionLeadingVertexRef selection=") +
                 nb::repr(nb::cast(self.selection().copy().name)).c_str() + ">";
        } catch (const nb::python_error&) {
          PyErr_Clear();
          return std::string("<pistoris.model.SelectionLeadingVertexRef invalid>");
        }
      });
  leading_vertex.attr("__hash__") = nb::none();
  selection
      .def_prop_rw(
          "leading_vertex",
          &modelSelectionLeadingVertex,
          &setModelSelectionLeadingVertex,
          nb::for_getter(nb::sig("def leading_vertex(self) -> ModelSelectionLeadingVertexRef | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def leading_vertex(self, value: ModelSelectionLeadingVertex | None, /) -> None")))
      .def_prop_ro(
          "vertices",
          [](const ElementRef<ModelSelectionAccess>& self) {
            return modelElementReferences<ModelVertexAccess>(
                self, unwrap(self.owner().selectionVertices(static_cast<SelectionId>(self.index()))));
          },
          nb::sig("def vertices(self) -> tuple[VertexRef, ...]"))
      .def_prop_ro(
          "bones",
          [](const ElementRef<ModelSelectionAccess>& self) {
            return modelElementReferences<ModelBoneAccess>(
                self, unwrap(self.owner().selectionBones(static_cast<SelectionId>(self.index()))));
          },
          nb::sig("def bones(self) -> tuple[BoneRef, ...]"))
      .def_prop_ro(
          "action_points",
          [](const ElementRef<ModelSelectionAccess>& self) {
            return modelElementReferences<ModelActionPointAccess>(
                self, unwrap(self.owner().selectionActionPoints(static_cast<SelectionId>(self.index()))));
          },
          nb::sig("def action_points(self) -> tuple[ActionPointRef, ...]"))
      .def_prop_ro("includes_origin", [](const ElementRef<ModelSelectionAccess>& self) {
        return unwrap(self.owner().selectionIncludesOrigin(static_cast<SelectionId>(self.index())));
      });

  auto selection_set = nb::class_<ModelSelectionSet>(
      module, "ModelSelectionSet", "A live mutable set of selections containing a model member.");
  selection_set.def("__len__", &ModelSelectionSet::size)
      .def("__contains__", &ModelSelectionSet::contains, nb::arg("selection"))
      .def("add", &ModelSelectionSet::add, nb::arg("selection"))
      .def("discard", &ModelSelectionSet::discard, nb::arg("selection"))
      .def("remove", &ModelSelectionSet::remove, nb::arg("selection"))
      .def("clear", &ModelSelectionSet::clear)
      .def(
          "__iter__",
          [](const ModelSelectionSet& self) -> nb::iterator { return nb::iter(self.values()); },
          nb::sig("def __iter__(self) -> Iterator[ModelSelectionRef]"))
      .def("__repr__", [](const ModelSelectionSet& self) {
        return std::string("<pistoris.model.SelectionSet len=") + std::to_string(self.size()) + ">";
      });
  registerMutableSet(selection_set);
  selection_set.attr("__hash__") = nb::none();

  auto origin = nb::class_<ModelOriginRef>(module, "ModelOriginRef", "The live model origin.");
  origin.def("copy", &ModelOriginRef::copy, "Return an independent authoring value.")
      .def_prop_rw(
          "bone",
          [](const ModelOriginRef& self) { return modelBoneReference(self.owner(), self.copy().bone); },
          [](ModelOriginRef& self, nb::handle bone_value) {
            const auto bone = modelBoneReferenceName(*self.owner(), bone_value);
            unwrap(self.owner()->setOrigin({modelBoneIndex(*self.owner(), bone)}));
          },
          nb::for_getter(nb::sig("def bone(self) -> BoneRef | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def bone(self, value: BoneRef | None, /) -> None")))
      .def_prop_ro(
          "selections",
          [](const ModelOriginRef& self) { return ModelSelectionSet(self.owner(), ModelSelectionMember::kOrigin); },
          nb::sig("def selections(self) -> ModelSelectionSet"))
      .def("__repr__", [](const ModelOriginRef& self) {
        const auto bone_name = self.copy().bone;
        return std::string("<pistoris.model.OriginRef bone=") +
               (bone_name ? std::string(nb::repr(nb::cast(*bone_name)).c_str()) : "None") + ">";
      });
  origin.attr("__hash__") = nb::none();

  nb::class_<ModelMeshView>(module, "ModelMesh", "The model mesh and its editing operations.")
      .def_prop_ro(
          "vertices",
          [](const ModelMeshView& self) { return ElementCollection<ModelVertexAccess>(self.owner()); },
          nb::sig("def vertices(self) -> ModelVertexCollection"))
      .def_prop_ro(
          "faces",
          [](const ModelMeshView& self) { return ElementCollection<ModelFaceAccess>(self.owner()); },
          nb::sig("def faces(self) -> ModelFaceCollection"))
      .def_prop_ro(
          "textures",
          [](const ModelMeshView& self) { return ElementCollection<ModelTextureAccess>(self.owner()); },
          nb::sig("def textures(self) -> ModelTextureCollection"))
      .def("validate",
           [](const ModelMeshView& self) {
             auto result = [&] {
               nb::gil_scoped_release release;
               return self.owner()->validateMesh();
             }();
             unwrap(std::move(result));
           })
      .def(
          "weld_vertices",
          [](const ModelMeshView& self,
             float radius,
             Level::PositionWeldMetric metric,
             Level::DegenerateFacePolicy degenerate_faces) {
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.owner()->weldVertices({.radius = radius,
                                                 .metric = modelWeldMetric(metric),
                                                 .degenerate_faces = modelDegenerateFacePolicy(degenerate_faces)});
            }();
            unwrap(std::move(result));
            self.owner()->tracking.vertices.invalidate();
            self.owner()->tracking.faces.invalidate();
          },
          nb::kw_only(),
          nb::arg("radius").sig("0.0001") = 1.0e-4f,
          nb::arg("metric") = Level::PositionWeldMetric::kEuclidean,
          nb::arg("degenerate_faces") = Level::DegenerateFacePolicy::kPreserve)
      .def(
          "replace",
          [](const ModelMeshView& self,
             const std::vector<ModelVertex>& vertices,
             const std::vector<ModelFace>& faces,
             const nb::sequence& textures) { replaceModelMesh(*self.owner(), vertices, faces, textures); },
          nb::arg("vertices"),
          nb::arg("faces"),
          nb::arg("textures") = nb::tuple(),
          nb::sig("def replace(self, vertices: Sequence[ModelVertex], faces: Sequence[ModelFace], "
                  "textures: Sequence[Texture] = ()) -> None"))
      .def("clear",
           [](const ModelMeshView& self) {
             self.owner()->clearMesh();
             self.owner()->tracking.vertices.invalidate();
             self.owner()->tracking.faces.invalidate();
             self.owner()->tracking.textures.invalidate();
           })
      .def("__repr__", [](const ModelMeshView& self) {
        return std::string("<pistoris.model.Mesh vertices=") + std::to_string(self.owner()->vertexCount()) +
               " faces=" + std::to_string(self.owner()->faceCount()) +
               " textures=" + std::to_string(self.owner()->textureCount()) + ">";
      });

  nb::class_<ModelSkeletonView>(module, "ModelSkeleton", "The model skeleton and its editing operations.")
      .def_prop_ro(
          "bones",
          [](const ModelSkeletonView& self) { return ElementCollection<ModelBoneAccess>(self.owner()); },
          nb::sig("def bones(self) -> ModelBoneCollection"))
      .def("validate",
           [](const ModelSkeletonView& self) {
             auto result = [&] {
               nb::gil_scoped_release release;
               return self.owner()->validateSkeleton();
             }();
             unwrap(std::move(result));
           })
      .def("infer_selection_memberships",
           [](const ModelSkeletonView& self) { unwrap(self.owner()->inferBoneSelectionMemberships()); })
      .def(
          "replace",
          [](const ModelSkeletonView& self, const std::vector<ModelBone>& bones) {
            replaceModelSkeleton(*self.owner(), bones);
          },
          nb::arg("bones"))
      .def("clear",
           [](const ModelSkeletonView& self) {
             self.owner()->clearSkeleton();
             self.owner()->tracking.bones.invalidate();
           })
      .def("__repr__", [](const ModelSkeletonView& self) {
        return std::string("<pistoris.model.Skeleton bones=") + std::to_string(self.owner()->boneCount()) + ">";
      });
}

void bindAnimationReferences(nb::module_& module) {
  auto keyframe = bindElementCollection<AnimationKeyframeAccess>(
      module, "AnimationKeyframeRef", "AnimationKeyframeCollection", "pistoris.animation.KeyframeRef");
  const auto keyframe_collection = module.attr("AnimationKeyframeCollection");
  keyframe_collection.attr("replace") = nb::cpp_function(
      [](const ElementCollection<AnimationKeyframeAccess>& self,
         const std::vector<AnimationFrame>& frames,
         std::uint32_t frame_length) {
        std::vector<ArxAnimationKeyframeInput> inputs;
        inputs.reserve(frames.size());
        for (const auto& frame : frames) {
          inputs.push_back({animationKeyframeNative(*self.owner(), frame.keyframe),
                            frame.group_transforms.data(),
                            frame.group_transforms.size()});
        }
        unwrap(self.owner()->replaceKeyframes(frame_length, inputs.data(), inputs.size()));
        self.owner()->tracking.groups.invalidate();
        self.owner()->tracking.keyframes.invalidate();
        self.owner()->tracking.group_transforms.invalidate();
      },
      nb::is_method(),
      nb::arg("frames"),
      nb::kw_only(),
      nb::arg("frame_length"),
      nb::sig("def replace(self, frames: Sequence[AnimationFrame], *, frame_length: int) -> None"));
  bindElementField(keyframe, "frame", &AnimationKeyframeValue::frame);
  bindElementField(keyframe, "root_translation", &AnimationKeyframeValue::root_translation);
  bindElementField(keyframe, "root_rotation", &AnimationKeyframeValue::root_rotation);
  bindElementField(keyframe, "footstep", &AnimationKeyframeValue::footstep);
  keyframe.def_prop_rw(
      "sound",
      [](const ElementRef<AnimationKeyframeAccess>& self) {
        return animationSoundReference(self.owner().shared_from_this(), self.copy().sound);
      },
      [](ElementRef<AnimationKeyframeAccess>& self, nb::handle sound) {
        auto value = self.copy();
        value.sound = animationSoundReferencePath(self.owner(), sound);
        self.set(value);
      },
      nb::for_getter(nb::sig("def sound(self) -> SoundRef | None")),
      nb::for_setter(nb::arg("value").none()),
      nb::for_setter(nb::sig("def sound(self, value: SoundRef | None, /) -> None")));

  auto transform = bindElementCollection<AnimationGroupTransformAccess>(module,
                                                                        "AnimationGroupTransformRef",
                                                                        "AnimationGroupTransformCollection",
                                                                        "pistoris.animation.GroupTransformRef");
  bindElementField(transform, "rotation", &ArxAnimationGroupTransform::rotation);
  bindElementField(transform, "translation", &ArxAnimationGroupTransform::translation);
  bindElementField(transform, "scale", &ArxAnimationGroupTransform::scale);
  const auto frame_type = module.attr("AnimationFrame");
  const auto frame_transforms_getter = nb::cpp_function(
      [](nb::pointer_and_handle<AnimationFrame> frame) {
        return ElementCollection<AnimationGroupTransformAccess>(nb::borrow<nb::object>(frame.h));
      },
      nb::is_method(),
      nb::is_getter(),
      nb::sig("def group_transforms(self) -> AnimationGroupTransformCollection"));
  const auto frame_transforms_setter = nb::cpp_function(
      [](AnimationFrame& frame, std::vector<ArxAnimationGroupTransform> transforms) {
        frame.group_transforms = std::move(transforms);
        ++frame.group_transforms_revision;
      },
      nb::is_method(),
      nb::arg("value"),
      nb::sig("def group_transforms(self, value: Sequence[AnimationGroupTransform], /) -> None"));
  frame_type.attr("group_transforms") =
      nb::module_::import_("builtins").attr("property")(frame_transforms_getter, frame_transforms_setter);
  keyframe.def_prop_ro(
      "group_transforms",
      [](const ElementRef<AnimationKeyframeAccess>& self) {
        const std::size_t keyframe_index = self.index();
        auto owner = self.owner().shared_from_this();
        return ElementCollection<AnimationGroupTransformAccess>(
            std::move(owner), keyframe_index, self.owner().tracking.keyframes);
      },
      nb::sig("def group_transforms(self) -> AnimationGroupTransformCollection"));

  auto sound = bindElementCollection<AnimationSoundAccess>(
      module, "AnimationSoundRef", "AnimationSoundCollection", "pistoris.animation.SoundRef");
  sound
      .def_prop_rw(
          "path",
          [](const ElementRef<AnimationSoundAccess>& self) {
            return copyString(self.owner().sounds()[self.index()].path);
          },
          [](ElementRef<AnimationSoundAccess>& self, std::string_view path) {
            unwrap(self.owner().setSoundPath(static_cast<SoundIndex>(self.index()), path));
          })
      .def_prop_rw(
          "encoded_audio",
          [](const ElementRef<AnimationSoundAccess>& self) {
            const auto audio = self.owner().sounds()[self.index()].encoded_audio;
            return toOptionalBytes({audio.data, audio.size});
          },
          [](ElementRef<AnimationSoundAccess>& self, nb::handle data) {
            if (data.is_none()) {
              unwrap(self.owner().clearSoundData(static_cast<SoundIndex>(self.index())));
              return;
            }
            const auto audio = byteSpan(data);
            unwrap(self.owner().setSoundData(static_cast<SoundIndex>(self.index()), {audio.data(), audio.size()}));
          },
          nb::for_getter(nb::sig("def encoded_audio(self) -> bytes | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def encoded_audio(self, value: object | None, /) -> None")));

  nb::class_<AnimationGroupIterator>(module, "_AnimationGroupCollectionIterator")
      .def("__iter__", [](AnimationGroupIterator& self) -> AnimationGroupIterator& { return self; })
      .def("__next__", &AnimationGroupIterator::next);
  auto groups = nb::class_<AnimationGroupCollection>(
      module, "AnimationGroupCollection", "A live sequence of animation group identities.");
  groups.def("__len__", &AnimationGroupCollection::size)
      .def("__getitem__", &AnimationGroupCollection::at, nb::arg("index"))
      .def(
          "__getitem__",
          [](const AnimationGroupCollection& self, const nb::slice& slice) {
            auto [start, stop, step, length] = slice.compute(self.size());
            (void)stop;
            nb::list result;
            for (std::size_t index = 0; index < length; ++index) {
              result.append(self.at(start));
              start += step;
            }
            return result;
          },
          nb::arg("slice"),
          nb::sig("def __getitem__(self, slice: slice) -> list[AnimationGroupRef]"))
      .def(
          "__iter__",
          [](const AnimationGroupCollection& self) { return AnimationGroupIterator(self); },
          nb::sig("def __iter__(self) -> Iterator[AnimationGroupRef]"))
      .def("__repr__", [](const AnimationGroupCollection& self) {
        return std::string("<pistoris.animation.GroupCollection len=") + std::to_string(self.size()) + ">";
      });
  bindSequenceProtocol(groups, "AnimationGroupRef");
  auto group = nb::class_<AnimationGroupRef>(
      module, "AnimationGroupRef", "A live animation group reference invalidated when group topology changes.");
  group.def_prop_ro("index", &AnimationGroupRef::index)
      .def_prop_rw("claimed", &AnimationGroupRef::claimed, &AnimationGroupRef::setClaimed)
      .def_prop_ro("is_void", &AnimationGroupRef::isVoid)
      .def("make_void",
           &AnimationGroupRef::makeVoid,
           "Perform the destructive reset to the void transform and clear its claimed state.")
      .def(
          "__eq__",
          [](const AnimationGroupRef& self, nb::handle other) {
            if (!nb::isinstance<AnimationGroupRef>(other)) return false;
            try {
              return self.sameIdentity(nb::cast<const AnimationGroupRef&>(other));
            } catch (const nb::python_error&) {
              PyErr_Clear();
              return false;
            }
          },
          nb::is_operator())
      .def("__repr__", [](const AnimationGroupRef& self) {
        try {
          return std::string("<pistoris.animation.GroupRef index=") + std::to_string(self.index()) + ">";
        } catch (const nb::python_error&) {
          PyErr_Clear();
          return std::string("<pistoris.animation.GroupRef invalid>");
        }
      });
  group.attr("__hash__") = nb::none();
}

void bindModel(nb::module_& module) {
  nb::enum_<Model::InventoryIconLayout>(module, "InventoryIconLayout")
      .value("CENTER", Model::InventoryIconLayout::kCenter)
      .value("TOP_LEFT", Model::InventoryIconLayout::kTopLeft)
      .value("TOP_RIGHT", Model::InventoryIconLayout::kTopRight)
      .value("BOTTOM_LEFT", Model::InventoryIconLayout::kBottomLeft)
      .value("BOTTOM_RIGHT", Model::InventoryIconLayout::kBottomRight)
      .value("STRETCH", Model::InventoryIconLayout::kStretch);
  bindModelReferences(module);
  module.def(
      "obj_material_library_paths",
      [](std::string_view obj) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::objMaterialLibraryPaths(obj);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("obj"));

  nb::class_<PythonModel>(
      module,
      "Model",
      "An editable model resource with live element collections. from_*_bytes parses encoded input; "
      "include_* options control returned references, and to_* conversion outputs keep sidecars explicit.")
      .def(nb::new_([] { return std::make_shared<PythonModel>(); }))
      .def("copy", [](const PythonModel& self) { return tracked<Model, ModelTracking>(Model(self)); })
      .def("reset",
           [](PythonModel& self) {
             unwrap(self.reset());
             self.tracking.invalidate();
           })
      .def_static(
          "from_ftl",
          [](const Ftl& data, bool include_texture_sources, NativeTextMode mode) {
            ModelImportOutput output;
            auto result = [&] {
              nb::gil_scoped_release release;
              return Model::importNative(data, include_texture_sources ? &output.texture_source_paths : nullptr, mode);
            }();
            output.model = tracked<Model, ModelTracking>(unwrap(std::move(result)));
            return output;
          },
          nb::arg("ftl"),
          nb::kw_only(),
          nb::arg("include_texture_sources") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def_static(
          "from_ftl_bytes",
          [](nb::handle data, bool include_texture_sources, NativeTextMode mode) {
            ModelImportOutput output;
            const auto bytes = byteSpan(data);
            auto parsed = [&] {
              nb::gil_scoped_release release;
              return readFtl(bytes);
            }();
            auto ftl = unwrap(std::move(parsed));
            auto result = [&] {
              nb::gil_scoped_release release;
              return Model::importNative(ftl, include_texture_sources ? &output.texture_source_paths : nullptr, mode);
            }();
            output.model = tracked<Model, ModelTracking>(unwrap(std::move(result)));
            return output;
          },
          nb::arg("data"),
          nb::kw_only(),
          nb::arg("include_texture_sources") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def_static(
          "from_obj",
          [](std::string_view obj, std::string_view mtl, bool include_texture_sources) {
            ModelImportOutput output;
            auto result = [&] {
              nb::gil_scoped_release release;
              return Model::importObj(obj, mtl, include_texture_sources ? &output.texture_source_paths : nullptr);
            }();
            output.model = tracked<Model, ModelTracking>(unwrap(std::move(result)));
            return output;
          },
          nb::arg("obj"),
          nb::arg("mtl") = "",
          nb::kw_only(),
          nb::arg("include_texture_sources") = true)
      .def_static(
          "from_obj",
          [](std::string_view obj, const nb::sequence& material_libraries, bool include_texture_sources) {
            ModelImportOutput output;
            const auto libraries = materialLibraryViews(material_libraries);
            auto result = [&] {
              nb::gil_scoped_release release;
              return Model::importObj(
                  obj, libraries.values, include_texture_sources ? &output.texture_source_paths : nullptr);
            }();
            output.model = tracked<Model, ModelTracking>(unwrap(std::move(result)));
            return output;
          },
          nb::arg("obj"),
          nb::arg("material_libraries"),
          nb::kw_only(),
          nb::arg("include_texture_sources") = true,
          nb::sig("def from_obj(obj: str, material_libraries: Sequence[ObjMaterialLibrary], *, "
                  "include_texture_sources: bool = True) -> ModelImport"))
      .def_static(
          "from_glb",
          [](nb::handle data,
             bool include_animations,
             bool include_texture_sources,
             bool include_sound_sources,
             float units) {
            ModelImportOutput output;
            Model::GlbImportOptions options{.arx_units_per_glb_unit = units};
            const auto bytes = byteSpan(data);
            if (include_animations) {
              ArxAnimationConversionReport report{};
              auto result = [&] {
                nb::gil_scoped_release release;
                return Model::importGlbWithAnimations(bytes,
                                                      options,
                                                      &report,
                                                      include_texture_sources ? &output.texture_source_paths : nullptr,
                                                      include_sound_sources ? &output.sound_sources : nullptr);
              }();
              auto imported = unwrap(std::move(result));
              output.model = tracked<Model, ModelTracking>(std::move(imported.model));
              output.animations.reserve(imported.animations.size());
              for (auto& animation : imported.animations)
                output.animations.push_back(tracked<Animation, AnimationTracking>(std::move(animation)));
              output.animation_report = report;
            } else {
              auto result = [&] {
                nb::gil_scoped_release release;
                return Model::importGlb(
                    bytes, options, include_texture_sources ? &output.texture_source_paths : nullptr);
              }();
              output.model = tracked<Model, ModelTracking>(unwrap(std::move(result)));
            }
            return output;
          },
          nb::arg("data"),
          nb::kw_only(),
          nb::arg("include_animations") = true,
          nb::arg("include_texture_sources") = true,
          nb::arg("include_sound_sources") = true,
          nb::arg("arx_units_per_glb_unit") = 10.0f)
      .def(
          "to_ftl",
          [](const PythonModel& self, bool include_sidecars, NativeTextMode mode) {
            NativeModelBakeOptions options{.include_texture_files = include_sidecars, .text_mode = mode};
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.bakeNativeBundle(options);
            }();
            auto bundle = unwrap(std::move(result));
            ModelNativeOutput output;
            output.ftl = std::move(bundle.ftl);
            output.texture_files.reserve(bundle.texture_files.size());
            for (const auto& file : bundle.texture_files) output.texture_files.append(textureFile(self, file));
            return output;
          },
          nb::kw_only(),
          nb::arg("include_sidecars") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def(
          "to_ftl_bytes",
          [](const PythonModel& self, bool include_sidecars, NativeTextMode mode, bool compress) {
            NativeModelBakeOptions options{.include_texture_files = include_sidecars, .text_mode = mode};
            auto baked = [&] {
              nb::gil_scoped_release release;
              return self.bakeNativeBundle(options);
            }();
            auto bundle = unwrap(std::move(baked));
            ModelBytesOutput output;
            auto written = [&] {
              nb::gil_scoped_release release;
              return writeFtl(bundle.ftl, compress);
            }();
            output.ftl = unwrap(std::move(written));
            output.texture_files.reserve(bundle.texture_files.size());
            for (const auto& file : bundle.texture_files) output.texture_files.append(textureFile(self, file));
            return output;
          },
          nb::kw_only(),
          nb::arg("include_sidecars") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("compress") = true)
      .def(
          "to_glb",
          [](const PythonModel& self, const nb::sequence& animations, float units, bool include_sidecars) {
            Model::GlbExportOptions options{.arx_units_per_glb_unit = units};
            const auto borrowed = borrowResourcePointers<Animation, PythonAnimation>(animations);
            if (!include_sidecars) {
              ArxAnimationConversionReport report{};
              ModelGlbOutput output;
              auto result = [&] {
                nb::gil_scoped_release release;
                return self.exportGlb(borrowed.values, options, &report);
              }();
              output.glb = unwrap(std::move(result));
              output.animation_report = report;
              return output;
            }
            ArxAnimationConversionReport report{};
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.exportGlbBundle(borrowed.values, options, &report);
            }();
            auto bundle = unwrap(std::move(result));
            ModelGlbOutput output;
            output.glb = std::move(bundle.glb);
            output.sound_files.reserve(bundle.sound_files.size());
            for (auto& item : bundle.sound_files) {
              auto animation = nb::cast<std::shared_ptr<PythonAnimation>>(borrowed.owners[item.animation_index]);
              output.sound_files.append({animation, soundFile(*animation, item.file)});
            }
            output.animation_report = report;
            return output;
          },
          nb::arg("animations") = nb::tuple(),
          nb::kw_only(),
          nb::arg("arx_units_per_glb_unit") = 10.0f,
          nb::arg("include_sidecars") = true,
          nb::sig("def to_glb(self, animations: Sequence[Animation] = (), *, arx_units_per_glb_unit: float = 10.0, "
                  "include_sidecars: bool = True) -> ModelGlbOutput"))
      .def(
          "to_level_preview_glb",
          [](const PythonModel& self, float units, const std::string& class_path, const std::string& asset_name) {
            Model::LevelPreviewGlbOptions options{
                .arx_units_per_glb_unit = units, .class_path = class_path, .asset_name = asset_name};
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.exportLevelPreviewGlb(options);
            }();
            return toBytes(unwrap(std::move(result)));
          },
          nb::kw_only(),
          nb::arg("arx_units_per_glb_unit") = 100.0f,
          nb::arg("class_path") = "",
          nb::arg("asset_name") = "")
      .def(
          "to_obj",
          [](const PythonModel& self, const std::string& stem, bool include_sidecars) {
            ObjExportOptions options{.include_files = include_sidecars};
            auto exported = [&] {
              nb::gil_scoped_release release;
              return self.exportObj(stem, options);
            }();
            auto output = unwrap(std::move(exported));
            ObjOutput result;
            result.obj = std::move(output.text);
            result.mtl = std::move(output.mtl);
            result.texture_files.reserve(output.texture_files.size());
            for (auto& file : output.texture_files) {
              result.texture_files.append({copyString(self.textures()[file.source_texture].path),
                                           std::move(file.path),
                                           std::move(file.encoded_image)});
            }
            return result;
          },
          nb::arg("stem"),
          nb::kw_only(),
          nb::arg("include_sidecars") = true)
      .def("validate",
           [](const PythonModel& self) {
             auto result = [&] {
               nb::gil_scoped_release release;
               return self.validate();
             }();
             unwrap(std::move(result));
           })
      .def(
          "scale", [](PythonModel& self, float factor) { unwrap(self.scale(factor)); }, nb::arg("factor"))
      .def(
          "rotate", [](PythonModel& self, ArxQuat rotation) { unwrap(self.rotate(rotation)); }, nb::arg("rotation"))
      .def(
          "translate", [](PythonModel& self, ArxVector3 offset) { unwrap(self.translate(offset)); }, nb::arg("offset"))
      .def(
          "apply_reference",
          [](PythonModel& self,
             const PythonModel& reference,
             bool snap_bone_positions,
             bool copy_bone_selection_memberships,
             bool copy_action_point_selections) {
            unwrap(self.applyReference(reference,
                                       {.snap_bone_positions = snap_bone_positions,
                                        .copy_bone_selection_memberships = copy_bone_selection_memberships,
                                        .copy_action_point_selections = copy_action_point_selections}));
            self.tracking.invalidate();
          },
          nb::arg("reference"),
          nb::kw_only(),
          nb::arg("snap_bone_positions") = false,
          nb::arg("copy_bone_selection_memberships") = false,
          nb::arg("copy_action_point_selections") = false)
      .def_prop_rw(
          "resource_path",
          [](const PythonModel& self) { return std::string(self.resourcePath()); },
          [](PythonModel& self, const std::string& value) { unwrap(self.setResourcePath(value)); })
      .def("__repr__",
           [](const PythonModel& self) {
             return resourceRepr(
                 "Model", self.resourcePath(), {{"vertices", self.vertexCount()}, {"faces", self.faceCount()}});
           })
      .def_prop_ro("mesh", [](std::shared_ptr<PythonModel> self) { return ModelMeshView(std::move(self)); })
      .def_prop_ro("skeleton", [](std::shared_ptr<PythonModel> self) { return ModelSkeletonView(std::move(self)); })
      .def_prop_ro("origin", [](std::shared_ptr<PythonModel> self) { return ModelOriginRef(std::move(self)); })
      .def_prop_ro(
          "action_points",
          [](std::shared_ptr<PythonModel> self) { return ElementCollection<ModelActionPointAccess>(std::move(self)); },
          nb::sig("def action_points(self) -> ModelActionPointCollection"))
      .def_prop_ro(
          "selections",
          [](std::shared_ptr<PythonModel> self) { return ElementCollection<ModelSelectionAccess>(std::move(self)); },
          nb::sig("def selections(self) -> ModelSelectionCollection"))
      .def_prop_ro("inventory_icon",
                   [](std::shared_ptr<PythonModel> self) { return ModelInventoryIconRef(std::move(self)); });
}

void bindAnimation(nb::module_& module) {
  bindAnimationReferences(module);
  nb::class_<PythonAnimation>(
      module,
      "Animation",
      "An editable skeletal animation with live keyframe and group views. from_tea_bytes parses encoded input; "
      "include_sound_sources controls returned references, and to_tea outputs keep sound sidecars explicit.")
      .def(nb::new_([] { return std::make_shared<PythonAnimation>(); }))
      .def("copy", [](const PythonAnimation& self) { return tracked<Animation, AnimationTracking>(Animation(self)); })
      .def("reset",
           [](PythonAnimation& self) {
             unwrap(self.reset());
             self.tracking.invalidate();
           })
      .def_static(
          "from_tea",
          [](const Tea& data, bool include_sound_sources, NativeTextMode mode) {
            AnimationImportOutput output;
            auto result = [&] {
              nb::gil_scoped_release release;
              return Animation::importNative(data, include_sound_sources ? &output.sound_sources : nullptr, mode);
            }();
            output.animation = tracked<Animation, AnimationTracking>(unwrap(std::move(result)));
            return output;
          },
          nb::arg("tea"),
          nb::kw_only(),
          nb::arg("include_sound_sources") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def_static(
          "from_tea_bytes",
          [](nb::handle data, bool include_sound_sources, NativeTextMode mode) {
            AnimationImportOutput output;
            const auto bytes = byteSpan(data);
            auto parsed = [&] {
              nb::gil_scoped_release release;
              return readTea(bytes);
            }();
            auto tea = unwrap(std::move(parsed));
            auto result = [&] {
              nb::gil_scoped_release release;
              return Animation::importNative(tea, include_sound_sources ? &output.sound_sources : nullptr, mode);
            }();
            output.animation = tracked<Animation, AnimationTracking>(unwrap(std::move(result)));
            return output;
          },
          nb::arg("data"),
          nb::kw_only(),
          nb::arg("include_sound_sources") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def(
          "to_tea",
          [](const PythonAnimation& self, bool include_sidecars, NativeTextMode mode) {
            NativeAnimationBakeOptions options{.include_sound_files = include_sidecars, .text_mode = mode};
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.bakeNativeBundle(options);
            }();
            auto bundle = unwrap(std::move(result));
            AnimationNativeOutput output;
            output.tea = std::move(bundle.tea);
            output.sound_files.reserve(bundle.sound_files.size());
            for (const auto& file : bundle.sound_files) output.sound_files.append(soundFile(self, file));
            return output;
          },
          nb::kw_only(),
          nb::arg("include_sidecars") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def(
          "to_tea_bytes",
          [](const PythonAnimation& self, bool include_sidecars, NativeTextMode mode) {
            NativeAnimationBakeOptions options{.include_sound_files = include_sidecars, .text_mode = mode};
            auto baked = [&] {
              nb::gil_scoped_release release;
              return self.bakeNativeBundle(options);
            }();
            auto bundle = unwrap(std::move(baked));
            AnimationBytesOutput output;
            auto written = [&] {
              nb::gil_scoped_release release;
              return writeTea(bundle.tea);
            }();
            output.tea = unwrap(std::move(written));
            output.sound_files.reserve(bundle.sound_files.size());
            for (const auto& file : bundle.sound_files) output.sound_files.append(soundFile(self, file));
            return output;
          },
          nb::kw_only(),
          nb::arg("include_sidecars") = true,
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def("validate",
           [](const PythonAnimation& self) {
             auto result = [&] {
               nb::gil_scoped_release release;
               return self.validate();
             }();
             unwrap(std::move(result));
           })
      .def(
          "scale", [](PythonAnimation& self, float factor) { unwrap(self.scale(factor)); }, nb::arg("factor"))
      .def(
          "rotate", [](PythonAnimation& self, ArxQuat rotation) { unwrap(self.rotate(rotation)); }, nb::arg("rotation"))
      .def_prop_rw(
          "name",
          [](const PythonAnimation& self) { return std::string(self.name()); },
          [](PythonAnimation& self, const std::string& value) { unwrap(self.setName(value)); })
      .def_prop_rw(
          "resource_path",
          [](const PythonAnimation& self) { return std::string(self.resourcePath()); },
          [](PythonAnimation& self, const std::string& value) { unwrap(self.setResourcePath(value)); })
      .def("__repr__",
           [](const PythonAnimation& self) {
             return resourceRepr("Animation",
                                 self.resourcePath(),
                                 {{"keyframes", self.keyframeCount()}, {"groups", self.groupCount()}});
           })
      .def_prop_rw(
          "frame_length",
          [](const PythonAnimation& self) { return self.frameLength(); },
          [](PythonAnimation& self, std::uint32_t value) { unwrap(self.setFrameLength(value)); })
      .def_prop_ro(
          "groups",
          [](std::shared_ptr<PythonAnimation> self) { return AnimationGroupCollection(std::move(self)); },
          nb::sig("def groups(self) -> AnimationGroupCollection"))
      .def_prop_ro(
          "keyframes",
          [](std::shared_ptr<PythonAnimation> self) {
            return ElementCollection<AnimationKeyframeAccess>(std::move(self));
          },
          nb::sig("def keyframes(self) -> AnimationKeyframeCollection"))
      .def_prop_ro(
          "sounds",
          [](std::shared_ptr<PythonAnimation> self) {
            return ElementCollection<AnimationSoundAccess>(std::move(self));
          },
          nb::sig("def sounds(self) -> AnimationSoundCollection"));
}

}  // namespace

std::size_t ModelFaceAccess::size(const Owner& owner, std::size_t) { return owner.faceCount(); }

CollectionTracker& ModelFaceAccess::tracker(Owner& owner, std::size_t) { return owner.tracking.faces; }

ModelFaceAccess::Value ModelFaceAccess::get(const Owner& owner, std::size_t, std::size_t index) {
  const ArxModelFace source = owner.faces()[index];
  Value result;
  result.normal = source.normal;
  result.texture = modelTexturePath(owner, source.texture);
  result.flags = source.flags;
  result.transval = source.transval;
  for (std::size_t corner = 0; corner < result.corners.size(); ++corner) {
    result.corners[corner] = {modelVertexValue(owner, source.corners[corner].vertex),
                              source.corners[corner].normal,
                              source.corners[corner].u,
                              source.corners[corner].v};
  }
  return result;
}

void ModelFaceAccess::set(Owner& owner, std::size_t, std::size_t index, const Value& value) {
  Model updated(owner);
  const ArxModelFace current = updated.faces()[index];
  unwrap(updated.setFace(static_cast<FaceIndex>(index), modelFaceValue(updated, value, current)));
  commitModel(owner, std::move(updated));
}

void ModelFaceAccess::append(Owner& owner, std::size_t, const Value& value) {
  Model updated(owner);
  (void)unwrap(updated.addFace(modelFaceValue(updated, value)));
  commitModel(owner, std::move(updated));
}

void ModelFaceAccess::remove(Owner& owner, std::size_t, std::size_t index) {
  unwrap(owner.removeFace(static_cast<FaceIndex>(index)));
  owner.tracking.faces.remove(index);
}

void bindModelAnimation(nb::module_& module) {
  bindOutputs(module);
  bindModel(module);
  bindAnimation(module);
}

}  // namespace pistoris::python
