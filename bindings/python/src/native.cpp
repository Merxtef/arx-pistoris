// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "binding_utils.h"
#include "bindings.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <nanobind/stl/bind_map.h>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

NB_MAKE_OPAQUE(std::vector<std::int32_t>);
NB_MAKE_OPAQUE(std::vector<pistoris::ArxVector3>);
NB_MAKE_OPAQUE(std::vector<pistoris::ArxColor3>);
NB_MAKE_OPAQUE(std::vector<pistoris::amb::Key>);
NB_MAKE_OPAQUE(std::vector<pistoris::amb::Track>);
NB_MAKE_OPAQUE(std::vector<pistoris::cin::Bitmap>);
NB_MAKE_OPAQUE(std::vector<pistoris::cin::Sound>);
NB_MAKE_OPAQUE(std::vector<pistoris::cin::Keyframe>);
NB_MAKE_OPAQUE(std::vector<pistoris::ftl::Vertex>);
NB_MAKE_OPAQUE(std::vector<pistoris::ftl::Face>);
NB_MAKE_OPAQUE(std::vector<pistoris::ftl::TextureContainer>);
NB_MAKE_OPAQUE(std::vector<pistoris::ftl::Group>);
NB_MAKE_OPAQUE(std::vector<pistoris::ftl::Action>);
NB_MAKE_OPAQUE(std::vector<pistoris::ftl::Selection>);
NB_MAKE_OPAQUE(std::vector<pistoris::tea::GroupAnim>);
NB_MAKE_OPAQUE(std::vector<pistoris::tea::Keyframe>);
NB_MAKE_OPAQUE(std::vector<pistoris::dlf::Entity>);
NB_MAKE_OPAQUE(std::vector<pistoris::dlf::Fog>);
NB_MAKE_OPAQUE(std::vector<pistoris::dlf::Zone>);
NB_MAKE_OPAQUE(std::vector<pistoris::dlf::PathNode>);
NB_MAKE_OPAQUE(std::vector<pistoris::dlf::Path>);
NB_MAKE_OPAQUE(std::vector<pistoris::llf::Light>);
NB_MAKE_OPAQUE(std::vector<pistoris::fts::Poly>);
NB_MAKE_OPAQUE(std::vector<pistoris::fts::Cell>);
NB_MAKE_OPAQUE(std::vector<pistoris::fts::Anchor>);
NB_MAKE_OPAQUE(std::vector<pistoris::fts::Portal>);
NB_MAKE_OPAQUE(std::vector<pistoris::fts::Room>);
NB_MAKE_OPAQUE(std::vector<pistoris::fts::EpData>);
NB_MAKE_OPAQUE(std::vector<pistoris::fts::RoomDistData>);
NB_MAKE_OPAQUE(std::unordered_map<std::int32_t, pistoris::fts::Texture>);

namespace pistoris::python {
namespace {

template <class T, std::size_t Size>
nb::tuple fixedArray(const T (&values)[Size]) {
  nb::list result;
  for (const T& value : values) result.append(value);
  return nb::tuple(result);
}

template <class T, std::size_t Size>
void setFixedArray(T (&destination)[Size], const nb::sequence& values) {
  if (nb::len(values) != Size) throw nb::value_error("native array has the wrong length");
  for (std::size_t index = 0; index < Size; ++index) destination[index] = nb::cast<T>(values[index]);
}

template <class Owner, class Value>
void bindMutableMember(nb::class_<Owner>& binding, const char* name, Value Owner::* member) {
  binding.def_prop_rw(
      name,
      [member](Owner& owner) -> Value& { return owner.*member; },
      [member](Owner& owner, const Value& value) { owner.*member = value; });
}

template <class Vector>
void bindNativeVector(nb::module_& module, const char* name) {
  auto binding = nb::bind_vector<Vector, nb::rv_policy::reference_internal>(module, name);
  registerMutableSequence(binding);
}

void bindContainers(nb::module_& module) {
  bindNativeVector<std::vector<std::int32_t>>(module, "Int32List");
  bindNativeVector<std::vector<ArxVector3>>(module, "Vector3List");
  bindNativeVector<std::vector<ArxColor3>>(module, "Color3List");
  bindNativeVector<std::vector<amb::Key>>(module, "AmbKeyList");
  bindNativeVector<std::vector<amb::Track>>(module, "AmbTrackList");
  bindNativeVector<std::vector<cin::Bitmap>>(module, "CinBitmapList");
  bindNativeVector<std::vector<cin::Sound>>(module, "CinSoundList");
  bindNativeVector<std::vector<cin::Keyframe>>(module, "CinKeyframeList");
  bindNativeVector<std::vector<ftl::Vertex>>(module, "FtlVertexList");
  bindNativeVector<std::vector<ftl::Face>>(module, "FtlFaceList");
  bindNativeVector<std::vector<ftl::TextureContainer>>(module, "FtlTextureList");
  bindNativeVector<std::vector<ftl::Group>>(module, "FtlGroupList");
  bindNativeVector<std::vector<ftl::Action>>(module, "FtlActionList");
  bindNativeVector<std::vector<ftl::Selection>>(module, "FtlSelectionList");
  bindNativeVector<std::vector<tea::GroupAnim>>(module, "TeaGroupList");
  bindNativeVector<std::vector<tea::Keyframe>>(module, "TeaKeyframeList");
  bindNativeVector<std::vector<dlf::Entity>>(module, "DlfEntityList");
  bindNativeVector<std::vector<dlf::Fog>>(module, "DlfFogList");
  bindNativeVector<std::vector<dlf::Zone>>(module, "DlfZoneList");
  bindNativeVector<std::vector<dlf::PathNode>>(module, "DlfPathNodeList");
  bindNativeVector<std::vector<dlf::Path>>(module, "DlfPathList");
  bindNativeVector<std::vector<llf::Light>>(module, "LlfLightList");
  bindNativeVector<std::vector<fts::Poly>>(module, "FtsPolyList");
  bindNativeVector<std::vector<fts::Cell>>(module, "FtsCellList");
  bindNativeVector<std::vector<fts::Anchor>>(module, "FtsAnchorList");
  bindNativeVector<std::vector<fts::Portal>>(module, "FtsPortalList");
  bindNativeVector<std::vector<fts::Room>>(module, "FtsRoomList");
  bindNativeVector<std::vector<fts::EpData>>(module, "FtsRoomPolygonList");
  bindNativeVector<std::vector<fts::RoomDistData>>(module, "FtsRoomDistanceList");
  auto textures = nb::bind_map<std::unordered_map<std::int32_t, fts::Texture>>(module, "FtsTextureMap");
  registerMutableMapping(textures);
}

void bindAmb(nb::module_& native) {
  nb::module_ module = native.def_submodule("amb");
  nb::class_<amb::Setting>(module, "Setting")
      .def(nb::init<>())
      .def_rw("min", &amb::Setting::min)
      .def_rw("max", &amb::Setting::max)
      .def_rw("interval_ms", &amb::Setting::interval_ms)
      .def_rw("flags", &amb::Setting::flags);
  auto key = nb::class_<amb::Key>(module, "Key");
  key.def(nb::init<>())
      .def_rw("start_ms", &amb::Key::start_ms)
      .def_rw("loop_minus_one", &amb::Key::loop_minus_one)
      .def_rw("delay_min_ms", &amb::Key::delay_min_ms)
      .def_rw("delay_max_ms", &amb::Key::delay_max_ms);
  bindMutableMember(key, "volume", &amb::Key::volume);
  bindMutableMember(key, "pitch", &amb::Key::pitch);
  bindMutableMember(key, "pan", &amb::Key::pan);
  bindMutableMember(key, "x", &amb::Key::x);
  bindMutableMember(key, "y", &amb::Key::y);
  bindMutableMember(key, "z", &amb::Key::z);
  auto track = nb::class_<amb::Track>(module, "Track");
  track.def(nb::init<>())
      .def_prop_rw(
          "sample_path",
          [](const amb::Track& value) { return stringBytes(value.sample_path); },
          [](amb::Track& value, const nb::bytes& path) { setStringBytes(value.sample_path, path); })
      .def_rw("flags", &amb::Track::flags);
  bindMutableMember(track, "keys", &amb::Track::keys);
  auto data = nb::class_<Amb>(module, "Data");
  data.def(nb::init<>());
  bindMutableMember(data, "tracks", &Amb::tracks);

  module.attr("TRACK_POSITION") = amb::kTrackPosition;
  module.attr("TRACK_MASTER") = amb::kTrackMaster;
  module.attr("SETTING_RANDOM") = amb::kSettingRandom;
  module.attr("SETTING_INTERPOLATE") = amb::kSettingInterpolate;
  module.def(
      "read",
      [](nb::handle data) {
        const auto bytes = byteSpan(data);
        auto result = [&] {
          nb::gil_scoped_release release;
          return readAmb(bytes);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("data"));
  module.def(
      "write",
      [](const Amb& data) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return writeAmb(data);
        }();
        return toBytes(unwrap(std::move(result)));
      },
      nb::arg("data"));
  module.def(
      "validate",
      [](const Amb& data) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::validate(data);
        }();
        unwrap(std::move(result));
      },
      nb::arg("data"));
  module.def(
      "to_json",
      [](const Amb& data, bool pretty, NativeTextMode mode) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::toAmbJson(data, pretty, mode);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("data"),
      nb::arg("pretty") = false,
      nb::arg("text_mode") = NativeTextMode::kAuto);
  module.def(
      "from_json",
      [](const std::string& json, NativeTextMode mode) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::fromAmbJson(json, mode);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("json"),
      nb::arg("text_mode") = NativeTextMode::kUtf8);
}

void bindCin(nb::module_& native) {
  nb::module_ module = native.def_submodule("cin");
  nb::class_<cin::Bitmap>(module, "Bitmap")
      .def(nb::init<>())
      .def_rw("subdivision_scale", &cin::Bitmap::subdivision_scale)
      .def_prop_rw(
          "path",
          [](const cin::Bitmap& value) { return stringBytes(value.path); },
          [](cin::Bitmap& value, const nb::bytes& path) { setStringBytes(value.path, path); });
  nb::class_<cin::Sound>(module, "Sound")
      .def(nb::init<>())
      .def_prop_rw(
          "path",
          [](const cin::Sound& value) { return stringBytes(value.path); },
          [](cin::Sound& value, const nb::bytes& path) { setStringBytes(value.path, path); })
      .def_rw("speech", &cin::Sound::speech);
  nb::class_<cin::Light>(module, "Light")
      .def(nb::init<>())
      .def_rw("position", &cin::Light::position)
      .def_rw("fall_in", &cin::Light::fall_in)
      .def_rw("fall_out", &cin::Light::fall_out)
      .def_rw("color", &cin::Light::color)
      .def_rw("intensity", &cin::Light::intensity)
      .def_rw("random_intensity", &cin::Light::random_intensity);
  auto keyframe = nb::class_<cin::Keyframe>(module, "Keyframe");
  keyframe.def(nb::init<>())
      .def_rw("frame", &cin::Keyframe::frame)
      .def_rw("bitmap", &cin::Keyframe::bitmap)
      .def_rw("effects", &cin::Keyframe::effects)
      .def_rw("interpolation", &cin::Keyframe::interpolation)
      .def_rw("crossfade", &cin::Keyframe::crossfade)
      .def_rw("camera_position", &cin::Keyframe::camera_position)
      .def_rw("camera_roll", &cin::Keyframe::camera_roll)
      .def_rw("color", &cin::Keyframe::color)
      .def_rw("secondary_color", &cin::Keyframe::secondary_color)
      .def_rw("flash_color", &cin::Keyframe::flash_color)
      .def_rw("flash_decay", &cin::Keyframe::flash_decay)
      .def_rw("bitmap_position", &cin::Keyframe::bitmap_position)
      .def_rw("bitmap_roll", &cin::Keyframe::bitmap_roll)
      .def_rw("outgoing_speed", &cin::Keyframe::outgoing_speed)
      .def_rw("sound", &cin::Keyframe::sound);
  bindMutableMember(keyframe, "light", &cin::Keyframe::light);
  auto data = nb::class_<Cin>(module, "Data");
  data.def(nb::init<>()).def_rw("end_frame", &Cin::end_frame).def_rw("fps", &Cin::fps);
  bindMutableMember(data, "bitmaps", &Cin::bitmaps);
  bindMutableMember(data, "sounds", &Cin::sounds);
  bindMutableMember(data, "keyframes", &Cin::keyframes);
  module.def(
      "read",
      [](nb::handle data) {
        const auto bytes = byteSpan(data);
        auto result = [&] {
          nb::gil_scoped_release release;
          return readCin(bytes);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("data"));
  module.def(
      "write",
      [](const Cin& data) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return writeCin(data);
        }();
        return toBytes(unwrap(std::move(result)));
      },
      nb::arg("data"));
  module.def(
      "validate",
      [](const Cin& data) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::validate(data);
        }();
        unwrap(std::move(result));
      },
      nb::arg("data"));
}

void bindFtl(nb::module_& native) {
  nb::module_ module = native.def_submodule("ftl");
  nb::class_<Vec3<std::uint16_t>>(module, "Index3")
      .def(nb::init<>())
      .def_rw("x", &Vec3<std::uint16_t>::x)
      .def_rw("y", &Vec3<std::uint16_t>::y)
      .def_rw("z", &Vec3<std::uint16_t>::z);
  nb::class_<ftl::Vertex>(module, "Vertex")
      .def(nb::init<>())
      .def_rw("position", &ftl::Vertex::position)
      .def_rw("normal", &ftl::Vertex::normal);
  nb::class_<ftl::Face>(module, "Face")
      .def(nb::init<>())
      .def_rw("type", &ftl::Face::type)
      .def_rw("vertex_idx", &ftl::Face::vertex_idx)
      .def_rw("texture_id", &ftl::Face::texture_id)
      .def_rw("u", &ftl::Face::u)
      .def_rw("v", &ftl::Face::v)
      .def_rw("transval", &ftl::Face::transval)
      .def_rw("normal", &ftl::Face::norm);
  nb::class_<ftl::Header>(module, "Header")
      .def(nb::init<>())
      .def_rw("origin", &ftl::Header::origin)
      .def_prop_rw(
          "name",
          [](const ftl::Header& value) { return fixedStringBytes(value.name); },
          [](ftl::Header& value, const nb::bytes& name) { setFixedStringBytes(value.name, name); });
  nb::class_<ftl::TextureContainer>(module, "Texture")
      .def(nb::init<>())
      .def_prop_rw(
          "filename",
          [](const ftl::TextureContainer& value) { return fixedStringBytes(value.filename); },
          [](ftl::TextureContainer& value, const nb::bytes& filename) {
            setFixedStringBytes(value.filename, filename);
          });
  auto group = nb::class_<ftl::Group>(module, "Group");
  group.def(nb::init<>())
      .def_prop_rw(
          "name",
          [](const ftl::Group& value) { return fixedStringBytes(value.name); },
          [](ftl::Group& value, const nb::bytes& name) { setFixedStringBytes(value.name, name); })
      .def_rw("origin", &ftl::Group::origin)
      .def_rw("blob_shadow_size", &ftl::Group::blob_shadow_size);
  bindMutableMember(group, "indices", &ftl::Group::indices);
  nb::class_<ftl::Action>(module, "Action")
      .def(nb::init<>())
      .def_prop_rw(
          "name",
          [](const ftl::Action& value) { return fixedStringBytes(value.name); },
          [](ftl::Action& value, const nb::bytes& name) { setFixedStringBytes(value.name, name); })
      .def_rw("vertex_idx", &ftl::Action::vertex_idx)
      .def_rw("action", &ftl::Action::action)
      .def_rw("sfx", &ftl::Action::sfx);
  auto selection = nb::class_<ftl::Selection>(module, "Selection");
  selection.def(nb::init<>())
      .def_prop_rw(
          "name",
          [](const ftl::Selection& value) { return fixedStringBytes(value.name); },
          [](ftl::Selection& value, const nb::bytes& name) { setFixedStringBytes(value.name, name); });
  bindMutableMember(selection, "selected", &ftl::Selection::selected);
  auto data = nb::class_<Ftl>(module, "Data");
  data.def(nb::init<>());
  bindMutableMember(data, "header", &Ftl::header);
  bindMutableMember(data, "vertices", &Ftl::vertices);
  bindMutableMember(data, "faces", &Ftl::faces);
  bindMutableMember(data, "textures", &Ftl::texture_containers);
  bindMutableMember(data, "groups", &Ftl::groups);
  bindMutableMember(data, "actions", &Ftl::actions);
  bindMutableMember(data, "selections", &Ftl::selections);
  module.def(
      "read",
      [](nb::handle data) {
        const auto bytes = byteSpan(data);
        auto result = [&] {
          nb::gil_scoped_release release;
          return readFtl(bytes);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("data"));
  module.def(
      "write",
      [](const Ftl& data, bool compress) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return writeFtl(data, compress);
        }();
        return toBytes(unwrap(std::move(result)));
      },
      nb::arg("data"),
      nb::arg("compress") = true);
  module.def(
      "validate",
      [](const Ftl& data) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::validate(data);
        }();
        unwrap(std::move(result));
      },
      nb::arg("data"));
  module.def(
      "to_json",
      [](const Ftl& data, bool pretty, NativeTextMode mode) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::toFtlJson(data, pretty, mode);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("data"),
      nb::arg("pretty") = false,
      nb::arg("text_mode") = NativeTextMode::kAuto);
  module.def(
      "from_json",
      [](const std::string& json, NativeTextMode mode) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::fromFtlJson(json, mode);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("json"),
      nb::arg("text_mode") = NativeTextMode::kUtf8);
}

void bindTea(nb::module_& native) {
  nb::module_ module = native.def_submodule("tea");
  nb::class_<tea::Sample>(module, "Sample")
      .def(nb::init<>())
      .def_prop_rw(
          "name",
          [](const tea::Sample& value) { return fixedStringBytes(value.name); },
          [](tea::Sample& value, const nb::bytes& name) { setFixedStringBytes(value.name, name); });
  nb::class_<tea::GroupAnim>(module, "GroupTransform")
      .def(nb::init<>())
      .def_rw("key_group", &tea::GroupAnim::key_group)
      .def_rw("quat", &tea::GroupAnim::quat)
      .def_rw("translate", &tea::GroupAnim::translate)
      .def_rw("zoom", &tea::GroupAnim::zoom);
  auto keyframe = nb::class_<tea::Keyframe>(module, "Keyframe");
  keyframe.def(nb::init<>())
      .def_rw("num_frame", &tea::Keyframe::num_frame)
      .def_rw("flag_frame", &tea::Keyframe::flag_frame)
      .def_rw("translate", &tea::Keyframe::translate)
      .def_rw("quat", &tea::Keyframe::quat)
      .def_rw("sample", &tea::Keyframe::sample);
  bindMutableMember(keyframe, "groups", &tea::Keyframe::groups);
  auto data = nb::class_<Tea>(module, "Data");
  data.def(nb::init<>())
      .def_rw("num_frames", &Tea::num_frames)
      .def_rw("num_groups", &Tea::num_groups)
      .def_prop_rw(
          "name",
          [](const Tea& value) { return fixedStringBytes(value.name); },
          [](Tea& value, const nb::bytes& name) { setFixedStringBytes(value.name, name); });
  bindMutableMember(data, "keyframes", &Tea::keyframes);
  module.def(
      "read",
      [](nb::handle data) {
        const auto bytes = byteSpan(data);
        auto result = [&] {
          nb::gil_scoped_release release;
          return readTea(bytes);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("data"));
  module.def(
      "write",
      [](const Tea& data) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return writeTea(data);
        }();
        return toBytes(unwrap(std::move(result)));
      },
      nb::arg("data"));
  module.def(
      "validate",
      [](const Tea& data) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::validate(data);
        }();
        unwrap(std::move(result));
      },
      nb::arg("data"));
  module.def(
      "to_json",
      [](const Tea& data, bool pretty, NativeTextMode mode) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::toTeaJson(data, pretty, mode);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("data"),
      nb::arg("pretty") = false,
      nb::arg("text_mode") = NativeTextMode::kAuto);
  module.def(
      "from_json",
      [](const std::string& json, NativeTextMode mode) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::fromTeaJson(json, mode);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("json"),
      nb::arg("text_mode") = NativeTextMode::kUtf8);
}

void bindDlf(nb::module_& native) {
  nb::module_ module = native.def_submodule("dlf");
  nb::class_<dlf::PlayerSpawn>(module, "PlayerSpawn")
      .def(nb::init<>())
      .def_rw("position", &dlf::PlayerSpawn::position)
      .def_rw("angle", &dlf::PlayerSpawn::angle);
  nb::class_<dlf::Entity>(module, "Entity")
      .def(nb::init<>())
      .def_prop_rw(
          "class_path",
          [](const dlf::Entity& value) { return fixedStringBytes(value.class_path); },
          [](dlf::Entity& value, const nb::bytes& path) { setFixedStringBytes(value.class_path, path); })
      .def_rw("ident", &dlf::Entity::ident)
      .def_rw("position", &dlf::Entity::position)
      .def_rw("angle", &dlf::Entity::angle);
  nb::class_<dlf::Fog>(module, "Fog")
      .def(nb::init<>())
      .def_rw("position", &dlf::Fog::position)
      .def_rw("color", &dlf::Fog::color)
      .def_rw("size", &dlf::Fog::size)
      .def_rw("directional", &dlf::Fog::directional)
      .def_rw("scale", &dlf::Fog::scale)
      .def_rw("angle", &dlf::Fog::angle)
      .def_rw("speed", &dlf::Fog::speed)
      .def_rw("rotate_speed", &dlf::Fog::rotate_speed)
      .def_rw("lifetime_ms", &dlf::Fog::lifetime_ms)
      .def_rw("frequency", &dlf::Fog::frequency);
  nb::class_<dlf::ZoneAmbiance>(module, "ZoneAmbiance")
      .def(nb::init<>())
      .def_prop_rw(
          "name",
          [](const dlf::ZoneAmbiance& value) { return fixedStringBytes(value.name); },
          [](dlf::ZoneAmbiance& value, const nb::bytes& name) { setFixedStringBytes(value.name, name); })
      .def_rw("volume", &dlf::ZoneAmbiance::volume);
  auto zone = nb::class_<dlf::Zone>(module, "Zone");
  zone.def(nb::init<>())
      .def_prop_rw(
          "name",
          [](const dlf::Zone& value) { return fixedStringBytes(value.name); },
          [](dlf::Zone& value, const nb::bytes& name) { setFixedStringBytes(value.name, name); })
      .def_rw("position", &dlf::Zone::position)
      .def_rw("height", &dlf::Zone::height)
      .def_rw("color", &dlf::Zone::color)
      .def_rw("farclip", &dlf::Zone::farclip);
  bindMutableMember(zone, "ambiance", &dlf::Zone::ambiance);
  bindMutableMember(zone, "points", &dlf::Zone::points);
  nb::enum_<dlf::PathNodeType>(module, "PathNodeType")
      .value("STANDARD", dlf::PathNodeType::kStandard)
      .value("BEZIER", dlf::PathNodeType::kBezier)
      .value("CONTROL_POINT", dlf::PathNodeType::kControlPoint);
  nb::class_<dlf::PathNode>(module, "PathNode")
      .def(nb::init<>())
      .def_rw("relative_position", &dlf::PathNode::relative_position)
      .def_rw("type", &dlf::PathNode::type)
      .def_rw("time_ms", &dlf::PathNode::time_ms);
  auto path = nb::class_<dlf::Path>(module, "Path");
  path.def(nb::init<>())
      .def_prop_rw(
          "name",
          [](const dlf::Path& value) { return fixedStringBytes(value.name); },
          [](dlf::Path& value, const nb::bytes& name) { setFixedStringBytes(value.name, name); })
      .def_rw("position", &dlf::Path::position);
  bindMutableMember(path, "nodes", &dlf::Path::nodes);
  auto data = nb::class_<Dlf>(module, "Data");
  data.def(nb::init<>())
      .def_rw("version", &Dlf::version)
      .def_prop_rw(
          "scene_path",
          [](const Dlf& value) { return fixedStringBytes(value.scene_path); },
          [](Dlf& value, const nb::bytes& path) { setFixedStringBytes(value.scene_path, path); });
  bindMutableMember(data, "player_spawn", &Dlf::player_spawn);
  bindMutableMember(data, "entities", &Dlf::entities);
  bindMutableMember(data, "fogs", &Dlf::fogs);
  bindMutableMember(data, "zones", &Dlf::zones);
  bindMutableMember(data, "paths", &Dlf::paths);
  auto bundle = nb::class_<DlfBundle>(module, "Bundle");
  bundle.def(nb::init<>()).def_rw("embedded_lighting", &DlfBundle::embedded_lighting);
  bindMutableMember(bundle, "dlf", &DlfBundle::dlf);
  module.def(
      "read",
      [](nb::handle data) {
        const auto bytes = byteSpan(data);
        auto result = [&] {
          nb::gil_scoped_release release;
          return readDlf(bytes);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("data"));
  module.def(
      "write",
      [](const Dlf& data, const Llf* embedded_lighting, const std::string& signer, bool compress) {
        DlfWriteOptions options{.embedded_lighting = embedded_lighting, .signer = signer};
        auto result = [&] {
          nb::gil_scoped_release release;
          return writeDlf(data, options, compress);
        }();
        return toBytes(unwrap(std::move(result)));
      },
      nb::arg("data"),
      nb::arg("embedded_lighting") = nullptr,
      nb::arg("signer") = "",
      nb::arg("compress") = true);
  module.def(
      "validate",
      [](const Dlf& data) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::validate(data);
        }();
        unwrap(std::move(result));
      },
      nb::arg("data"));
  module.def(
      "to_json",
      [](const Dlf& data, bool pretty, const std::string& signer, NativeTextMode mode) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::toDlfJson(data, pretty, signer, mode);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("data"),
      nb::arg("pretty") = false,
      nb::arg("signer") = "",
      nb::arg("text_mode") = NativeTextMode::kAuto);
  module.def(
      "from_json",
      [](const std::string& json, NativeTextMode mode) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::fromDlfJson(json, mode);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("json"),
      nb::arg("text_mode") = NativeTextMode::kUtf8);
}

void bindLlf(nb::module_& native) {
  nb::module_ module = native.def_submodule("llf");
  nb::class_<llf::Light>(module, "Light")
      .def(nb::init<>())
      .def_rw("position", &llf::Light::position)
      .def_rw("color", &llf::Light::color)
      .def_rw("fallstart", &llf::Light::fallstart)
      .def_rw("fallend", &llf::Light::fallend)
      .def_rw("intensity", &llf::Light::intensity)
      .def_rw("flicker", &llf::Light::flicker)
      .def_rw("effect_radius", &llf::Light::effect_radius)
      .def_rw("effect_frequency", &llf::Light::effect_frequency)
      .def_rw("effect_size", &llf::Light::effect_size)
      .def_rw("effect_speed", &llf::Light::effect_speed)
      .def_rw("flare_size", &llf::Light::flare_size)
      .def_rw("flags", &llf::Light::flags);
  auto data = nb::class_<Llf>(module, "Data");
  data.def(nb::init<>()).def_rw("version", &Llf::version);
  bindMutableMember(data, "lights", &Llf::lights);
  bindMutableMember(data, "colors", &Llf::colors);
  module.def(
      "read",
      [](nb::handle data) {
        const auto bytes = byteSpan(data);
        auto result = [&] {
          nb::gil_scoped_release release;
          return readLlf(bytes);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("data"));
  module.def(
      "write",
      [](const Llf& data, const std::string& signer, bool compress) {
        LlfWriteOptions options{.signer = signer};
        auto result = [&] {
          nb::gil_scoped_release release;
          return writeLlf(data, options, compress);
        }();
        return toBytes(unwrap(std::move(result)));
      },
      nb::arg("data"),
      nb::arg("signer") = "",
      nb::arg("compress") = true);
  module.def(
      "validate",
      [](const Llf& data) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::validate(data);
        }();
        unwrap(std::move(result));
      },
      nb::arg("data"));
  module.def(
      "to_json",
      [](const Llf& data, bool pretty, const std::string& signer) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::toLlfJson(data, pretty, signer);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("data"),
      nb::arg("pretty") = false,
      nb::arg("signer") = "");
  module.def(
      "from_json",
      [](const std::string& json) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::fromLlfJson(json);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("json"));
}

void bindFts(nb::module_& native) {
  nb::module_ module = native.def_submodule("fts");
  nb::class_<fts::SceneHeader>(module, "SceneHeader")
      .def(nb::init<>())
      .def_rw("version", &fts::SceneHeader::version)
      .def_rw("size_x", &fts::SceneHeader::sizex)
      .def_rw("size_z", &fts::SceneHeader::sizez)
      .def_rw("num_textures", &fts::SceneHeader::num_textures)
      .def_rw("num_polys", &fts::SceneHeader::num_polys)
      .def_rw("num_anchors", &fts::SceneHeader::num_anchors)
      .def_rw("player_position", &fts::SceneHeader::playerpos)
      .def_rw("scene_position", &fts::SceneHeader::Mscenepos)
      .def_rw("num_portals", &fts::SceneHeader::num_portals)
      .def_rw("num_rooms", &fts::SceneHeader::num_rooms);
  nb::class_<fts::Texture>(module, "Texture")
      .def(nb::init<>())
      .def_rw("temp", &fts::Texture::temp)
      .def_prop_rw(
          "filename",
          [](const fts::Texture& value) { return fixedStringBytes(value.fic); },
          [](fts::Texture& value, const nb::bytes& path) { setFixedStringBytes(value.fic, path); });
  nb::class_<fts::Vertex>(module, "Vertex")
      .def(nb::init<>())
      .def_rw("y", &fts::Vertex::sy)
      .def_rw("x", &fts::Vertex::ssx)
      .def_rw("z", &fts::Vertex::ssz)
      .def_rw("u", &fts::Vertex::stu)
      .def_rw("v", &fts::Vertex::stv);
  nb::class_<fts::Poly>(module, "Polygon")
      .def(nb::init<>())
      .def_prop_rw(
          "vertices",
          [](const fts::Poly& value) { return fixedArray(value.v); },
          [](fts::Poly& value, const nb::sequence& vertices) { setFixedArray(value.v, vertices); },
          nb::for_getter(nb::sig("def vertices(self) -> tuple[Vertex, Vertex, Vertex, Vertex]")),
          nb::for_setter(nb::sig("def vertices(self, value: Sequence[Vertex], /) -> None")))
      .def_rw("texture", &fts::Poly::tex)
      .def_rw("normal", &fts::Poly::norm)
      .def_rw("normal2", &fts::Poly::norm2)
      .def_prop_rw(
          "vertex_normals",
          [](const fts::Poly& value) { return fixedArray(value.nrml); },
          [](fts::Poly& value, const nb::sequence& normals) { setFixedArray(value.nrml, normals); },
          nb::for_getter(nb::sig("def vertex_normals(self) -> tuple[pistoris._core.Vector3, pistoris._core.Vector3, "
                                 "pistoris._core.Vector3, pistoris._core.Vector3]")),
          nb::for_setter(nb::sig("def vertex_normals(self, value: Sequence[pistoris._core.Vector3], /) -> None")))
      .def_rw("transval", &fts::Poly::transval)
      .def_rw("area", &fts::Poly::area)
      .def_rw("type", &fts::Poly::type)
      .def_rw("room", &fts::Poly::room)
      .def_rw("paddy", &fts::Poly::paddy);
  nb::class_<fts::AnchorData>(module, "AnchorData")
      .def(nb::init<>())
      .def_rw("position", &fts::AnchorData::pos)
      .def_rw("radius", &fts::AnchorData::radius)
      .def_rw("height", &fts::AnchorData::height)
      .def_rw("num_linked", &fts::AnchorData::num_linked)
      .def_rw("flags", &fts::AnchorData::flags);
  nb::class_<fts::SavedTextureVertex>(module, "SavedTextureVertex")
      .def(nb::init<>())
      .def_rw("position", &fts::SavedTextureVertex::pos)
      .def_rw("rhw", &fts::SavedTextureVertex::rhw)
      .def_rw("color", &fts::SavedTextureVertex::color)
      .def_rw("specular", &fts::SavedTextureVertex::specular)
      .def_rw("u", &fts::SavedTextureVertex::tu)
      .def_rw("v", &fts::SavedTextureVertex::tv);
  nb::class_<fts::SavePoly>(module, "SavedPolygon")
      .def(nb::init<>())
      .def_rw("type", &fts::SavePoly::type)
      .def_rw("min", &fts::SavePoly::min)
      .def_rw("max", &fts::SavePoly::max)
      .def_rw("normal", &fts::SavePoly::norm)
      .def_rw("normal2", &fts::SavePoly::norm2)
      .def_prop_rw(
          "vertices",
          [](const fts::SavePoly& value) { return fixedArray(value.v); },
          [](fts::SavePoly& value, const nb::sequence& vertices) { setFixedArray(value.v, vertices); },
          nb::for_getter(
              nb::sig("def vertices(self) -> tuple[SavedTextureVertex, SavedTextureVertex, SavedTextureVertex, "
                      "SavedTextureVertex]")),
          nb::for_setter(nb::sig("def vertices(self, value: Sequence[SavedTextureVertex], /) -> None")))
      .def_prop_rw(
          "texture_vertices",
          [](const fts::SavePoly& value) { return fixedArray(value.tv); },
          [](fts::SavePoly& value, const nb::sequence& vertices) { setFixedArray(value.tv, vertices); },
          nb::for_getter(
              nb::sig("def texture_vertices(self) -> tuple[SavedTextureVertex, SavedTextureVertex, SavedTextureVertex, "
                      "SavedTextureVertex]")),
          nb::for_setter(nb::sig("def texture_vertices(self, value: Sequence[SavedTextureVertex], /) -> None")))
      .def_prop_rw(
          "vertex_normals",
          [](const fts::SavePoly& value) { return fixedArray(value.nrml); },
          [](fts::SavePoly& value, const nb::sequence& normals) { setFixedArray(value.nrml, normals); },
          nb::for_getter(nb::sig("def vertex_normals(self) -> tuple[pistoris._core.Vector3, pistoris._core.Vector3, "
                                 "pistoris._core.Vector3, pistoris._core.Vector3]")),
          nb::for_setter(nb::sig("def vertex_normals(self, value: Sequence[pistoris._core.Vector3], /) -> None")))
      .def_rw("texture", &fts::SavePoly::tex)
      .def_rw("center", &fts::SavePoly::center)
      .def_rw("transval", &fts::SavePoly::transval)
      .def_rw("area", &fts::SavePoly::area)
      .def_rw("room", &fts::SavePoly::room)
      .def_rw("misc", &fts::SavePoly::misc);
  auto portal = nb::class_<fts::Portal>(module, "Portal");
  portal.def(nb::init<>())
      .def_rw("room_1", &fts::Portal::room_1)
      .def_rw("room_2", &fts::Portal::room_2)
      .def_rw("use_portal", &fts::Portal::useportal)
      .def_rw("paddy", &fts::Portal::paddy);
  bindMutableMember(portal, "polygon", &fts::Portal::poly);
  nb::class_<fts::RoomData>(module, "RoomData")
      .def(nb::init<>())
      .def_rw("num_portals", &fts::RoomData::num_portals)
      .def_rw("num_polys", &fts::RoomData::num_polys)
      .def_prop_rw(
          "padding",
          [](const fts::RoomData& value) { return fixedArray(value.padd); },
          [](fts::RoomData& value, const nb::sequence& padding) { setFixedArray(value.padd, padding); },
          nb::for_getter(nb::sig("def padding(self) -> tuple[int, int, int, int, int, int]")),
          nb::for_setter(nb::sig("def padding(self, value: Sequence[int], /) -> None")));
  nb::class_<fts::EpData>(module, "RoomPolygon")
      .def(nb::init<>())
      .def_rw("x", &fts::EpData::px)
      .def_rw("y", &fts::EpData::py)
      .def_rw("index", &fts::EpData::idx)
      .def_rw("padding", &fts::EpData::padd);
  nb::class_<fts::RoomDistData>(module, "RoomDistance")
      .def(nb::init<>())
      .def_rw("distance", &fts::RoomDistData::distance)
      .def_rw("start_position", &fts::RoomDistData::startpos)
      .def_rw("end_position", &fts::RoomDistData::endpos);
  auto cell = nb::class_<fts::Cell>(module, "Cell");
  cell.def(nb::init<>());
  bindMutableMember(cell, "polygons", &fts::Cell::polygons);
  bindMutableMember(cell, "anchor_ids", &fts::Cell::anchor_ids);
  auto anchor = nb::class_<fts::Anchor>(module, "Anchor");
  anchor.def(nb::init<>());
  bindMutableMember(anchor, "data", &fts::Anchor::data);
  bindMutableMember(anchor, "linked", &fts::Anchor::linked);
  auto room = nb::class_<fts::Room>(module, "Room");
  room.def(nb::init<>());
  bindMutableMember(room, "data", &fts::Room::data);
  bindMutableMember(room, "portal_ids", &fts::Room::portal_ids);
  bindMutableMember(room, "polygons", &fts::Room::polygons);
  auto data = nb::class_<Fts>(module, "Data");
  data.def(nb::init<>());
  bindMutableMember(data, "scene", &Fts::scene);
  bindMutableMember(data, "textures", &Fts::textures);
  bindMutableMember(data, "cells", &Fts::cells);
  bindMutableMember(data, "anchors", &Fts::anchors);
  bindMutableMember(data, "portals", &Fts::portals);
  bindMutableMember(data, "rooms", &Fts::rooms);
  bindMutableMember(data, "room_distances", &Fts::room_distances);
  nb::class_<FtsJsonImport>(module, "JsonImport")
      .def_ro("data", &FtsJsonImport::fts)
      .def_ro("level", &FtsJsonImport::level);
  module.def(
      "read",
      [](nb::handle data) {
        const auto bytes = byteSpan(data);
        auto result = [&] {
          nb::gil_scoped_release release;
          return readFts(bytes);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("data"));
  module.def(
      "write",
      [](const Fts& data, bool compress) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return writeFts(data, compress);
        }();
        return toBytes(unwrap(std::move(result)));
      },
      nb::arg("data"),
      nb::arg("compress") = true);
  module.def(
      "validate",
      [](const Fts& data) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::validate(data);
        }();
        unwrap(std::move(result));
      },
      nb::arg("data"));
  module.def(
      "to_json",
      [](const Fts& data, std::uint32_t level, bool pretty, NativeTextMode mode) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::toFtsJson(data, level, pretty, mode);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("data"),
      nb::kw_only(),
      nb::arg("level"),
      nb::arg("pretty") = false,
      nb::arg("text_mode") = NativeTextMode::kAuto);
  module.def(
      "from_json",
      [](const std::string& json, NativeTextMode mode) {
        auto result = [&] {
          nb::gil_scoped_release release;
          return pistoris::fromFtsJson(json, mode);
        }();
        return unwrap(std::move(result));
      },
      nb::arg("json"),
      nb::arg("text_mode") = NativeTextMode::kUtf8);
}

}  // namespace

void bindNative(nb::module_& root) {
  nb::module_ native = root.def_submodule("native");
  bindContainers(native);
  bindAmb(native);
  bindCin(native);
  bindFtl(native);
  bindTea(native);
  bindDlf(native);
  bindLlf(native);
  bindFts(native);
}

}  // namespace pistoris::python
