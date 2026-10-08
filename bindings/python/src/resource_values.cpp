// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "binding_utils.h"
#include "bindings.h"
#include "resource_state.h"
#include "resource_types.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <nanobind/stl/array.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>
#include <stdexcept>
#include <string>
#include <utility>

namespace pistoris::python {
namespace {

ModelSelection modelSelectionValue(std::string_view name) { return ModelSelection(canonicalSelectionIdentifier(name)); }

class SelectionValueSet {
 public:
  SelectionValueSet(nb::object owner, std::vector<ModelSelection>* values)
      : owner_(std::move(owner)), values_(values) {}

  [[nodiscard]] std::size_t size() const noexcept { return values_->size(); }
  [[nodiscard]] bool contains(nb::handle value) const {
    if (!nb::isinstance<ModelSelection>(value)) return false;
    return containsValue(nb::cast<const ModelSelection&>(value));
  }
  [[nodiscard]] bool containsValue(const ModelSelection& value) const noexcept {
    return std::ranges::find(*values_, value) != values_->end();
  }
  [[nodiscard]] nb::set snapshot() const {
    nb::set result;
    for (const ModelSelection& value : *values_) result.add(nb::cast(value));
    return result;
  }
  void assign(const nb::iterable& values) {
    std::vector<ModelSelection> replacement;
    for (nb::handle value : values) {
      ModelSelection selection = nb::cast<ModelSelection>(value);
      if (std::ranges::find(replacement, selection) == replacement.end()) replacement.push_back(std::move(selection));
    }
    *values_ = std::move(replacement);
  }
  void add(const ModelSelection& value) {
    if (!containsValue(value)) values_->push_back(value);
  }
  void discard(const ModelSelection& value) {
    const auto found = std::ranges::find(*values_, value);
    if (found != values_->end()) values_->erase(found);
  }
  void remove(const ModelSelection& value) {
    const auto found = std::ranges::find(*values_, value);
    if (found == values_->end()) throw nb::key_error(value.name.c_str());
    values_->erase(found);
  }
  void clear() noexcept { values_->clear(); }

 private:
  nb::object owner_;
  std::vector<ModelSelection>* values_ = nullptr;
};

template <class Value>
void bindSelectionNames(nb::class_<Value>& binding) {
  binding.def_prop_rw(
      "selections",
      [](nb::handle owner) {
        return SelectionValueSet(nb::borrow<nb::object>(owner), &nb::cast<Value&>(owner).selections);
      },
      [](nb::handle owner, const nb::iterable& values) {
        SelectionValueSet(nb::borrow<nb::object>(owner), &nb::cast<Value&>(owner).selections).assign(values);
      },
      nb::for_getter(nb::sig("def selections(self) -> ModelSelectionValueSet")),
      nb::for_setter(nb::sig("def selections(self, value: Iterable[ModelSelection], /) -> None")));
}

template <class Value, class Index>
void bindOptionalIndexField(nb::class_<Value>& binding, const char* name, Index Value::* member, Index absent) {
  binding.def_prop_rw(
      name,
      [member, absent](const Value& value) { return semanticIndex(value.*member, absent); },
      [member, absent](Value& value, const std::optional<Index>& index) { value.*member = index.value_or(absent); });
}

void bindFlags(nb::module_& module) {
  nb::enum_<FaceTypeBitmask>(module, "FaceFlag", nb::is_arithmetic(), nb::is_flag())
      .value("NO_SHADOW", kFaceBitNoShadow)
      .value("DOUBLESIDED", kFaceBitDoublesided)
      .value("TRANS", kFaceBitTrans)
      .value("WATER", kFaceBitWater)
      .value("GLOW", kFaceBitGlow)
      .value("IGNORE", kFaceBitIgnore)
      .value("QUAD", kFaceBitQuad)
      .value("TILED", kFaceBitTiled)
      .value("METAL", kFaceBitMetal)
      .value("HIDE", kFaceBitHide)
      .value("STONE", kFaceBitStone)
      .value("WOOD", kFaceBitWood)
      .value("GRAVEL", kFaceBitGravel)
      .value("EARTH", kFaceBitEarth)
      .value("NOCOL", kFaceBitNocol)
      .value("LAVA", kFaceBitLava)
      .value("CLIMB", kFaceBitClimb)
      .value("FALL", kFaceBitFall)
      .value("NOPATH", kFaceBitNopath)
      .value("NODRAW", kFaceBitNodraw)
      .value("PRECISE_PATH", kFaceBitPrecisePath)
      .value("NO_CLIMB", kFaceBitNoClimb)
      .value("ANGULAR", kFaceBitAngular)
      .value("ANGULAR_IDX0", kFaceBitAngularIdx0)
      .value("ANGULAR_IDX1", kFaceBitAngularIdx1)
      .value("ANGULAR_IDX2", kFaceBitAngularIdx2)
      .value("ANGULAR_IDX3", kFaceBitAngularIdx3)
      .value("LATE_MIP", kFaceBitLateMip)
      .value("LEVEL_ALL", static_cast<FaceTypeBitmask>(ARX_LEVEL_FACE_BITS_ALL))
      .value("ALL", kFaceBitsAll);
  nb::enum_<LightFlagBitmask>(module, "LightFlag", nb::is_arithmetic(), nb::is_flag())
      .value("SEMIDYNAMIC", kLightFlagSemidynamic)
      .value("EXTINGUISHABLE", kLightFlagExtinguishable)
      .value("START_EXTINGUISHED", kLightFlagStartExtinguished)
      .value("SPAWN_FIRE", kLightFlagSpawnFire)
      .value("SPAWN_SMOKE", kLightFlagSpawnSmoke)
      .value("OFF", kLightFlagOff)
      .value("COLOR_LEGACY", kLightFlagColorLegacy)
      .value("NO_CASTED", kLightFlagNoCasted)
      .value("FIX_FLARE_SIZE", kLightFlagFixFlareSize)
      .value("FIREPLACE", kLightFlagFireplace)
      .value("NO_IGNIT", kLightFlagNoIgnit)
      .value("FLARE", kLightFlagFlare)
      .value("ALL", kLightFlagsAll);
  nb::enum_<AnchorFlagBitmask>(module, "AnchorFlag", nb::is_arithmetic(), nb::is_flag())
      .value("BLOCKED", AnchorFlagBitmask::kBlocked);
}

template <class T, std::size_t Size>
nb::tuple fixedArray(const T (&values)[Size]) {
  nb::list result;
  for (const T& value : values) result.append(value);
  return nb::tuple(result);
}

template <class T, std::size_t Size>
nb::tuple fixedArray(const std::array<T, Size>& values) {
  nb::list result;
  for (const T& value : values) result.append(value);
  return nb::tuple(result);
}

template <class T, std::size_t Size>
void setFixedArray(T (&destination)[Size], const nb::sequence& values) {
  if (nb::len(values) != Size) throw nb::value_error("array has the wrong length");
  for (std::size_t index = 0; index < Size; ++index) destination[index] = nb::cast<T>(values[index]);
}

template <class T, std::size_t Size>
void setFixedArray(std::array<T, Size>& destination, const nb::sequence& values) {
  if (nb::len(values) != Size) throw nb::value_error("array has the wrong length");
  for (std::size_t index = 0; index < Size; ++index) destination[index] = nb::cast<T>(values[index]);
}

void bindOwnedMedia(nb::module_& module) {
  auto texture = nb::class_<Texture>(module, "Texture");
  texture
      .def(nb::new_([](const std::string& path, nb::handle encoded_image, std::string external_image_extension) {
             auto* value = new Texture;
             value->path = canonicalResourcePath(path);
             value->external_image_extension = std::move(external_image_extension);
             if (!encoded_image.is_none()) {
               const auto bytes = byteSpan(encoded_image);
               if (bytes.size() == 0) throw nb::value_error("encoded_image cannot be empty; use None to clear it");
               value->encoded_image.assign(bytes.begin(), bytes.end());
             }
             return value;
           }),
           nb::kw_only(),
           nb::arg("path") = "",
           nb::arg("encoded_image") = nb::none(),
           nb::arg("external_image_extension") = "")
      .def_prop_rw(
          "path",
          [](const Texture& value) { return value.path; },
          [](Texture& value, std::string_view path) { value.path = canonicalResourcePath(path); })
      .def_prop_rw(
          "encoded_image",
          [](const Texture& value) { return toOptionalBytes(value.encoded_image); },
          [](Texture& value, nb::handle data) {
            if (data.is_none()) {
              value.encoded_image.clear();
              return;
            }
            const auto bytes = byteSpan(data);
            if (bytes.size() == 0) throw nb::value_error("encoded_image cannot be empty; use None to clear it");
            value.encoded_image.assign(bytes.begin(), bytes.end());
          },
          nb::for_getter(nb::sig("def encoded_image(self) -> bytes | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def encoded_image(self, value: object | None, /) -> None")))
      .def_rw("external_image_extension", &Texture::external_image_extension);
  bindRecordMediaField(texture, "encoded_image", &Texture::encoded_image, true);

  auto sound = nb::class_<Sound>(module, "Sound");
  sound
      .def(nb::new_([](const std::string& path, nb::handle encoded_audio) {
             auto* value = new Sound;
             value->path = canonicalResourcePath(path);
             if (!encoded_audio.is_none()) {
               const auto bytes = byteSpan(encoded_audio);
               if (bytes.size() == 0) throw nb::value_error("encoded_audio cannot be empty; use None to clear it");
               value->encoded_audio.assign(bytes.begin(), bytes.end());
             }
             return value;
           }),
           nb::kw_only(),
           nb::arg("path") = "",
           nb::arg("encoded_audio") = nb::none())
      .def_prop_rw(
          "path",
          [](const Sound& value) { return value.path; },
          [](Sound& value, std::string_view path) { value.path = canonicalResourcePath(path); })
      .def_prop_rw(
          "encoded_audio",
          [](const Sound& value) { return toOptionalBytes(value.encoded_audio); },
          [](Sound& value, nb::handle data) {
            if (data.is_none()) {
              value.encoded_audio.clear();
              return;
            }
            const auto bytes = byteSpan(data);
            if (bytes.size() == 0) throw nb::value_error("encoded_audio cannot be empty; use None to clear it");
            value.encoded_audio.assign(bytes.begin(), bytes.end());
          },
          nb::for_getter(nb::sig("def encoded_audio(self) -> bytes | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def encoded_audio(self, value: object | None, /) -> None")));
  bindRecordMediaField(sound, "encoded_audio", &Sound::encoded_audio, true);

  auto texture_file = nb::class_<TextureFile>(module, "TextureFile");
  texture_file.def_ro("source_path", &TextureFile::source_path)
      .def_ro("path", &TextureFile::path)
      .def_prop_ro("encoded_image", [](const TextureFile& value) { return toBytes(value.encoded_image); });
  bindRecordMediaField(texture_file, "encoded_image", &TextureFile::encoded_image, false);

  auto sound_file = nb::class_<SoundFileValue>(module, "SoundFile");
  sound_file.def_ro("source_path", &SoundFileValue::source_path)
      .def_ro("path", &SoundFileValue::path)
      .def_prop_ro("encoded_audio", [](const SoundFileValue& value) { return toBytes(value.encoded_audio); });
  bindRecordMediaField(sound_file, "encoded_audio", &SoundFileValue::encoded_audio, false);

  auto cinematic_sound_file = nb::class_<CinematicSoundFileValue>(module, "CinematicSoundFile");
  cinematic_sound_file.def_ro("kind", &CinematicSoundFileValue::kind)
      .def_ro("source_path", &CinematicSoundFileValue::source_path)
      .def_ro("language", &CinematicSoundFileValue::language)
      .def_ro("path", &CinematicSoundFileValue::path)
      .def_prop_ro("encoded_audio", [](const CinematicSoundFileValue& value) { return toBytes(value.encoded_audio); });
  bindRecordMediaField(cinematic_sound_file, "encoded_audio", &CinematicSoundFileValue::encoded_audio, false);
  nb::class_<SoundSourceReferenceValue>(module, "SoundSourceReference")
      .def_ro("sound_path", &SoundSourceReferenceValue::sound_path)
      .def_ro("source_path", &SoundSourceReferenceValue::source_path);
  nb::class_<CinematicSoundSourceReferenceValue>(module, "CinematicSoundSourceReference")
      .def_ro("kind", &CinematicSoundSourceReferenceValue::kind)
      .def_ro("sound_path", &CinematicSoundSourceReferenceValue::sound_path)
      .def_ro("source_path", &CinematicSoundSourceReferenceValue::source_path);
  nb::class_<ObjMaterialLibrary>(module, "ObjMaterialLibrary")
      .def(nb::new_([](std::string path, std::string text) {
             return new ObjMaterialLibrary{std::move(path), std::move(text)};
           }),
           nb::kw_only(),
           nb::arg("path") = "",
           nb::arg("text") = "")
      .def_rw("path", &ObjMaterialLibrary::path)
      .def_rw("text", &ObjMaterialLibrary::text);
}

void bindSidecarSequences(nb::module_& module) {
  bindReadOnlySequence<TextureFile>(module, "TextureFileSequence", "TextureFile", "pistoris.TextureFileSequence");
  bindReadOnlySequence<SoundFileValue>(module, "SoundFileSequence", "SoundFile", "pistoris.SoundFileSequence");
  bindReadOnlySequence<CinematicSoundFileValue>(
      module, "CinematicSoundFileSequence", "CinematicSoundFile", "pistoris.cinematic.SoundFileSequence");
}

void bindModelValues(nb::module_& module) {
  nb::class_<ModelSelection>(module, "ModelSelection")
      .def(nb::new_([](const std::string& name) { return new ModelSelection(modelSelectionValue(name)); }),
           nb::kw_only(),
           nb::arg("name"))
      .def_prop_ro("name", [](const ModelSelection& value) { return value.name; })
      .def(
          "__eq__",
          [](const ModelSelection& left, nb::handle right) {
            return valueEquals(
                left, right, [](const ModelSelection& lhs, const ModelSelection& rhs) { return lhs == rhs; });
          },
          nb::sig("def __eq__(self, other: object, /) -> bool"))
      .def("__hash__", [](const ModelSelection& value) { return valueHash(value.name); })
      .def("__repr__", [](const ModelSelection& value) {
        return std::string("Selection(name=") + nb::repr(nb::cast(value.name)).c_str() + ")";
      });

  nb::class_<ModelSelectionLeadingVertex>(module, "ModelSelectionLeadingVertex")
      .def(nb::new_([](ArxVector3 position, const std::optional<std::string>& bone) {
             return new ModelSelectionLeadingVertex{position, canonicalOptionalModelIdentifier(bone)};
           }),
           nb::kw_only(),
           nb::arg("position") = ArxVector3{},
           nb::arg("bone") = nb::none())
      .def_rw("position", &ModelSelectionLeadingVertex::position)
      .def_prop_rw(
          "bone",
          [](const ModelSelectionLeadingVertex& value) { return value.bone; },
          [](ModelSelectionLeadingVertex& value, const std::optional<std::string>& bone) {
            value.bone = canonicalOptionalModelIdentifier(bone);
          });

  auto selection_values = nb::class_<SelectionValueSet>(
      module, "ModelSelectionValueSet", "A live mutable set of selections on a detached model value.");
  selection_values.def("__len__", &SelectionValueSet::size)
      .def("__contains__", &SelectionValueSet::contains, nb::arg("selection"))
      .def("add", &SelectionValueSet::add, nb::arg("selection"))
      .def("discard", &SelectionValueSet::discard, nb::arg("selection"))
      .def("remove", &SelectionValueSet::remove, nb::arg("selection"))
      .def("clear", &SelectionValueSet::clear)
      .def(
          "__iter__",
          [](const SelectionValueSet& self) { return self.snapshot().attr("__iter__")(); },
          nb::sig("def __iter__(self) -> Iterator[ModelSelection]"))
      .def("__repr__", [](const SelectionValueSet& self) { return std::string(nb::repr(self.snapshot()).c_str()); });
  selection_values.attr("__hash__") = nb::none();
  registerMutableSet(selection_values);

  auto vertex = nb::class_<ModelVertex>(module, "ModelVertex");
  vertex
      .def(nb::new_(
               [](ArxVector3 position, const std::optional<std::string>& bone, std::vector<ModelSelection> selections) {
                 return new ModelVertex{position, canonicalOptionalModelIdentifier(bone), std::move(selections)};
               }),
           nb::kw_only(),
           nb::arg("position") = ArxVector3{},
           nb::arg("bone") = nb::none(),
           nb::arg("selections") = nb::tuple())
      .def_rw("position", &ModelVertex::position)
      .def_prop_rw(
          "bone",
          [](const ModelVertex& value) { return value.bone; },
          [](ModelVertex& value, const std::optional<std::string>& bone) {
            value.bone = canonicalOptionalModelIdentifier(bone);
          });
  bindSelectionNames(vertex);

  nb::class_<ModelCorner>(module, "ModelCorner")
      .def(nb::new_([](ModelVertex vertex, ArxVector3 normal, float u, float v) {
             return new ModelCorner{std::move(vertex), normal, u, v};
           }),
           nb::kw_only(),
           nb::arg("vertex") = ModelVertex{},
           nb::arg("normal") = ArxVector3{},
           nb::arg("u") = 0.0f,
           nb::arg("v") = 0.0f)
      .def_rw("vertex", &ModelCorner::vertex)
      .def_rw("normal", &ModelCorner::normal)
      .def_rw("u", &ModelCorner::u)
      .def_rw("v", &ModelCorner::v);
  auto face = nb::class_<ModelFace>(module, "ModelFace");
  face.def(nb::new_([](const std::optional<std::array<ModelCorner, 3>>& corners,
                       ArxVector3 normal,
                       const std::optional<std::string>& texture,
                       FaceTypeBitmask flags,
                       float transval) {
             auto* value = new ModelFace;
             if (corners) value->corners = *corners;
             value->normal = normal;
             value->texture = canonicalOptionalResourcePath(texture);
             value->flags = flags;
             value->transval = transval;
             return value;
           }),
           nb::kw_only(),
           nb::arg("corners") = nb::none(),
           nb::arg("normal") = ArxVector3{},
           nb::arg("texture") = nb::none(),
           nb::arg("flags") = static_cast<FaceTypeBitmask>(0),
           nb::arg("transval") = 0.0f)
      .def_prop_rw(
          "corners",
          [](nb::pointer_and_handle<ModelFace> value) {
            return NestedElementCollection<ModelFaceCornerAccess>(nb::borrow<nb::object>(value.h));
          },
          [](ModelFace& value, const nb::sequence& corners) { setFixedArray(value.corners, corners); },
          nb::for_getter(nb::sig("def corners(self) -> ModelFaceCornerCollection")),
          nb::for_setter(nb::sig("def corners(self, value: Sequence[ModelCorner], /) -> None")))
      .def_rw("normal", &ModelFace::normal)
      .def_prop_rw(
          "texture",
          [](const ModelFace& value) { return value.texture; },
          [](ModelFace& value, const std::optional<std::string>& texture) {
            value.texture = canonicalOptionalResourcePath(texture);
          })
      .def_prop_rw(
          "flags",
          [](const ModelFace& value) { return static_cast<FaceTypeBitmask>(value.flags); },
          [](ModelFace& value, FaceTypeBitmask flags) { value.flags = flags; })
      .def_rw("transval", &ModelFace::transval);

  auto bone = nb::class_<ModelBone>(module, "ModelBone");
  bone.def(nb::new_([](const std::string& name,
                       ArxVector3 position,
                       const std::optional<std::string>& parent,
                       float blob_shadow_size,
                       std::vector<ModelSelection>
                           selections) {
             auto* value = new ModelBone;
             value->name = canonicalModelIdentifier(name);
             value->position = position;
             value->parent = canonicalOptionalModelIdentifier(parent);
             value->blob_shadow_size = blob_shadow_size;
             value->selections = std::move(selections);
             return value;
           }),
           nb::kw_only(),
           nb::arg("name") = "",
           nb::arg("position") = ArxVector3{},
           nb::arg("parent") = nb::none(),
           nb::arg("blob_shadow_size") = 0.0f,
           nb::arg("selections") = nb::tuple())
      .def_prop_rw(
          "name",
          [](const ModelBone& value) { return value.name; },
          [](ModelBone& value, std::string_view name) { value.name = canonicalModelIdentifier(name); })
      .def_rw("position", &ModelBone::position)
      .def_prop_rw(
          "parent",
          [](const ModelBone& value) { return value.parent; },
          [](ModelBone& value, const std::optional<std::string>& parent) {
            value.parent = canonicalOptionalModelIdentifier(parent);
          })
      .def_rw("blob_shadow_size", &ModelBone::blob_shadow_size);
  bindSelectionNames(bone);

  auto action_point = nb::class_<ModelActionPoint>(module, "ModelActionPoint");
  action_point
      .def(nb::new_([](const std::string& name,
                       ArxVector3 position,
                       const std::optional<std::string>& bone,
                       std::vector<ModelSelection>
                           selections) {
             auto* value = new ModelActionPoint;
             value->name = canonicalModelIdentifier(name);
             value->position = position;
             value->bone = canonicalOptionalModelIdentifier(bone);
             value->selections = std::move(selections);
             return value;
           }),
           nb::kw_only(),
           nb::arg("name") = "",
           nb::arg("position") = ArxVector3{},
           nb::arg("bone") = nb::none(),
           nb::arg("selections") = nb::tuple())
      .def_prop_rw(
          "name",
          [](const ModelActionPoint& value) { return value.name; },
          [](ModelActionPoint& value, std::string_view name) { value.name = canonicalModelIdentifier(name); })
      .def_rw("position", &ModelActionPoint::position)
      .def_prop_rw(
          "bone",
          [](const ModelActionPoint& value) { return value.bone; },
          [](ModelActionPoint& value, const std::optional<std::string>& bone) {
            value.bone = canonicalOptionalModelIdentifier(bone);
          });
  bindSelectionNames(action_point);

  auto origin = nb::class_<ModelOrigin>(module, "ModelOrigin");
  origin
      .def(nb::new_([](const std::optional<std::string>& bone, std::vector<ModelSelection> selections) {
             return new ModelOrigin{canonicalOptionalModelIdentifier(bone), std::move(selections)};
           }),
           nb::kw_only(),
           nb::arg("bone") = nb::none(),
           nb::arg("selections") = nb::tuple())
      .def_prop_rw(
          "bone",
          [](const ModelOrigin& value) { return value.bone; },
          [](ModelOrigin& value, const std::optional<std::string>& bone) {
            value.bone = canonicalOptionalModelIdentifier(bone);
          });
  bindSelectionNames(origin);
}

void bindAnimationAmbianceValues(nb::module_& module) {
  nb::enum_<AmbianceAutomationMode>(module, "AmbianceAutomationMode")
      .value("CONSTANT", AmbianceAutomationMode::kConstant)
      .value("STEP", AmbianceAutomationMode::kStep)
      .value("RANDOM_STEP", AmbianceAutomationMode::kRandomStep)
      .value("INTERPOLATED", AmbianceAutomationMode::kInterpolated)
      .value("RANDOM_INTERPOLATED", AmbianceAutomationMode::kRandomInterpolated);
  nb::enum_<AmbianceTrackKind>(module, "AmbianceTrackKind")
      .value("PANNED", AmbianceTrackKind::kPanned)
      .value("POSITIONED", AmbianceTrackKind::kPositioned);
  nb::class_<ArxAnimationGroupTransform>(module, "AnimationGroupTransform")
      .def(nb::new_([](ArxQuat rotation, ArxVector3 translation, ArxVector3 scale) {
             return new ArxAnimationGroupTransform{rotation, translation, scale};
           }),
           nb::kw_only(),
           nb::arg("rotation") = ArxQuat{},
           nb::arg("translation") = ArxVector3{},
           nb::arg("scale") = ArxVector3{1.0f, 1.0f, 1.0f})
      .def_rw("rotation", &ArxAnimationGroupTransform::rotation)
      .def_rw("translation", &ArxAnimationGroupTransform::translation)
      .def_rw("scale", &ArxAnimationGroupTransform::scale);
  auto keyframe = nb::class_<AnimationKeyframeValue>(module, "AnimationKeyframe");
  keyframe
      .def(nb::new_([](std::uint32_t frame,
                       ArxVector3 root_translation,
                       ArxQuat root_rotation,
                       bool footstep,
                       const std::optional<std::string>& sound_path) {
             return new AnimationKeyframeValue{
                 frame, root_translation, root_rotation, footstep, canonicalOptionalResourcePath(sound_path)};
           }),
           nb::kw_only(),
           nb::arg("frame") = 0,
           nb::arg("root_translation") = ArxVector3{},
           nb::arg("root_rotation") = ArxQuat{},
           nb::arg("footstep") = false,
           nb::arg("sound_path") = nb::none())
      .def_rw("frame", &AnimationKeyframeValue::frame)
      .def_rw("root_translation", &AnimationKeyframeValue::root_translation)
      .def_rw("root_rotation", &AnimationKeyframeValue::root_rotation)
      .def_rw("footstep", &AnimationKeyframeValue::footstep)
      .def_prop_rw(
          "sound_path",
          [](const AnimationKeyframeValue& value) { return value.sound; },
          [](AnimationKeyframeValue& value, const std::optional<std::string>& sound_path) {
            value.sound = canonicalOptionalResourcePath(sound_path);
          });
  nb::class_<AnimationFrame>(module, "AnimationFrame")
      .def(nb::new_([](AnimationKeyframeValue keyframe, std::vector<ArxAnimationGroupTransform> group_transforms) {
             auto* value = new AnimationFrame;
             value->keyframe = std::move(keyframe);
             value->group_transforms = std::move(group_transforms);
             return value;
           }),
           nb::kw_only(),
           nb::arg("keyframe") = AnimationKeyframeValue{},
           nb::arg("group_transforms") = nb::tuple())
      .def_rw("keyframe", &AnimationFrame::keyframe)
      .def_prop_rw(
          "group_transforms",
          [](const AnimationFrame& value) { return snapshot(value.group_transforms); },
          [](AnimationFrame& value, std::vector<ArxAnimationGroupTransform> transforms) {
            value.group_transforms = std::move(transforms);
          },
          nb::for_getter(nb::sig("def group_transforms(self) -> tuple[AnimationGroupTransform, ...]")),
          nb::for_setter(nb::sig("def group_transforms(self, value: Sequence[AnimationGroupTransform], /) -> None")));
  nb::class_<ArxAmbianceAutomation>(module, "AmbianceAutomation")
      .def(nb::new_([](float first, float second, std::uint32_t interval_ms, AmbianceAutomationMode mode) {
             return new ArxAmbianceAutomation{first, second, interval_ms, static_cast<ArxAmbianceAutomationMode>(mode)};
           }),
           nb::kw_only(),
           nb::arg("first") = 0.0f,
           nb::arg("second") = 0.0f,
           nb::arg("interval_ms") = 0,
           nb::arg("mode") = AmbianceAutomationMode::kConstant)
      .def_rw("first", &ArxAmbianceAutomation::first)
      .def_rw("second", &ArxAmbianceAutomation::second)
      .def_rw("interval_ms", &ArxAmbianceAutomation::interval_ms)
      .def_prop_rw(
          "mode",
          [](const ArxAmbianceAutomation& value) { return static_cast<AmbianceAutomationMode>(value.mode); },
          [](ArxAmbianceAutomation& value, AmbianceAutomationMode mode) {
            value.mode = static_cast<ArxAmbianceAutomationMode>(mode);
          });
  nb::class_<ArxAmbiancePannedKey>(module, "AmbiancePannedKey")
      .def(nb::new_([](std::uint32_t start_delay_ms,
                       std::uint32_t play_count,
                       std::uint32_t delay_min_ms,
                       std::uint32_t delay_max_ms,
                       ArxAmbianceAutomation volume,
                       ArxAmbianceAutomation pitch,
                       ArxAmbianceAutomation pan) {
             return new ArxAmbiancePannedKey{
                 start_delay_ms, play_count, delay_min_ms, delay_max_ms, volume, pitch, pan};
           }),
           nb::kw_only(),
           nb::arg("start_delay_ms") = 0,
           nb::arg("play_count") = 1,
           nb::arg("delay_min_ms") = 0,
           nb::arg("delay_max_ms") = 0,
           nb::arg("volume") = ArxAmbianceAutomation{},
           nb::arg("pitch") = ArxAmbianceAutomation{},
           nb::arg("pan") = ArxAmbianceAutomation{})
      .def_rw("start_delay_ms", &ArxAmbiancePannedKey::start_delay_ms)
      .def_rw("play_count", &ArxAmbiancePannedKey::play_count)
      .def_rw("delay_min_ms", &ArxAmbiancePannedKey::delay_min_ms)
      .def_rw("delay_max_ms", &ArxAmbiancePannedKey::delay_max_ms)
      .def_rw("volume", &ArxAmbiancePannedKey::volume)
      .def_rw("pitch", &ArxAmbiancePannedKey::pitch)
      .def_rw("pan", &ArxAmbiancePannedKey::pan);
  nb::class_<ArxAmbiancePositionedKey>(module, "AmbiancePositionedKey")
      .def(nb::new_([](std::uint32_t start_delay_ms,
                       std::uint32_t play_count,
                       std::uint32_t delay_min_ms,
                       std::uint32_t delay_max_ms,
                       ArxAmbianceAutomation volume,
                       ArxAmbianceAutomation pitch,
                       ArxAmbianceAutomation x,
                       ArxAmbianceAutomation y,
                       ArxAmbianceAutomation z) {
             return new ArxAmbiancePositionedKey{
                 start_delay_ms, play_count, delay_min_ms, delay_max_ms, volume, pitch, x, y, z};
           }),
           nb::kw_only(),
           nb::arg("start_delay_ms") = 0,
           nb::arg("play_count") = 1,
           nb::arg("delay_min_ms") = 0,
           nb::arg("delay_max_ms") = 0,
           nb::arg("volume") = ArxAmbianceAutomation{},
           nb::arg("pitch") = ArxAmbianceAutomation{},
           nb::arg("x") = ArxAmbianceAutomation{},
           nb::arg("y") = ArxAmbianceAutomation{},
           nb::arg("z") = ArxAmbianceAutomation{})
      .def_rw("start_delay_ms", &ArxAmbiancePositionedKey::start_delay_ms)
      .def_rw("play_count", &ArxAmbiancePositionedKey::play_count)
      .def_rw("delay_min_ms", &ArxAmbiancePositionedKey::delay_min_ms)
      .def_rw("delay_max_ms", &ArxAmbiancePositionedKey::delay_max_ms)
      .def_rw("volume", &ArxAmbiancePositionedKey::volume)
      .def_rw("pitch", &ArxAmbiancePositionedKey::pitch)
      .def_rw("x", &ArxAmbiancePositionedKey::x)
      .def_rw("y", &ArxAmbiancePositionedKey::y)
      .def_rw("z", &ArxAmbiancePositionedKey::z);
  auto panned_track = nb::class_<AmbiancePannedTrackValue>(module, "AmbiancePannedTrack");
  panned_track
      .def(nb::new_([](const std::optional<std::string>& sound_path, std::vector<ArxAmbiancePannedKey> keys) {
             return new AmbiancePannedTrackValue{canonicalOptionalResourcePath(sound_path), std::move(keys)};
           }),
           nb::kw_only(),
           nb::arg("sound_path") = nb::none(),
           nb::arg("keys") = nb::tuple())
      .def_prop_rw(
          "keys",
          [](const AmbiancePannedTrackValue& value) { return snapshot(value.keys); },
          [](AmbiancePannedTrackValue& value, std::vector<ArxAmbiancePannedKey> keys) { value.keys = std::move(keys); },
          nb::for_getter(nb::sig("def keys(self) -> tuple[AmbiancePannedKey, ...]")),
          nb::for_setter(nb::sig("def keys(self, value: Sequence[AmbiancePannedKey], /) -> None")))
      .def_prop_rw(
          "sound_path",
          [](const AmbiancePannedTrackValue& value) { return value.sound; },
          [](AmbiancePannedTrackValue& value, const std::optional<std::string>& sound_path) {
            value.sound = canonicalOptionalResourcePath(sound_path);
          });

  auto positioned_track = nb::class_<AmbiancePositionedTrackValue>(module, "AmbiancePositionedTrack");
  positioned_track
      .def(nb::new_([](const std::optional<std::string>& sound_path, std::vector<ArxAmbiancePositionedKey> keys) {
             return new AmbiancePositionedTrackValue{canonicalOptionalResourcePath(sound_path), std::move(keys)};
           }),
           nb::kw_only(),
           nb::arg("sound_path") = nb::none(),
           nb::arg("keys") = nb::tuple())
      .def_prop_rw(
          "keys",
          [](const AmbiancePositionedTrackValue& value) { return snapshot(value.keys); },
          [](AmbiancePositionedTrackValue& value, std::vector<ArxAmbiancePositionedKey> keys) {
            value.keys = std::move(keys);
          },
          nb::for_getter(nb::sig("def keys(self) -> tuple[AmbiancePositionedKey, ...]")),
          nb::for_setter(nb::sig("def keys(self, value: Sequence[AmbiancePositionedKey], /) -> None")))
      .def_prop_rw(
          "sound_path",
          [](const AmbiancePositionedTrackValue& value) { return value.sound; },
          [](AmbiancePositionedTrackValue& value, const std::optional<std::string>& sound_path) {
            value.sound = canonicalOptionalResourcePath(sound_path);
          });
}

void bindCinematicValues(nb::module_& module) {
  nb::enum_<CinematicIllustrationFormat>(module, "CinematicIllustrationFormat")
      .value("AUTO", CinematicIllustrationFormat::kAuto)
      .value("BMP", CinematicIllustrationFormat::kBmp)
      .value("TGA", CinematicIllustrationFormat::kTga);
  nb::enum_<ArxCinematicInterpolation>(module, "CinematicInterpolation")
      .value("NONE", ARX_CINEMATIC_INTERPOLATION_NONE)
      .value("BEZIER", ARX_CINEMATIC_INTERPOLATION_BEZIER)
      .value("LINEAR", ARX_CINEMATIC_INTERPOLATION_LINEAR);
  nb::enum_<ArxCinematicBaseEffect>(module, "CinematicBaseEffect")
      .value("NONE", ARX_CINEMATIC_BASE_EFFECT_NONE)
      .value("FADE_IN", ARX_CINEMATIC_BASE_EFFECT_FADE_IN)
      .value("FADE_OUT", ARX_CINEMATIC_BASE_EFFECT_FADE_OUT)
      .value("BLUR", ARX_CINEMATIC_BASE_EFFECT_BLUR);
  nb::enum_<ArxCinematicPostEffect>(module, "CinematicPostEffect")
      .value("NONE", ARX_CINEMATIC_POST_EFFECT_NONE)
      .value("FLASH", ARX_CINEMATIC_POST_EFFECT_FLASH)
      .value("SUPPRESS_FLASH", ARX_CINEMATIC_POST_EFFECT_SUPPRESS_FLASH);
  nb::class_<ArxCinematicLight>(module, "CinematicLight")
      .def(nb::new_([](ArxVector3 position,
                       float fall_in,
                       float fall_out,
                       ArxColor3 color,
                       float intensity,
                       float random_intensity) {
             return new ArxCinematicLight{position, fall_in, fall_out, color, intensity, random_intensity};
           }),
           nb::kw_only(),
           nb::arg("position") = ArxVector3{},
           nb::arg("fall_in") = 0.0f,
           nb::arg("fall_out") = 0.0f,
           nb::arg("color") = ArxColor3{},
           nb::arg("intensity") = -1.0f,
           nb::arg("random_intensity") = 0.0f)
      .def_rw("position", &ArxCinematicLight::position)
      .def_rw("fall_in", &ArxCinematicLight::fall_in)
      .def_rw("fall_out", &ArxCinematicLight::fall_out)
      .def_rw("color", &ArxCinematicLight::color)
      .def_rw("intensity", &ArxCinematicLight::intensity)
      .def_rw("random_intensity", &ArxCinematicLight::random_intensity);
  auto illustration = nb::class_<CinematicIllustrationValue>(module, "CinematicIllustration");
  illustration
      .def(nb::new_([](const std::string& path,
                       nb::handle encoded_image,
                       std::string external_image_extension,
                       std::int32_t subdivision_scale) {
             auto* value = new CinematicIllustrationValue;
             value->path = canonicalResourcePath(path);
             if (!encoded_image.is_none()) {
               const auto image = byteSpan(encoded_image);
               value->encoded_image.assign(image.begin(), image.end());
             }
             value->external_image_extension = std::move(external_image_extension);
             value->subdivision_scale = subdivision_scale;
             return value;
           }),
           nb::kw_only(),
           nb::arg("path") = "",
           nb::arg("encoded_image") = nb::none(),
           nb::arg("external_image_extension") = "",
           nb::arg("subdivision_scale") = 1)
      .def_prop_rw(
          "path",
          [](const CinematicIllustrationValue& value) { return value.path; },
          [](CinematicIllustrationValue& value, std::string_view path) { value.path = canonicalResourcePath(path); })
      .def_prop_rw(
          "encoded_image",
          [](const CinematicIllustrationValue& value) { return toOptionalBytes(value.encoded_image); },
          [](CinematicIllustrationValue& value, nb::handle data) {
            if (data.is_none()) {
              value.encoded_image.clear();
              return;
            }
            const auto image = byteSpan(data);
            value.encoded_image.assign(image.begin(), image.end());
          },
          nb::for_getter(nb::sig("def encoded_image(self) -> bytes | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def encoded_image(self, value: object | None, /) -> None")))
      .def_rw("external_image_extension", &CinematicIllustrationValue::external_image_extension)
      .def_rw("subdivision_scale", &CinematicIllustrationValue::subdivision_scale);
  bindRecordMediaField(illustration, "encoded_image", &CinematicIllustrationValue::encoded_image, true);

  auto keyframe = nb::class_<CinematicKeyframeValue>(module, "CinematicKeyframe");
  keyframe
      .def(nb::new_([](std::int32_t frame,
                       ArxVector3 camera_position,
                       float camera_roll,
                       ArxColor3 color,
                       ArxColor3 secondary_color,
                       ArxColor3 flash_color,
                       float flash_decay,
                       std::optional<ArxCinematicLight>
                           light,
                       float outgoing_speed,
                       const std::optional<std::string>& sound_path,
                       SoundKind sound_kind,
                       ArxCinematicInterpolation interpolation,
                       ArxCinematicBaseEffect base_effect,
                       ArxCinematicPostEffect post_effect,
                       bool crossfade,
                       bool dream) {
             auto* value = new CinematicKeyframeValue{};
             value->frame = frame;
             value->camera_position = camera_position;
             value->camera_roll = camera_roll;
             value->color = color;
             value->secondary_color = secondary_color;
             value->flash_color = flash_color;
             value->flash_decay = flash_decay;
             value->light = light;
             value->outgoing_speed = outgoing_speed;
             value->sound_path = canonicalOptionalResourcePath(sound_path);
             value->sound_kind = sound_kind;
             value->interpolation = interpolation;
             value->base_effect = base_effect;
             value->post_effect = post_effect;
             value->crossfade = crossfade;
             value->dream = dream;
             return value;
           }),
           nb::kw_only(),
           nb::arg("frame") = 0,
           nb::arg("camera_position") = ArxVector3{},
           nb::arg("camera_roll") = 0.0f,
           nb::arg("color") = ArxColor3{1.0f, 1.0f, 1.0f},
           nb::arg("secondary_color") = ArxColor3{1.0f, 1.0f, 1.0f},
           nb::arg("flash_color") = ArxColor3{1.0f, 1.0f, 1.0f},
           nb::arg("flash_decay") = 0.0f,
           nb::arg("light") = nb::none(),
           nb::arg("outgoing_speed") = 1.0f,
           nb::arg("sound_path") = nb::none(),
           nb::arg("sound_kind") = SoundKind::kEffect,
           nb::arg("interpolation") = ARX_CINEMATIC_INTERPOLATION_LINEAR,
           nb::arg("base_effect") = ARX_CINEMATIC_BASE_EFFECT_NONE,
           nb::arg("post_effect") = ARX_CINEMATIC_POST_EFFECT_NONE,
           nb::arg("crossfade") = false,
           nb::arg("dream") = false)
      .def_rw("frame", &CinematicKeyframeValue::frame)
      .def_rw("camera_position", &CinematicKeyframeValue::camera_position)
      .def_rw("camera_roll", &CinematicKeyframeValue::camera_roll)
      .def_rw("color", &CinematicKeyframeValue::color)
      .def_rw("secondary_color", &CinematicKeyframeValue::secondary_color)
      .def_rw("flash_color", &CinematicKeyframeValue::flash_color)
      .def_rw("flash_decay", &CinematicKeyframeValue::flash_decay)
      .def_rw("light", &CinematicKeyframeValue::light)
      .def_rw("outgoing_speed", &CinematicKeyframeValue::outgoing_speed)
      .def_prop_rw(
          "sound_path",
          [](const CinematicKeyframeValue& value) { return value.sound_path; },
          [](CinematicKeyframeValue& value, const std::optional<std::string>& sound_path) {
            value.sound_path = canonicalOptionalResourcePath(sound_path);
          })
      .def_rw("sound_kind", &CinematicKeyframeValue::sound_kind)
      .def_rw("interpolation", &CinematicKeyframeValue::interpolation)
      .def_rw("base_effect", &CinematicKeyframeValue::base_effect)
      .def_rw("post_effect", &CinematicKeyframeValue::post_effect)
      .def_rw("crossfade", &CinematicKeyframeValue::crossfade)
      .def_rw("dream", &CinematicKeyframeValue::dream);
}

void bindLevelValues(nb::module_& module) {
  nb::enum_<PortalShape>(module, "PortalShape")
      .value("TRIANGLE", PortalShape::kTriangle)
      .value("QUAD", PortalShape::kQuad);
  nb::enum_<ZoneHeightMode>(module, "ZoneHeightMode")
      .value("FINITE", ZoneHeightMode::kFinite)
      .value("INFINITE", ZoneHeightMode::kInfinite);
  nb::enum_<PathNodeType>(module, "PathNodeType")
      .value("STANDARD", PathNodeType::kStandard)
      .value("BEZIER", PathNodeType::kBezier);
  nb::class_<LevelZoneAmbiance>(module, "LevelZoneAmbiance")
      .def(nb::new_([](const std::string& name, float volume) {
             return new LevelZoneAmbiance{canonicalZoneAmbiance(name), volume};
           }),
           nb::kw_only(),
           nb::arg("name") = "",
           nb::arg("volume") = 100.0f)
      .def_ro("name", &LevelZoneAmbiance::name)
      .def_ro("volume", &LevelZoneAmbiance::volume)
      .def(
          "__eq__",
          [](const LevelZoneAmbiance& left, nb::handle right) {
            return valueEquals(left, right, [](const auto& lhs, const auto& rhs) {
              return lhs.name == rhs.name && lhs.volume == rhs.volume;
            });
          },
          nb::sig("def __eq__(self, other: object, /) -> bool"))
      .def("__hash__", [](const LevelZoneAmbiance& value) { return valueHash(value.name, value.volume); })
      .def("__repr__", [](const LevelZoneAmbiance& value) {
        return std::string("ZoneAmbiance(name=") + nb::repr(nb::cast(value.name)).c_str() +
               ", volume=" + nb::repr(nb::cast(value.volume)).c_str() + ")";
      });
  auto minimap_sampler = nb::class_<MinimapSamplerValue>(module, "MinimapSampler");
  minimap_sampler
      .def(nb::new_([](nb::handle encoded_image, ArxColor3 color) {
             auto* value = new MinimapSamplerValue;
             if (!encoded_image.is_none()) {
               const auto bytes = byteSpan(encoded_image);
               if (bytes.size() == 0) throw nb::value_error("encoded_image cannot be empty; use None to clear it");
               value->encoded_image.assign(bytes.begin(), bytes.end());
             }
             value->color = color;
             return value;
           }),
           nb::kw_only(),
           nb::arg("encoded_image") = nb::none(),
           nb::arg("color") = ArxColor3{})
      .def_prop_rw(
          "encoded_image",
          [](const MinimapSamplerValue& value) { return toOptionalBytes(value.encoded_image); },
          [](MinimapSamplerValue& value, nb::handle data) {
            if (data.is_none()) {
              value.encoded_image.clear();
              return;
            }
            const auto bytes = byteSpan(data);
            if (bytes.size() == 0) throw nb::value_error("encoded_image cannot be empty; use None to clear it");
            value.encoded_image.assign(bytes.begin(), bytes.end());
          },
          nb::for_getter(nb::sig("def encoded_image(self) -> bytes | None")),
          nb::for_setter(nb::arg("value").none()),
          nb::for_setter(nb::sig("def encoded_image(self, value: object | None, /) -> None")))
      .def_rw("color", &MinimapSamplerValue::color);
  bindRecordMediaField(minimap_sampler, "encoded_image", &MinimapSamplerValue::encoded_image, true);
  nb::class_<ArxLevelVertex>(module, "LevelVertex")
      .def(nb::new_([](ArxVector3 position) { return new ArxLevelVertex{position}; }),
           nb::kw_only(),
           nb::arg("position") = ArxVector3{})
      .def_rw("position", &ArxLevelVertex::position);
  nb::class_<LevelCorner>(module, "LevelCorner")
      .def(nb::new_([](ArxLevelVertex vertex, ArxVector3 normal, float u, float v, ArxColor3 color) {
             return new LevelCorner{vertex, normal, u, v, color};
           }),
           nb::kw_only(),
           nb::arg("vertex") = ArxLevelVertex{},
           nb::arg("normal") = ArxVector3{},
           nb::arg("u") = 0.0f,
           nb::arg("v") = 0.0f,
           nb::arg("color") = ArxColor3{0.5f, 0.5f, 0.5f})
      .def_rw("vertex", &LevelCorner::vertex)
      .def_rw("normal", &LevelCorner::normal)
      .def_rw("u", &LevelCorner::u)
      .def_rw("v", &LevelCorner::v)
      .def_rw("color", &LevelCorner::color);
  auto face = nb::class_<LevelFace>(module, "LevelFace");
  face.def(nb::new_([](const std::optional<std::array<LevelCorner, 3>>& corners,
                       const std::optional<std::string>& texture,
                       const std::optional<std::string>& room,
                       FaceTypeBitmask flags,
                       float transval,
                       const std::optional<ArxVector3>& normal) {
             auto* value = new LevelFace;
             if (corners) value->corners = *corners;
             value->normal = normal;
             value->texture = canonicalOptionalResourcePath(texture);
             value->room = canonicalOptionalIdentifier(room);
             value->flags = flags;
             value->transval = transval;
             return value;
           }),
           nb::kw_only(),
           nb::arg("corners") = nb::none(),
           nb::arg("texture") = nb::none(),
           nb::arg("room") = nb::none(),
           nb::arg("flags") = static_cast<FaceTypeBitmask>(0),
           nb::arg("transval") = 0.0f,
           nb::arg("normal") = nb::none())
      .def_prop_rw(
          "corners",
          [](nb::pointer_and_handle<LevelFace> value) {
            return NestedElementCollection<LevelFaceCornerAccess>(nb::borrow<nb::object>(value.h));
          },
          [](LevelFace& value, const nb::sequence& corners) { setFixedArray(value.corners, corners); },
          nb::for_getter(nb::sig("def corners(self) -> LevelFaceCornerCollection")),
          nb::for_setter(nb::sig("def corners(self, value: Sequence[LevelCorner], /) -> None")))
      .def_prop_rw(
          "texture",
          [](const LevelFace& value) { return value.texture; },
          [](LevelFace& value, const std::optional<std::string>& texture) {
            value.texture = canonicalOptionalResourcePath(texture);
          })
      .def_prop_rw(
          "room",
          [](const LevelFace& value) { return value.room; },
          [](LevelFace& value, const std::optional<std::string>& room) {
            value.room = canonicalOptionalIdentifier(room);
          })
      .def_prop_rw(
          "flags",
          [](const LevelFace& value) { return static_cast<FaceTypeBitmask>(value.flags); },
          [](LevelFace& value, FaceTypeBitmask flags) { value.flags = flags; })
      .def_prop_rw(
          "normal",
          [](const LevelFace& value) { return value.normal.value_or(levelFaceNormal(value)); },
          [](LevelFace& value, ArxVector3 normal) { value.normal = normal; })
      .def_rw("transval", &LevelFace::transval);
  nb::class_<LevelRoomDistanceValue>(module, "LevelRoomDistance")
      .def(nb::new_([](float distance,
                       const std::optional<std::string>& portal_a,
                       const std::optional<std::string>& portal_b) {
             return new LevelRoomDistanceValue{
                 distance, canonicalOptionalIdentifier(portal_a), canonicalOptionalIdentifier(portal_b)};
           }),
           nb::kw_only(),
           nb::arg("distance") = -1.0f,
           nb::arg("portal_a") = nb::none(),
           nb::arg("portal_b") = nb::none())
      .def_rw("distance", &LevelRoomDistanceValue::distance)
      .def_prop_rw(
          "portal_a",
          [](const LevelRoomDistanceValue& value) { return value.portal_a; },
          [](LevelRoomDistanceValue& value, const std::optional<std::string>& portal) {
            value.portal_a = canonicalOptionalIdentifier(portal);
          })
      .def_prop_rw(
          "portal_b",
          [](const LevelRoomDistanceValue& value) { return value.portal_b; },
          [](LevelRoomDistanceValue& value, const std::optional<std::string>& portal) {
            value.portal_b = canonicalOptionalIdentifier(portal);
          });
  nb::class_<LevelAnchorConnection>(module, "LevelAnchorConnection")
      .def(nb::new_([](const std::string& first, const std::string& second) {
             return new LevelAnchorConnection{canonicalIdentifier(first), canonicalIdentifier(second)};
           }),
           nb::kw_only(),
           nb::arg("first") = "",
           nb::arg("second") = "")
      .def_prop_rw(
          "first",
          [](const LevelAnchorConnection& value) { return value.first; },
          [](LevelAnchorConnection& value, std::string_view anchor) { value.first = canonicalIdentifier(anchor); })
      .def_prop_rw(
          "second",
          [](const LevelAnchorConnection& value) { return value.second; },
          [](LevelAnchorConnection& value, std::string_view anchor) { value.second = canonicalIdentifier(anchor); });
  nb::class_<LevelNavSurfaceTriangleValue>(module, "LevelNavSurfaceTriangle")
      .def(nb::new_([](std::array<ArxLevelVertex, 3> vertices) { return new LevelNavSurfaceTriangleValue{vertices}; }),
           nb::kw_only(),
           nb::arg("vertices") = std::array<ArxLevelVertex, 3>{})
      .def_prop_rw(
          "vertices",
          [](const LevelNavSurfaceTriangleValue& value) { return fixedArray(value.vertices); },
          [](LevelNavSurfaceTriangleValue& value, const nb::sequence& vertices) {
            setFixedArray(value.vertices, vertices);
          },
          nb::for_getter(nb::sig("def vertices(self) -> tuple[LevelVertex, LevelVertex, LevelVertex]")),
          nb::for_setter(nb::sig("def vertices(self, value: Sequence[LevelVertex], /) -> None")));
  nb::class_<ArxLevelNavSurfaceInfo>(module, "LevelNavSurfaceInfo")
      .def_prop_ro("has_surface", [](const ArxLevelNavSurfaceInfo& value) { return value.has_surface != 0; })
      .def_ro("vertex_count", &ArxLevelNavSurfaceInfo::vertex_count)
      .def_ro("triangle_count", &ArxLevelNavSurfaceInfo::triangle_count);
  nb::class_<ArxLevelPlayerSpawn>(module, "LevelPlayerSpawn")
      .def(nb::new_(
               [](ArxVector3 position, ArxQuat rotation) { return new ArxLevelPlayerSpawn{position, rotation, 1}; }),
           nb::kw_only(),
           nb::arg("position") = ArxVector3{},
           nb::arg("rotation") = ArxQuat{})
      .def_ro("position", &ArxLevelPlayerSpawn::position)
      .def_ro("rotation", &ArxLevelPlayerSpawn::rotation)
      .def(
          "__eq__",
          [](const ArxLevelPlayerSpawn& left, nb::handle right) {
            return valueEquals(left, right, [](const auto& lhs, const auto& rhs) {
              return lhs.position.x == rhs.position.x && lhs.position.y == rhs.position.y &&
                     lhs.position.z == rhs.position.z && lhs.rotation.w == rhs.rotation.w &&
                     lhs.rotation.x == rhs.rotation.x && lhs.rotation.y == rhs.rotation.y &&
                     lhs.rotation.z == rhs.rotation.z;
            });
          },
          nb::sig("def __eq__(self, other: object, /) -> bool"))
      .def("__hash__",
           [](const ArxLevelPlayerSpawn& value) {
             return valueHash(value.position.x,
                              value.position.y,
                              value.position.z,
                              value.rotation.w,
                              value.rotation.x,
                              value.rotation.y,
                              value.rotation.z);
           })
      .def("__repr__", [](const ArxLevelPlayerSpawn& value) {
        return std::string("PlayerSpawn(position=") + nb::repr(nb::cast(value.position)).c_str() +
               ", rotation=" + nb::repr(nb::cast(value.rotation)).c_str() + ")";
      });
  nb::class_<ArxLevelPathNode>(module, "LevelPathNode")
      .def(nb::new_([](ArxVector3 relative_position, PathNodeType type, std::uint32_t time_ms) {
             return new ArxLevelPathNode{relative_position, static_cast<ArxPathNodeType>(type), time_ms};
           }),
           nb::kw_only(),
           nb::arg("relative_position") = ArxVector3{},
           nb::arg("type") = PathNodeType::kStandard,
           nb::arg("time_ms") = 0)
      .def_rw("relative_position", &ArxLevelPathNode::relative_position)
      .def_prop_rw(
          "type",
          [](const ArxLevelPathNode& value) { return static_cast<PathNodeType>(value.type); },
          [](ArxLevelPathNode& value, PathNodeType type) { value.type = static_cast<ArxPathNodeType>(type); })
      .def_rw("time_ms", &ArxLevelPathNode::time_ms);
  nb::class_<LevelRoom>(module, "LevelRoom")
      .def(nb::new_([](const std::string& name) {
             auto* value = new LevelRoom;
             value->name = canonicalIdentifier(name);
             return value;
           }),
           nb::kw_only(),
           nb::arg("name") = "")
      .def_prop_rw(
          "name",
          [](const LevelRoom& value) { return value.name; },
          [](LevelRoom& value, std::string_view name) { value.name = canonicalIdentifier(name); });
  auto portal = nb::class_<LevelPortal>(module, "LevelPortal");
  portal
      .def(nb::new_([](const std::string& name,
                       const std::optional<std::string>& room_front,
                       const std::optional<std::string>& room_back,
                       PortalShape shape,
                       std::array<ArxVector3, 4>
                           vertices) {
             auto* value = new LevelPortal;
             value->name = canonicalIdentifier(name);
             value->room_front = canonicalOptionalIdentifier(room_front);
             value->room_back = canonicalOptionalIdentifier(room_back);
             value->shape = static_cast<ArxPortalShape>(shape);
             value->vertices = vertices;
             return value;
           }),
           nb::kw_only(),
           nb::arg("name") = "",
           nb::arg("room_front") = nb::none(),
           nb::arg("room_back") = nb::none(),
           nb::arg("shape") = PortalShape::kQuad,
           nb::arg("vertices") = std::array<ArxVector3, 4>{})
      .def_prop_rw(
          "name",
          [](const LevelPortal& value) { return value.name; },
          [](LevelPortal& value, std::string_view name) { value.name = canonicalIdentifier(name); })
      .def_prop_rw(
          "room_front",
          [](const LevelPortal& value) { return value.room_front; },
          [](LevelPortal& value, const std::optional<std::string>& room) {
            value.room_front = canonicalOptionalIdentifier(room);
          })
      .def_prop_rw(
          "room_back",
          [](const LevelPortal& value) { return value.room_back; },
          [](LevelPortal& value, const std::optional<std::string>& room) {
            value.room_back = canonicalOptionalIdentifier(room);
          })
      .def_prop_rw(
          "shape",
          [](const LevelPortal& value) { return static_cast<PortalShape>(value.shape); },
          [](LevelPortal& value, PortalShape shape) { value.shape = static_cast<ArxPortalShape>(shape); })
      .def_prop_rw(
          "vertices",
          [](const LevelPortal& value) { return fixedArray(value.vertices); },
          [](LevelPortal& value, const nb::sequence& vertices) { setFixedArray(value.vertices, vertices); },
          nb::for_getter(nb::sig("def vertices(self) -> tuple[Vector3, Vector3, Vector3, Vector3]")),
          nb::for_setter(nb::sig("def vertices(self, value: Sequence[Vector3], /) -> None")));
  nb::class_<LevelAnchor>(module, "LevelAnchor")
      .def(nb::new_(
               [](ArxVector3 position, float radius, float height, AnchorFlagBitmask flags, const std::string& name) {
                 auto* value = new LevelAnchor;
                 value->position = position;
                 value->radius = radius;
                 value->height = height;
                 value->flags = static_cast<std::int16_t>(flags);
                 value->name = name.empty() ? std::string{} : canonicalIdentifier(name);
                 return value;
               }),
           nb::kw_only(),
           nb::arg("position") = ArxVector3{},
           nb::arg("radius") = kDefaultAnchorRadius,
           nb::arg("height") = kDefaultAnchorHeight,
           nb::arg("flags") = static_cast<AnchorFlagBitmask>(0),
           nb::arg("name") = "")
      .def_rw("position", &LevelAnchor::position)
      .def_rw("radius", &LevelAnchor::radius)
      .def_rw("height", &LevelAnchor::height)
      .def_prop_rw(
          "flags",
          [](const LevelAnchor& value) { return static_cast<AnchorFlagBitmask>(value.flags); },
          [](LevelAnchor& value, AnchorFlagBitmask flags) { value.flags = static_cast<std::int16_t>(flags); })
      .def_prop_rw(
          "name",
          [](const LevelAnchor& value) { return value.name; },
          [](LevelAnchor& value, std::string_view name) {
            value.name = name.empty() ? std::string{} : canonicalIdentifier(name);
          });
  nb::class_<LevelLight>(module, "LevelLight")
      .def(nb::new_([](const std::string& name,
                       ArxVector3 position,
                       ArxColor3 color,
                       float fall_start,
                       float fall_end,
                       float intensity,
                       ArxColor3 flicker,
                       float effect_radius,
                       float effect_frequency,
                       float effect_size,
                       float effect_speed,
                       float flare_size,
                       LightFlagBitmask flags) {
             auto* value = new LevelLight;
             value->name = canonicalIdentifier(name);
             value->position = position;
             value->color = color;
             value->fallstart = fall_start;
             value->fallend = fall_end;
             value->intensity = intensity;
             value->flicker = flicker;
             value->effect_radius = effect_radius;
             value->effect_frequency = effect_frequency;
             value->effect_size = effect_size;
             value->effect_speed = effect_speed;
             value->flare_size = flare_size;
             value->flags = flags;
             return value;
           }),
           nb::kw_only(),
           nb::arg("name") = "",
           nb::arg("position") = ArxVector3{},
           nb::arg("color") = ArxColor3{},
           nb::arg("fall_start") = 0.0f,
           nb::arg("fall_end") = 1.0f,
           nb::arg("intensity") = 0.0f,
           nb::arg("flicker") = ArxColor3{},
           nb::arg("effect_radius") = 0.0f,
           nb::arg("effect_frequency") = 0.0f,
           nb::arg("effect_size") = 0.0f,
           nb::arg("effect_speed") = 0.0f,
           nb::arg("flare_size") = 0.0f,
           nb::arg("flags") = static_cast<LightFlagBitmask>(0))
      .def_prop_rw(
          "name",
          [](const LevelLight& value) { return value.name; },
          [](LevelLight& value, std::string_view name) { value.name = canonicalIdentifier(name); })
      .def_rw("position", &LevelLight::position)
      .def_rw("color", &LevelLight::color)
      .def_rw("fall_start", &LevelLight::fallstart)
      .def_rw("fall_end", &LevelLight::fallend)
      .def_rw("intensity", &LevelLight::intensity)
      .def_rw("flicker", &LevelLight::flicker)
      .def_rw("effect_radius", &LevelLight::effect_radius)
      .def_rw("effect_frequency", &LevelLight::effect_frequency)
      .def_rw("effect_size", &LevelLight::effect_size)
      .def_rw("effect_speed", &LevelLight::effect_speed)
      .def_rw("flare_size", &LevelLight::flare_size)
      .def_prop_rw(
          "flags",
          [](const LevelLight& value) { return static_cast<LightFlagBitmask>(value.flags); },
          [](LevelLight& value, LightFlagBitmask flags) { value.flags = flags; });
  nb::class_<LevelEntity>(module, "LevelEntity")
      .def(nb::new_([](const std::string& class_path,
                       std::int32_t ident,
                       ArxVector3 position,
                       ArxQuat rotation,
                       const std::string& name) {
             auto* value = new LevelEntity;
             value->class_path = canonicalEntityClassPath(class_path);
             value->ident = ident;
             value->position = position;
             value->rotation = rotation;
             value->name = name.empty() ? std::string{} : canonicalIdentifier(name);
             return value;
           }),
           nb::kw_only(),
           nb::arg("class_path") = "",
           nb::arg("ident") = -1,
           nb::arg("position") = ArxVector3{},
           nb::arg("rotation") = ArxQuat{},
           nb::arg("name") = "")
      .def_prop_rw(
          "class_path",
          [](const LevelEntity& value) { return value.class_path; },
          [](LevelEntity& value, std::string_view path) { value.class_path = canonicalEntityClassPath(path); })
      .def_rw("ident", &LevelEntity::ident)
      .def_rw("position", &LevelEntity::position)
      .def_rw("rotation", &LevelEntity::rotation)
      .def_prop_rw(
          "name",
          [](const LevelEntity& value) { return value.name; },
          [](LevelEntity& value, std::string_view name) {
            value.name = name.empty() ? std::string{} : canonicalIdentifier(name);
          });
  nb::class_<LevelFog>(module, "LevelFog")
      .def(nb::new_([](ArxVector3 position,
                       ArxColor3 color,
                       float size,
                       bool directional,
                       float scale,
                       ArxQuat rotation,
                       float speed,
                       float rotate_speed,
                       std::int32_t lifetime_ms,
                       float frequency,
                       const std::string& name) {
             auto* value = new LevelFog;
             value->position = position;
             value->color = color;
             value->size = size;
             value->directional = directional;
             value->scale = scale;
             value->rotation = rotation;
             value->speed = speed;
             value->rotate_speed = rotate_speed;
             value->lifetime_ms = lifetime_ms;
             value->frequency = frequency;
             value->name = name.empty() ? std::string{} : canonicalIdentifier(name);
             return value;
           }),
           nb::kw_only(),
           nb::arg("position") = ArxVector3{},
           nb::arg("color") = ArxColor3{},
           nb::arg("size") = 0.0f,
           nb::arg("directional") = false,
           nb::arg("scale") = 0.0f,
           nb::arg("rotation") = ArxQuat{},
           nb::arg("speed") = 0.0f,
           nb::arg("rotate_speed") = 0.0f,
           nb::arg("lifetime_ms") = 0,
           nb::arg("frequency") = 0.0f,
           nb::arg("name") = "")
      .def_rw("position", &LevelFog::position)
      .def_rw("color", &LevelFog::color)
      .def_rw("size", &LevelFog::size)
      .def_rw("directional", &LevelFog::directional)
      .def_rw("scale", &LevelFog::scale)
      .def_rw("rotation", &LevelFog::rotation)
      .def_rw("speed", &LevelFog::speed)
      .def_rw("rotate_speed", &LevelFog::rotate_speed)
      .def_rw("lifetime_ms", &LevelFog::lifetime_ms)
      .def_rw("frequency", &LevelFog::frequency)
      .def_prop_rw(
          "name",
          [](const LevelFog& value) { return value.name; },
          [](LevelFog& value, std::string_view name) {
            value.name = name.empty() ? std::string{} : canonicalIdentifier(name);
          });
  nb::class_<LevelZone>(module, "LevelZone")
      .def(nb::new_([](const std::string& name,
                       std::vector<ArxVector2>
                           perimeter_xz,
                       float reference_y,
                       ZoneHeightMode height_mode,
                       float height,
                       std::optional<ArxColor3>
                           color,
                       std::optional<float>
                           far_clip,
                       std::optional<LevelZoneAmbiance>
                           ambiance) {
             auto* value = new LevelZone;
             value->name = canonicalLowerIdentifier(name);
             value->perimeter_xz = std::move(perimeter_xz);
             value->reference_y = reference_y;
             value->height_mode = static_cast<ArxZoneHeightMode>(height_mode);
             value->height = height;
             value->color = color;
             value->farclip = far_clip;
             value->ambiance = std::move(ambiance);
             return value;
           }),
           nb::kw_only(),
           nb::arg("name") = "",
           nb::arg("perimeter_xz") = nb::tuple(),
           nb::arg("reference_y") = 0.0f,
           nb::arg("height_mode") = ZoneHeightMode::kFinite,
           nb::arg("height") = 0.0f,
           nb::arg("color") = nb::none(),
           nb::arg("far_clip") = nb::none(),
           nb::arg("ambiance") = nb::none())
      .def_prop_rw(
          "name",
          [](const LevelZone& value) { return value.name; },
          [](LevelZone& value, std::string_view name) { value.name = canonicalLowerIdentifier(name); })
      .def_prop_rw(
          "perimeter_xz",
          [](const LevelZone& value) { return snapshot(value.perimeter_xz); },
          [](LevelZone& value, std::vector<ArxVector2> perimeter) { value.perimeter_xz = std::move(perimeter); },
          nb::for_getter(nb::sig("def perimeter_xz(self) -> tuple[Vector2, ...]")),
          nb::for_setter(nb::sig("def perimeter_xz(self, value: Sequence[Vector2], /) -> None")))
      .def_rw("reference_y", &LevelZone::reference_y)
      .def_prop_rw(
          "height_mode",
          [](const LevelZone& value) { return static_cast<ZoneHeightMode>(value.height_mode); },
          [](LevelZone& value, ZoneHeightMode mode) { value.height_mode = static_cast<ArxZoneHeightMode>(mode); })
      .def_rw("height", &LevelZone::height)
      .def_rw("color", &LevelZone::color)
      .def_rw("far_clip", &LevelZone::farclip)
      .def_rw("ambiance", &LevelZone::ambiance);
  nb::class_<LevelPath>(module, "LevelPath")
      .def(nb::new_([](const std::string& name, ArxVector3 position, std::vector<ArxLevelPathNode> nodes) {
             auto* value = new LevelPath;
             value->name = canonicalLowerIdentifier(name);
             value->position = position;
             value->nodes = std::move(nodes);
             return value;
           }),
           nb::kw_only(),
           nb::arg("name") = "",
           nb::arg("position") = ArxVector3{},
           nb::arg("nodes") = nb::tuple())
      .def_prop_rw(
          "name",
          [](const LevelPath& value) { return value.name; },
          [](LevelPath& value, std::string_view name) { value.name = canonicalLowerIdentifier(name); })
      .def_rw("position", &LevelPath::position)
      .def_prop_rw(
          "nodes",
          [](const LevelPath& value) { return snapshot(value.nodes); },
          [](LevelPath& value, std::vector<ArxLevelPathNode> nodes) { value.nodes = std::move(nodes); },
          nb::for_getter(nb::sig("def nodes(self) -> tuple[LevelPathNode, ...]")),
          nb::for_setter(nb::sig("def nodes(self, value: Sequence[LevelPathNode], /) -> None")));
}

}  // namespace

void bindResourceValues(nb::module_& module) {
  try {
    bindFlags(module);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("flags: ") + error.what());
  }
  try {
    bindOwnedMedia(module);
    bindSidecarSequences(module);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("media: ") + error.what());
  }
  try {
    bindModelValues(module);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("model values: ") + error.what());
  }
  try {
    bindAnimationAmbianceValues(module);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("animation/ambiance values: ") + error.what());
  }
  try {
    bindCinematicValues(module);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("cinematic values: ") + error.what());
  }
  try {
    bindLevelValues(module);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("level values: ") + error.what());
  }
  module.attr("SOUND_EFFECTS_LANGUAGE_ID") = nb::int_(kSoundEffects);
}

}  // namespace pistoris::python
