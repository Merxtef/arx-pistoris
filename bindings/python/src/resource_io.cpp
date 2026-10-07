// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Merxtef

#include "arx_pistoris/resource_io.hpp"

#include "binding_utils.h"
#include "bindings.h"
#include "resource_state.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pistoris::python {
namespace {

std::string pathToUtf8(const std::filesystem::path& path) {
  const std::u8string encoded = path.u8string();
  return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

void warnMountValidation(const resource_io::MountValidationReport& report) {
  for (const resource_io::MountValidationMessage& message : report.messages) {
    if (message.kind != resource_io::MountValidationKind::kMissingReadMount) continue;
    const std::string warning = "read mount does not exist and was skipped: " + pathToUtf8(message.path);
    if (PyErr_WarnEx(PyExc_UserWarning, warning.c_str(), 2) < 0) throw nb::python_error();
  }
}

std::optional<resource_io::ResourceMount> mountForPath(const resource_io::ResourceMounts& mounts,
                                                       const std::filesystem::path& path) {
  for (const resource_io::ResourceMount& mount : mounts.readMounts()) {
    std::error_code error;
    if (std::filesystem::equivalent(mount.path, path, error) && !error) return mount;
  }
  return std::nullopt;
}

resource_io::ResourceMounts openMounts(const resource_io::ResourceMountOptions& options) {
  resource_io::MountValidationReport report;
  auto result = [&] {
    nb::gil_scoped_release release;
    return resource_io::ResourceMounts::open(options, &report);
  }();
  auto mounts = unwrap(std::move(result));
  warnMountValidation(report);
  return mounts;
}

paths::ResourceSelector parsedSelector(std::string_view selector) {
  paths::ResourceSelector result;
  if (!paths::parseResourceSelector(selector, result)) throw nb::value_error("invalid resource selector");
  return result;
}

std::optional<std::filesystem::path> nativePath(nb::handle value) {
  PyObject* encoded = PyOS_FSPath(value.ptr());
  if (!encoded) {
    PyErr_Clear();
    return std::nullopt;
  }
  nb::object owner = nb::steal<nb::object>(encoded);
  return nb::cast<std::filesystem::path>(owner);
}

std::optional<std::string> logicalPath(nb::handle value) {
  PyObject* encoded = PyOS_FSPath(value.ptr());
  if (!encoded) {
    PyErr_Clear();
    return std::nullopt;
  }
  nb::object owner = nb::steal<nb::object>(encoded);
  if (!nb::isinstance<nb::str>(owner)) return std::nullopt;
  return nb::cast<std::string>(owner);
}

std::string requiredLogicalPath(nb::handle value, std::string_view name) {
  auto path = logicalPath(value);
  if (path) return std::move(*path);
  std::string message(name);
  message += " must be str or PathLike[str]";
  throw nb::type_error(message.c_str());
}

std::string mountedTarget(nb::handle value, const nb::object& selector_type, std::string_view name) {
  if (nb::isinstance(value, selector_type)) return nb::cast<std::string>(value.attr("to_path")());
  return requiredLogicalPath(value, name);
}

std::optional<std::filesystem::path> optionalNativePath(nb::handle value, std::string_view name) {
  if (value.is_none()) return std::nullopt;
  auto path = nativePath(value);
  if (!path) {
    std::string message(name);
    message += " must be PathLike or None";
    throw nb::type_error(message.c_str());
  }
  return path;
}

std::optional<std::string> optionalLogicalPath(nb::handle value, std::string_view name) {
  if (value.is_none()) return std::nullopt;
  return requiredLogicalPath(value, name);
}

paths::ModelPathView modelPathView(nb::handle value, std::string& name, std::string& tweak) {
  name = nb::cast<std::string>(value.attr("name"));
  tweak = nb::cast<std::string>(value.attr("tweak"));
  return {nb::cast<paths::ModelPathType>(value.attr("type")), name, tweak};
}

paths::AnimationPathView animationPathView(nb::handle value, std::string& name) {
  name = nb::cast<std::string>(value.attr("name"));
  return {nb::cast<paths::AnimationPathType>(value.attr("type")), name};
}

template <class Resource, class Tracking, class Load>
std::shared_ptr<TrackedResource<Resource, Tracking>> loadTracked(Load&& load) {
  auto result = [&] {
    nb::gil_scoped_release release;
    return load();
  }();
  return tracked<Resource, Tracking>(unwrap(std::move(result)));
}

template <class Load>
ModelImportOutput loadModelImport(Load&& load) {
  auto result = [&] {
    nb::gil_scoped_release release;
    return load();
  }();
  resource_io::LoadedModel loaded = unwrap(std::move(result));
  ModelImportOutput output;
  output.model = tracked<Model, ModelTracking>(std::move(loaded.model));
  output.animations.reserve(loaded.animations.size());
  for (Animation& animation : loaded.animations)
    output.animations.push_back(tracked<Animation, AnimationTracking>(std::move(animation)));
  return output;
}

std::size_t catalogIndex(std::int64_t requested, std::size_t size) { return sequenceIndex(requested, size); }

resource_io::ResourceIoFlags ioFlags(bool recover_case_collisions) noexcept {
  return recover_case_collisions ? resource_io::kResourceIoRecoverCaseCollisions : resource_io::kResourceIoFlagNone;
}

resource_io::ResourceOutputOptions outputOptions(resource_io::ResourceOutputParts outputs,
                                                 bool recover_case_collisions) noexcept {
  return {.outputs = outputs, .io_flags = ioFlags(recover_case_collisions)};
}

enum class WriteTargetFormat : std::uint8_t { kNative, kJson, kObj, kGlb, kUnknown };

char lowerAscii(char value) noexcept {
  return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) : value;
}

bool endsWithAsciiInsensitive(std::string_view value, std::string_view suffix) noexcept {
  if (value.size() < suffix.size()) return false;
  value.remove_prefix(value.size() - suffix.size());
  for (std::size_t index = 0; index < value.size(); ++index)
    if (lowerAscii(value[index]) != lowerAscii(suffix[index])) return false;
  return true;
}

WriteTargetFormat writeTargetFormat(std::string_view path, WriteTargetFormat extensionless) noexcept {
  if (endsWithAsciiInsensitive(path, ".glb")) return WriteTargetFormat::kGlb;
  if (endsWithAsciiInsensitive(path, ".obj")) return WriteTargetFormat::kObj;
  if (endsWithAsciiInsensitive(path, ".json")) return WriteTargetFormat::kJson;
  constexpr std::string_view kNativeExtensions[] = {".ftl", ".tea", ".fts", ".dlf", ".llf", ".amb", ".cin"};
  for (std::string_view extension : kNativeExtensions)
    if (endsWithAsciiInsensitive(path, extension)) return WriteTargetFormat::kNative;
  const std::size_t separator = path.find_last_of("/\\");
  const std::size_t dot = path.find_last_of('.');
  return dot == std::string_view::npos || (separator != std::string_view::npos && dot < separator)
             ? extensionless
             : WriteTargetFormat::kUnknown;
}

const char* writeTargetFormatName(WriteTargetFormat format) noexcept {
  switch (format) {
    case WriteTargetFormat::kNative:
      return "native";
    case WriteTargetFormat::kJson:
      return "JSON";
    case WriteTargetFormat::kObj:
      return "OBJ";
    case WriteTargetFormat::kGlb:
      return "GLB";
    case WriteTargetFormat::kUnknown:
      return "selected";
  }
  return "selected";
}

template <class Value>
void requireApplicable(const std::optional<Value>& value, bool applicable, std::string_view name,
                       WriteTargetFormat format) {
  if (!value || applicable || format == WriteTargetFormat::kUnknown) return;
  const std::string message = std::string(name) + " does not apply to " + writeTargetFormatName(format) + " output";
  throw nb::value_error(message.c_str());
}

resource_io::ModelWriteOptions modelWriteOptions(std::string_view path, WriteTargetFormat extensionless,
                                                 const std::optional<NativeTextMode>& text_mode,
                                                 const std::optional<float>& units, const std::optional<bool>& compress,
                                                 resource_io::ResourceOutputParts outputs,
                                                 bool recover_case_collisions) {
  const WriteTargetFormat format = writeTargetFormat(path, extensionless);
  requireApplicable(
      text_mode, format == WriteTargetFormat::kNative || format == WriteTargetFormat::kJson, "text_mode", format);
  requireApplicable(units, format == WriteTargetFormat::kGlb, "arx_units_per_glb_unit", format);
  requireApplicable(compress, format == WriteTargetFormat::kNative, "compress", format);
  return {.resource = outputOptions(outputs, recover_case_collisions),
          .glb = units ? std::optional{Model::GlbExportOptions{.arx_units_per_glb_unit = *units}} : std::nullopt,
          .native_text_mode = text_mode,
          .compress = compress};
}

resource_io::AmbianceWriteOptions ambianceWriteOptions(std::string_view path,
                                                       const std::optional<NativeTextMode>& text_mode,
                                                       const std::optional<float>& units,
                                                       resource_io::ResourceOutputParts outputs,
                                                       bool recover_case_collisions) {
  const WriteTargetFormat format = writeTargetFormat(path, WriteTargetFormat::kNative);
  requireApplicable(
      text_mode, format == WriteTargetFormat::kNative || format == WriteTargetFormat::kJson, "text_mode", format);
  requireApplicable(units, format == WriteTargetFormat::kGlb, "arx_units_per_glb_unit", format);
  return {.resource = outputOptions(outputs, recover_case_collisions),
          .glb = units ? std::optional{Ambiance::GlbExportOptions{.arx_units_per_glb_unit = *units}} : std::nullopt,
          .native_text_mode = text_mode};
}

resource_io::CinematicWriteOptions cinematicWriteOptions(
    std::string_view path, const std::optional<NativeTextMode>& text_mode,
    const std::optional<CinematicIllustrationFormat>& illustration_format, resource_io::ResourceOutputParts outputs,
    bool recover_case_collisions) {
  const WriteTargetFormat format = writeTargetFormat(path, WriteTargetFormat::kNative);
  requireApplicable(text_mode, format == WriteTargetFormat::kNative, "text_mode", format);
  requireApplicable(illustration_format, format == WriteTargetFormat::kNative, "illustration_format", format);
  return {.resource = outputOptions(outputs, recover_case_collisions),
          .native_text_mode = text_mode,
          .illustration_format =
              illustration_format ? std::optional{static_cast<ArxImageFormat>(*illustration_format)} : std::nullopt};
}

resource_io::LevelWriteOptions levelWriteOptions(
    std::string_view path, const std::optional<NativeTextMode>& text_mode, const std::optional<float>& units,
    const std::optional<ArxVector3>& offset, const std::optional<bool>& reconstruct_quads,
    const std::optional<bool>& compress, const std::optional<bool>& embed_lighting,
    const std::optional<std::string>& signer, resource_io::ResourceOutputParts outputs, bool recover_case_collisions) {
  const WriteTargetFormat format = writeTargetFormat(path, WriteTargetFormat::kNative);
  const bool native = format == WriteTargetFormat::kNative || format == WriteTargetFormat::kJson;
  requireApplicable(text_mode, native, "text_mode", format);
  requireApplicable(units, format == WriteTargetFormat::kGlb, "arx_units_per_glb_unit", format);
  requireApplicable(offset, format == WriteTargetFormat::kGlb, "arx_offset", format);
  requireApplicable(reconstruct_quads, native, "reconstruct_quads", format);
  requireApplicable(compress, native, "compress", format);
  requireApplicable(embed_lighting, native, "embed_lighting", format);
  requireApplicable(signer, native, "signer", format);
  resource_io::LevelWriteOptions options;
  options.resource = outputOptions(outputs, recover_case_collisions);
  if (units || offset) {
    Level::GlbExportOptions glb;
    glb.arx_units_per_glb_unit = units.value_or(100.0f);
    glb.arx_offset = offset.value_or(ArxVector3{});
    options.glb = glb;
  }
  options.native_text_mode = text_mode;
  options.reconstruct_quads = reconstruct_quads;
  options.compress = compress;
  options.embed_lighting = embed_lighting;
  options.signer = signer;
  return options;
}

resource_io::LoadedModel loadedModel(const ModelImportOutput& imported) {
  if (!imported.model) throw nb::value_error("Model import has no Model");
  resource_io::LoadedModel loaded{.model = *imported.model, .animations = {}};
  loaded.animations.reserve(imported.animations.size());
  for (const std::shared_ptr<PythonAnimation>& animation : imported.animations) {
    if (!animation) throw nb::value_error("Model import has an invalid Animation");
    loaded.animations.push_back(*animation);
  }
  return loaded;
}

struct PythonWritePlanState {
  resource_io::ResourceWritePlan plan;
};

struct PythonWriteCandidate {
  std::shared_ptr<PythonWritePlanState> state;
  std::size_t entry = 0;
  std::size_t candidate = 0;
};

struct PythonWriteEntry {
  std::shared_ptr<PythonWritePlanState> state;
  std::size_t entry = 0;
};

struct PythonWriteReportEntry {
  std::filesystem::path path;
  resource_io::ResourceWriteStatus status = resource_io::ResourceWriteStatus::kPending;
};

struct PythonWriteReport {
  std::vector<PythonWriteReportEntry> entries;
};

struct PythonWritePlan {
  std::shared_ptr<PythonWritePlanState> state;
};

const resource_io::ResourceWriteEntry& writeEntry(const PythonWriteEntry& value) {
  return value.state->plan.entries()[value.entry];
}

resource_io::ResourceWriteEntry& writeEntry(PythonWriteEntry& value) {
  return value.state->plan.entries()[value.entry];
}

const resource_io::ResourceOutput& writeCandidate(const PythonWriteCandidate& value) {
  return value.state->plan.entries()[value.entry].candidates()[value.candidate];
}

PythonWriteReport writeReport(const resource_io::ResourceWriteReport& source) {
  PythonWriteReport report;
  report.entries.reserve(source.entries().size());
  for (const resource_io::ResourceWriteReportEntry& entry : source.entries())
    report.entries.push_back({entry.nativePath(), entry.status()});
  return report;
}

template <class Prepare>
PythonWritePlan prepareWritePlan(const resource_io::Resources& resources, resource_io::ExistingFilePolicy if_exists,
                                 Prepare&& prepare) {
  auto prepared = [&] {
    nb::gil_scoped_release release;
    auto outputs = std::forward<Prepare>(prepare)();
    if (!outputs) return std::move(outputs).template propagate<resource_io::ResourceWritePlan>();
    return resources.prepareWrite(std::move(*outputs), {.existing_file_policy = if_exists});
  }();
  auto plan = unwrap(std::move(prepared));
  return {std::make_shared<PythonWritePlanState>(PythonWritePlanState{std::move(plan)})};
}

PythonWriteReport preflightWritePlan(PythonWritePlan& value) {
  auto result = [&] {
    nb::gil_scoped_release release;
    return value.state->plan.preflight();
  }();
  return writeReport(unwrap(std::move(result)));
}

PythonWriteReport executeWritePlan(PythonWritePlan& value) {
  auto result = [&] {
    nb::gil_scoped_release release;
    return value.state->plan.execute();
  }();
  return writeReport(unwrap(std::move(result)));
}

template <class Prepare>
PythonWriteReport finishWrite(const resource_io::Resources& resources, resource_io::ExistingFilePolicy if_exists,
                              Prepare&& prepare) {
  PythonWritePlan plan = prepareWritePlan(resources, if_exists, std::forward<Prepare>(prepare));
  return executeWritePlan(plan);
}

}  // namespace

void bindResourceIo(nb::module_& module) {
  nb::module_ io = module.def_submodule("resource_io", "Mounted Arx resource discovery and loading.");
  nb::module_ mount_types = io.def_submodule("mounts", "Mount configuration and lookup result types.");
  nb::module_ catalog_types = io.def_submodule("catalog", "Mounted resource catalog snapshots.");
  nb::module_ output_types = io.def_submodule("output", "Prepared resource writes and their outcomes.");
  io.attr("ALL_MOUNTS") = resource_io::kAllResourceMounts;
  mount_types.attr("MAXIMUM_READ_MOUNTS") = resource_io::kMaximumReadMounts;
  const nb::object model_selector_type = module.attr("paths").attr("ModelSelector");
  const nb::object animation_selector_type = module.attr("paths").attr("AnimationSelector");
  const nb::object level_selector_type = module.attr("paths").attr("LevelSelector");
  const nb::object ambiance_selector_type = module.attr("paths").attr("AmbianceSelector");
  const nb::object cinematic_selector_type = module.attr("paths").attr("CinematicSelector");
  const nb::object resource_kind_type = module.attr("paths").attr("ResourceKind");

  nb::enum_<resource_io::ResourceIoOperation>(io, "Operation")
      .value("OPEN_MOUNT", resource_io::ResourceIoOperation::kOpenMount)
      .value("READ", resource_io::ResourceIoOperation::kRead)
      .value("WRITE", resource_io::ResourceIoOperation::kWrite)
      .value("LIST_DIRECTORY", resource_io::ResourceIoOperation::kListDirectory)
      .value("SCAN_CATALOG", resource_io::ResourceIoOperation::kScanCatalog)
      .value("CLASSIFY", resource_io::ResourceIoOperation::kClassify);
  nb::enum_<resource_io::ResourceOutputPart>(
      io, "OutputPart", "Categories of files to include in a resource write.", nb::is_arithmetic(), nb::is_flag())
      .value("NONE", resource_io::kResourceOutputNone)
      .value("PRIMARY", resource_io::kResourceOutputPrimary)
      .value("COMPANIONS", resource_io::kResourceOutputCompanions)
      .value("TEXTURES", resource_io::kResourceOutputTextures)
      .value("AUDIO", resource_io::kResourceOutputAudio)
      .value("IMAGES", resource_io::kResourceOutputImages)
      .value("ALL", resource_io::kResourceOutputAll);
  nb::enum_<resource_io::ExistingFilePolicy>(
      io, "ExistingFilePolicy", "How a write handles a differing destination that already exists.")
      .value("ERROR", resource_io::ExistingFilePolicy::kError)
      .value("OVERWRITE", resource_io::ExistingFilePolicy::kOverwrite)
      .value("PRESERVE", resource_io::ExistingFilePolicy::kPreserve);
  nb::enum_<resource_io::ResourceOutputKind>(output_types, "OutputKind")
      .value("DATA", resource_io::ResourceOutputKind::kData)
      .value("IMAGE", resource_io::ResourceOutputKind::kImage)
      .value("AUDIO", resource_io::ResourceOutputKind::kAudio);
  nb::enum_<resource_io::ResourceWriteStatus>(output_types, "WriteStatus")
      .value("PENDING", resource_io::ResourceWriteStatus::kPending)
      .value("NEEDS_CANDIDATE", resource_io::ResourceWriteStatus::kNeedsCandidate)
      .value("NEEDS_EXISTING_FILE_POLICY", resource_io::ResourceWriteStatus::kNeedsExistingFilePolicy)
      .value("READY", resource_io::ResourceWriteStatus::kReady)
      .value("WRITTEN", resource_io::ResourceWriteStatus::kWritten)
      .value("ALREADY_CURRENT", resource_io::ResourceWriteStatus::kAlreadyCurrent)
      .value("PRESERVED", resource_io::ResourceWriteStatus::kPreserved)
      .value("FAILED", resource_io::ResourceWriteStatus::kFailed);
  nb::enum_<resource_io::ResourceDirectoryEntry::Kind>(mount_types, "DirectoryEntryKind")
      .value("DIRECTORY", resource_io::ResourceDirectoryEntry::Kind::kDirectory)
      .value("UNKNOWN_FILE", resource_io::ResourceDirectoryEntry::Kind::kUnknownFile)
      .value("FTL", resource_io::ResourceDirectoryEntry::Kind::kFtl)
      .value("TEA", resource_io::ResourceDirectoryEntry::Kind::kTea)
      .value("FTS", resource_io::ResourceDirectoryEntry::Kind::kFts)
      .value("DLF", resource_io::ResourceDirectoryEntry::Kind::kDlf)
      .value("LLF", resource_io::ResourceDirectoryEntry::Kind::kLlf)
      .value("AMB", resource_io::ResourceDirectoryEntry::Kind::kAmb)
      .value("CIN", resource_io::ResourceDirectoryEntry::Kind::kCin)
      .value("GLB", resource_io::ResourceDirectoryEntry::Kind::kGlb)
      .value("OBJ", resource_io::ResourceDirectoryEntry::Kind::kObj)
      .value("MTL", resource_io::ResourceDirectoryEntry::Kind::kMtl)
      .value("JSON", resource_io::ResourceDirectoryEntry::Kind::kJson)
      .value("PNG", resource_io::ResourceDirectoryEntry::Kind::kPng)
      .value("JPEG", resource_io::ResourceDirectoryEntry::Kind::kJpeg)
      .value("BMP", resource_io::ResourceDirectoryEntry::Kind::kBmp)
      .value("TGA", resource_io::ResourceDirectoryEntry::Kind::kTga)
      .value("WAV", resource_io::ResourceDirectoryEntry::Kind::kWav)
      .value("MP3", resource_io::ResourceDirectoryEntry::Kind::kMp3)
      .value("OGG", resource_io::ResourceDirectoryEntry::Kind::kOgg);

  nb::class_<resource_io::ResourceMount>(mount_types, "Mount")
      .def_ro("id", &resource_io::ResourceMount::id)
      .def_ro("path", &resource_io::ResourceMount::path);
  nb::class_<resource_io::ResourceRead>(mount_types, "ReadResult")
      .def_prop_ro("data", [](const resource_io::ResourceRead& value) { return toBytes(value.data); })
      .def_ro("native_path", &resource_io::ResourceRead::native_path)
      .def_ro("mount_id", &resource_io::ResourceRead::mount_id);
  nb::class_<resource_io::ResolvedResource>(mount_types, "ResolvedResource")
      .def_ro("native_path", &resource_io::ResolvedResource::native_path)
      .def_ro("mount_id", &resource_io::ResolvedResource::mount_id);
  nb::class_<resource_io::ResourceFile>(mount_types, "ResourceFile")
      .def_ro("logical_path", &resource_io::ResourceFile::logical_path)
      .def_ro("provider_mask", &resource_io::ResourceFile::mount_mask);
  nb::class_<resource_io::ResourceDirectoryEntry>(mount_types, "DirectoryEntry")
      .def_ro("name", &resource_io::ResourceDirectoryEntry::name)
      .def_ro("kind", &resource_io::ResourceDirectoryEntry::kind)
      .def_ro("provider_mask", &resource_io::ResourceDirectoryEntry::mount_mask);
  nb::class_<PythonWriteCandidate>(output_types, "WriteCandidate", "One encoded output competing for a destination.")
      .def_prop_ro("kind", [](const PythonWriteCandidate& value) { return writeCandidate(value).kind; })
      .def_prop_ro("primary", [](const PythonWriteCandidate& value) { return writeCandidate(value).primary; })
      .def_prop_ro("owner_kind",
                   [resource_kind_type](const PythonWriteCandidate& value) -> nb::object {
                     switch (writeCandidate(value).owner_kind) {
                       case ARX_RESOURCE_KIND_LEVEL:
                         return resource_kind_type.attr("LEVEL");
                       case ARX_RESOURCE_KIND_MODEL:
                         return resource_kind_type.attr("MODEL");
                       case ARX_RESOURCE_KIND_ANIMATION:
                         return resource_kind_type.attr("ANIMATION");
                       case ARX_RESOURCE_KIND_CINEMATIC:
                         return resource_kind_type.attr("CINEMATIC");
                       case ARX_RESOURCE_KIND_AMBIANCE:
                         return resource_kind_type.attr("AMBIANCE");
                       default:
                         return nb::none();
                     }
                     return nb::none();
                   })
      .def_prop_ro("owner_identity",
                   [](const PythonWriteCandidate& value) { return writeCandidate(value).owner_identity; })
      .def_prop_ro("resource_path",
                   [](const PythonWriteCandidate& value) -> nb::object {
                     const std::string& path = writeCandidate(value).resource_path;
                     return path.empty() ? nb::none() : nb::cast(path);
                   })
      .def_prop_ro("native_path",
                   [](const PythonWriteCandidate& value) -> nb::object {
                     const std::filesystem::path& path = writeCandidate(value).native_path;
                     return path.empty() ? nb::none() : nb::cast(path);
                   })
      .def_prop_ro("data", [](const PythonWriteCandidate& value) { return toBytes(writeCandidate(value).data); })
      .def_prop_ro("size", [](const PythonWriteCandidate& value) { return writeCandidate(value).data.size(); })
      .def_prop_ro("written", [](const PythonWriteCandidate& value) { return writeCandidate(value).written; });
  nb::class_<PythonWriteEntry>(output_types, "WriteEntry", "A resolved output destination in a write plan.")
      .def_prop_ro("path", [](const PythonWriteEntry& value) { return writeEntry(value).nativePath(); })
      .def_prop_ro("candidates",
                   [](const PythonWriteEntry& value) {
                     nb::list result;
                     const auto candidates = writeEntry(value).candidates();
                     for (std::size_t index = 0; index < candidates.size(); ++index)
                       result.append(PythonWriteCandidate{value.state, value.entry, index});
                     return nb::tuple(result);
                   })
      .def_prop_rw(
          "selected_candidate",
          [](const PythonWriteEntry& value) { return writeEntry(value).selectedCandidate(); },
          [](PythonWriteEntry& value, std::size_t selected) {
            if (!writeEntry(value).selectCandidate(selected)) throw nb::index_error("candidate index out of range");
          })
      .def_prop_rw(
          "if_exists",
          [](const PythonWriteEntry& value) { return writeEntry(value).existingFilePolicy(); },
          [](PythonWriteEntry& value, const std::optional<resource_io::ExistingFilePolicy>& policy) {
            if (policy)
              writeEntry(value).setExistingFilePolicy(*policy);
            else
              writeEntry(value).clearExistingFilePolicy();
          })
      .def_prop_ro("status", [](const PythonWriteEntry& value) { return writeEntry(value).status(); });
  nb::class_<PythonWriteReportEntry>(output_types, "WriteReportEntry", "The final state of one output destination.")
      .def_ro("path", &PythonWriteReportEntry::path)
      .def_ro("status", &PythonWriteReportEntry::status);
  auto write_report =
      nb::class_<PythonWriteReport>(output_types, "WriteReport", "An immutable snapshot of write outcomes.")
          .def("__len__", [](const PythonWriteReport& value) { return value.entries.size(); })
          .def("__getitem__",
               [](const PythonWriteReport& value, std::int64_t index) {
                 return value.entries[sequenceIndex(index, value.entries.size())];
               })
          .def(
              "__getitem__",
              [](const PythonWriteReport& value, const nb::slice& slice) {
                auto [start, stop, step, length] = slice.compute(value.entries.size());
                (void)stop;
                nb::list result;
                for (std::size_t index = 0; index < length; ++index) {
                  result.append(value.entries[start]);
                  start += step;
                }
                return result;
              },
              nb::arg("slice"),
              nb::sig("def __getitem__(self, slice: slice) -> list[WriteReportEntry]"));
  registerSequence(write_report);
  auto write_plan =
      nb::class_<PythonWritePlan>(output_types, "WritePlan", "A mutable set of pending output decisions.")
          .def("__len__", [](const PythonWritePlan& value) { return value.state->plan.entries().size(); })
          .def("__getitem__",
               [](const PythonWritePlan& value, std::int64_t index) {
                 return PythonWriteEntry{value.state, sequenceIndex(index, value.state->plan.entries().size())};
               })
          .def(
              "__getitem__",
              [](const PythonWritePlan& value, const nb::slice& slice) {
                auto [start, stop, step, length] = slice.compute(value.state->plan.entries().size());
                (void)stop;
                nb::list result;
                for (std::size_t index = 0; index < length; ++index) {
                  result.append(PythonWriteEntry{value.state, static_cast<std::size_t>(start)});
                  start += step;
                }
                return result;
              },
              nb::arg("slice"),
              nb::sig("def __getitem__(self, slice: slice) -> list[WriteEntry]"))
          .def_prop_rw(
              "default_if_exists",
              [](const PythonWritePlan& value) { return value.state->plan.defaultExistingFilePolicy(); },
              [](PythonWritePlan& value, resource_io::ExistingFilePolicy policy) {
                value.state->plan.setDefaultExistingFilePolicy(policy);
              })
          .def("preflight", &preflightWritePlan)
          .def("execute", &executeWritePlan);
  registerSequence(write_plan);
  nb::class_<resource_io::ResourceCatalogEntry>(
      catalog_types, "Entry", "One discovered selector and the mounts that provide it.")
      .def_prop_ro("selector",
                   [model_selector_type,
                    animation_selector_type,
                    level_selector_type,
                    ambiance_selector_type,
                    cinematic_selector_type](const resource_io::ResourceCatalogEntry& value) -> nb::object {
                     const paths::ResourceSelector& selector = value.resource;
                     switch (selector.kind) {
                       case ARX_RESOURCE_KIND_LEVEL:
                         return level_selector_type(selector.level);
                       case ARX_RESOURCE_KIND_MODEL:
                         return model_selector_type(selector.model_type, selector.name, selector.tweak);
                       case ARX_RESOURCE_KIND_ANIMATION:
                         return animation_selector_type(selector.animation_type, selector.name);
                       case ARX_RESOURCE_KIND_CINEMATIC:
                         return cinematic_selector_type(selector.name);
                       case ARX_RESOURCE_KIND_AMBIANCE:
                         return ambiance_selector_type(selector.name);
                       default:
                         break;
                     }
                     throw nb::value_error("catalog entry has an invalid resource selector");
                   })
      .def_ro("provider_mask", &resource_io::ResourceCatalogEntry::mount_mask);

  auto catalog =
      nb::class_<resource_io::ResourceCatalog>(
          catalog_types, "Catalog", "An immutable snapshot of selectors visible through selected mounts.")
          .def("__len__", [](const resource_io::ResourceCatalog& value) { return value.entries().size(); })
          .def("__getitem__",
               [](const resource_io::ResourceCatalog& value, std::int64_t index) {
                 return value.entries()[catalogIndex(index, value.entries().size())];
               })
          .def(
              "__getitem__",
              [](const resource_io::ResourceCatalog& value, const nb::slice& slice) {
                auto [start, stop, step, length] = slice.compute(value.entries().size());
                (void)stop;
                nb::list result;
                for (std::size_t index = 0; index < length; ++index) {
                  result.append(value.entries()[start]);
                  start += step;
                }
                return result;
              },
              nb::arg("slice"),
              nb::sig("def __getitem__(self, slice: slice) -> list[Entry]"))
          .def("__repr__", [](const resource_io::ResourceCatalog& value) {
            return "<pistoris.resource_io.catalog.Catalog len=" + std::to_string(value.entries().size()) + '>';
          });
  registerSequence(catalog);

  io.def(
      "libertatis_resource_root",
      [] {
        auto result = [&] {
          nb::gil_scoped_release release;
          return resource_io::libertatisResourceRoot();
        }();
        return unwrap(std::move(result));
      },
      "Return the platform Libertatis resource root without configuring mounts.");

  nb::class_<resource_io::ResourceMounts>(
      io, "ResourceMounts", "An ordered set of live mounted-resource roots and an independent write root.")
      .def(nb::new_([](const std::vector<std::filesystem::path>& read_mounts,
                       const std::optional<std::filesystem::path>& write_mount) {
             return new resource_io::ResourceMounts(openMounts({read_mounts, write_mount}));
           }),
           nb::arg("read_mounts") = nb::tuple(),
           nb::kw_only(),
           nb::arg("write_mount") = nb::none())
      .def(
          "add_read_mount",
          [](resource_io::ResourceMounts& self,
             const std::filesystem::path& path) -> std::optional<resource_io::ResourceMount> {
            resource_io::MountValidationReport report;
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.addReadMount(path, &report);
            }();
            unwrap(std::move(result));
            warnMountValidation(report);
            return mountForPath(self, path);
          },
          nb::arg("path"),
          "Append an existing read root and return its mount, or None when the path is missing.")
      .def("add_libertatis_mounts",
           [](resource_io::ResourceMounts& self) {
             resource_io::MountValidationReport report;
             auto result = [&] {
               nb::gil_scoped_release release;
               return self.addLibertatisMounts(&report);
             }();
             unwrap(std::move(result));
             warnMountValidation(report);
           })
      .def_prop_ro("read_mounts",
                   [](const resource_io::ResourceMounts& self) {
                     nb::list result;
                     for (const resource_io::ResourceMount& mount : self.readMounts()) result.append(mount);
                     return nb::tuple(result);
                   })
      .def_prop_rw(
          "write_mount",
          [](const resource_io::ResourceMounts& self) { return self.writeMount(); },
          [](resource_io::ResourceMounts& self, const std::optional<std::filesystem::path>& path) {
            resource_io::MountValidationReport report;
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.setWriteMount(path, &report);
            }();
            unwrap(std::move(result));
          })
      .def("set_libertatis_write_mount",
           [](resource_io::ResourceMounts& self) {
             resource_io::MountValidationReport report;
             auto result = [&] {
               nb::gil_scoped_release release;
               return self.setLibertatisWriteMount(&report);
             }();
             unwrap(std::move(result));
           })
      .def_prop_ro("available_mount_mask",
                   [](const resource_io::ResourceMounts& self) { return self.availableMounts(); })
      .def(
          "highest_priority_mount",
          [](const resource_io::ResourceMounts& self, resource_io::ResourceMountMask mount_mask) -> nb::object {
            const auto selected = self.highestPriorityMountId(mount_mask);
            if (!selected) return nb::none();
            for (const resource_io::ResourceMount& mount : self.readMounts()) {
              if (mount.id == *selected) return nb::cast(mount);
            }
            return nb::none();
          },
          nb::arg("mount_mask"))
      .def(
          "read",
          [](const resource_io::ResourceMounts& self,
             nb::handle path,
             resource_io::ResourceMountMask mount_mask,
             bool recover_case_collisions) {
            const std::string logical_path = requiredLogicalPath(path, "path");
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.read(logical_path, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
            }();
            return unwrap(std::move(result));
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("mount_mask") = resource_io::kAllResourceMounts,
          nb::arg("recover_case_collisions") = false)
      .def(
          "resolve",
          [](const resource_io::ResourceMounts& self,
             nb::handle path,
             resource_io::ResourceMountMask mount_mask,
             bool recover_case_collisions) {
            const std::string logical_path = requiredLogicalPath(path, "path");
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.resolve(logical_path, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
            }();
            return unwrap(std::move(result));
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("mount_mask") = resource_io::kAllResourceMounts,
          nb::arg("recover_case_collisions") = false)
      .def(
          "resolve_write_path",
          [](const resource_io::ResourceMounts& self, nb::handle path, bool recover_case_collisions) {
            const std::string logical_path = requiredLogicalPath(path, "path");
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.resolveWritePath(logical_path, ioFlags(recover_case_collisions));
            }();
            return unwrap(std::move(result));
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("recover_case_collisions") = false)
      .def(
          "write",
          [](const resource_io::ResourceMounts& self, nb::handle path, nb::handle data, bool recover_case_collisions) {
            const std::string logical_path = requiredLogicalPath(path, "path");
            const auto bytes = byteSpan(data);
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.write(logical_path, bytes, ioFlags(recover_case_collisions));
            }();
            unwrap(std::move(result));
          },
          nb::arg("path"),
          nb::arg("data"),
          nb::kw_only(),
          nb::arg("recover_case_collisions") = false)
      .def(
          "list_files",
          [](const resource_io::ResourceMounts& self,
             nb::handle directory,
             std::uint32_t max_depth,
             resource_io::ResourceMountMask mount_mask,
             bool recover_case_collisions) {
            const std::string logical_directory = requiredLogicalPath(directory, "directory");
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.enumerate(
                  logical_directory, max_depth, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
            }();
            return unwrap(std::move(result));
          },
          nb::arg("directory") = "",
          nb::kw_only(),
          nb::arg("max_depth"),
          nb::arg("mount_mask") = resource_io::kAllResourceMounts,
          nb::arg("recover_case_collisions") = false)
      .def(
          "list_directory",
          [](const resource_io::ResourceMounts& self,
             nb::handle directory,
             resource_io::ResourceMountMask mount_mask,
             bool recover_case_collisions) {
            const std::string logical_directory = requiredLogicalPath(directory, "directory");
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.listDirectory(logical_directory,
                                        {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
            }();
            return unwrap(std::move(result));
          },
          nb::arg("directory") = "",
          nb::kw_only(),
          nb::arg("mount_mask") = resource_io::kAllResourceMounts,
          nb::arg("recover_case_collisions") = false);

  nb::class_<resource_io::Resources>(io, "Resources")
      .def(nb::new_([](const std::vector<std::filesystem::path>& read_mounts,
                       const std::optional<std::filesystem::path>& write_mount) {
             return new resource_io::Resources(openMounts({read_mounts, write_mount}));
           }),
           nb::arg("read_mounts") = nb::tuple(),
           nb::kw_only(),
           nb::arg("write_mount") = nb::none())
      .def_prop_ro(
          "mounts",
          [](resource_io::Resources& self) -> resource_io::ResourceMounts& { return self.mounts(); },
          nb::rv_policy::reference_internal)
      .def(
          "scan_catalog",
          [](const resource_io::Resources& self,
             resource_io::ResourceMountMask mount_mask,
             bool recover_case_collisions) {
            auto result = [&] {
              nb::gil_scoped_release release;
              return self.scanCatalog({.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
            }();
            return unwrap(std::move(result));
          },
          nb::kw_only(),
          nb::arg("mount_mask") = resource_io::kAllResourceMounts,
          nb::arg("recover_case_collisions") = false)
      .def(
          "load_model",
          [model_selector_type](const resource_io::Resources& self,
                                nb::handle source,
                                NativeTextMode text_mode,
                                float units,
                                resource_io::ResourceMountMask mount_mask,
                                bool recover_case_collisions) {
            const resource_io::ModelLoadOptions options{.glb = {.arx_units_per_glb_unit = units},
                                                        .native_text_mode = text_mode};
            if (nb::isinstance<nb::str>(source)) {
              const std::string value = nb::cast<std::string>(source);
              if (paths::resourceSelectorKind(value) != ARX_RESOURCE_KIND_NONE) {
                const auto resource = parsedSelector(value);
                return loadModelImport([&] {
                  return self.loadModel(
                      resource, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
                });
              }
              return loadModelImport([&] {
                return self.loadModel(
                    value, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            if (nb::isinstance(source, model_selector_type)) {
              std::string name;
              std::string tweak;
              const auto resource = modelPathView(source, name, tweak);
              return loadModelImport([&] {
                return self.loadModel(
                    resource, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            if (const auto value = logicalPath(source)) {
              return loadModelImport([&] {
                return self.loadModel(
                    *value, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            throw nb::type_error("source must be a Model selector or mounted logical path");
          },
          nb::arg("source"),
          nb::kw_only(),
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("arx_units_per_glb_unit") = 10.0f,
          nb::arg("mount_mask") = resource_io::kAllResourceMounts,
          nb::arg("recover_case_collisions") = false)
      .def(
          "load_model_file",
          [](const resource_io::Resources& self, nb::handle source, NativeTextMode text_mode, float units) {
            const auto path = nativePath(source);
            if (!path) throw nb::type_error("source must be str or PathLike");
            const resource_io::ModelLoadOptions options{.glb = {.arx_units_per_glb_unit = units},
                                                        .native_text_mode = text_mode};
            return loadModelImport([&] { return self.loadModelFile(*path, options); });
          },
          nb::arg("source"),
          nb::kw_only(),
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("arx_units_per_glb_unit") = 10.0f)
      .def(
          "load_animation",
          [animation_selector_type](const resource_io::Resources& self,
                                    nb::handle source,
                                    NativeTextMode text_mode,
                                    resource_io::ResourceMountMask mount_mask,
                                    bool recover_case_collisions) {
            const resource_io::AnimationLoadOptions options{.native_text_mode = text_mode};
            if (nb::isinstance<nb::str>(source)) {
              const std::string value = nb::cast<std::string>(source);
              if (paths::resourceSelectorKind(value) != ARX_RESOURCE_KIND_NONE) {
                const auto resource = parsedSelector(value);
                return loadTracked<Animation, AnimationTracking>([&] {
                  return self.loadAnimation(
                      resource, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
                });
              }
              return loadTracked<Animation, AnimationTracking>([&] {
                return self.loadAnimation(
                    value, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            if (nb::isinstance(source, animation_selector_type)) {
              std::string name;
              const auto resource = animationPathView(source, name);
              return loadTracked<Animation, AnimationTracking>([&] {
                return self.loadAnimation(
                    resource, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            if (const auto value = logicalPath(source)) {
              return loadTracked<Animation, AnimationTracking>([&] {
                return self.loadAnimation(
                    *value, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            throw nb::type_error("source must be an Animation selector or mounted logical path");
          },
          nb::arg("source"),
          nb::kw_only(),
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("mount_mask") = resource_io::kAllResourceMounts,
          nb::arg("recover_case_collisions") = false)
      .def(
          "load_animation_file",
          [](const resource_io::Resources& self, nb::handle source, NativeTextMode text_mode) {
            const auto path = nativePath(source);
            if (!path) throw nb::type_error("source must be str or PathLike");
            const resource_io::AnimationLoadOptions options{.native_text_mode = text_mode};
            return loadTracked<Animation, AnimationTracking>([&] { return self.loadAnimationFile(*path, options); });
          },
          nb::arg("source"),
          nb::kw_only(),
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def(
          "load_level",
          [level_selector_type](const resource_io::Resources& self,
                                nb::handle source,
                                NativeTextMode text_mode,
                                float units,
                                const std::optional<ArxVector3>& offset,
                                resource_io::ResourceMountMask mount_mask,
                                bool recover_case_collisions) {
            resource_io::LevelLoadOptions options;
            options.glb.arx_units_per_glb_unit = units;
            options.glb.arx_offset = offset;
            options.native_text_mode = text_mode;
            if (nb::isinstance<nb::int_>(source)) {
              const auto level = nb::cast<std::uint32_t>(source);
              return loadTracked<Level, LevelTracking>([&] {
                return self.loadLevel(
                    level, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            if (nb::isinstance<nb::str>(source)) {
              const std::string value = nb::cast<std::string>(source);
              if (paths::resourceSelectorKind(value) != ARX_RESOURCE_KIND_NONE) {
                const paths::ResourceSelector selector = parsedSelector(value);
                if (selector.kind != ARX_RESOURCE_KIND_LEVEL) throw nb::value_error("selector is not a Level");
                return loadTracked<Level, LevelTracking>([&] {
                  return self.loadLevel(
                      selector.level, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
                });
              }
              return loadTracked<Level, LevelTracking>([&] {
                return self.loadLevel(
                    value, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            if (nb::isinstance(source, level_selector_type)) {
              const auto level = nb::cast<std::uint32_t>(source.attr("level"));
              return loadTracked<Level, LevelTracking>([&] {
                return self.loadLevel(
                    level, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            if (const auto value = logicalPath(source)) {
              return loadTracked<Level, LevelTracking>([&] {
                return self.loadLevel(
                    *value, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            throw nb::type_error("source must be a Level selector, level number, or mounted logical path");
          },
          nb::arg("source"),
          nb::kw_only(),
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("arx_units_per_glb_unit") = 100.0f,
          nb::arg("arx_offset") = nb::none(),
          nb::arg("mount_mask") = resource_io::kAllResourceMounts,
          nb::arg("recover_case_collisions") = false)
      .def(
          "load_level_file",
          [](const resource_io::Resources& self,
             nb::handle source,
             nb::handle llf,
             nb::handle dlf,
             NativeTextMode text_mode,
             float units,
             const std::optional<ArxVector3>& offset) {
            const auto path = nativePath(source);
            if (!path) throw nb::type_error("source must be str or PathLike");
            resource_io::LevelLoadOptions options;
            options.glb.arx_units_per_glb_unit = units;
            options.glb.arx_offset = offset;
            options.native_text_mode = text_mode;
            const auto llf_path = optionalNativePath(llf, "llf");
            const auto dlf_path = optionalNativePath(dlf, "dlf");
            return loadTracked<Level, LevelTracking>(
                [&] { return self.loadLevelFile(*path, llf_path, dlf_path, options); });
          },
          nb::arg("source"),
          nb::kw_only(),
          nb::arg("llf") = nb::none(),
          nb::arg("dlf") = nb::none(),
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("arx_units_per_glb_unit") = 100.0f,
          nb::arg("arx_offset") = nb::none())
      .def(
          "load_ambiance",
          [ambiance_selector_type](const resource_io::Resources& self,
                                   nb::handle source,
                                   NativeTextMode text_mode,
                                   float units,
                                   resource_io::ResourceMountMask mount_mask,
                                   bool recover_case_collisions) {
            const resource_io::AmbianceLoadOptions options{.glb = {.arx_units_per_glb_unit = units},
                                                           .native_text_mode = text_mode};
            if (nb::isinstance<nb::str>(source)) {
              const std::string value = nb::cast<std::string>(source);
              if (paths::resourceSelectorKind(value) != ARX_RESOURCE_KIND_NONE) {
                const auto resource = parsedSelector(value);
                return loadTracked<Ambiance, AmbianceTracking>([&] {
                  return self.loadAmbiance(
                      resource, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
                });
              }
              return loadTracked<Ambiance, AmbianceTracking>([&] {
                return self.loadAmbiance(
                    value, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            if (nb::isinstance(source, ambiance_selector_type)) {
              const std::string name = nb::cast<std::string>(source.attr("name"));
              return loadTracked<Ambiance, AmbianceTracking>([&] {
                return self.loadAmbiance(paths::AmbiancePathView{name},
                                         options,
                                         {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            if (const auto value = logicalPath(source)) {
              return loadTracked<Ambiance, AmbianceTracking>([&] {
                return self.loadAmbiance(
                    *value, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            throw nb::type_error("source must be an Ambiance selector or mounted logical path");
          },
          nb::arg("source"),
          nb::kw_only(),
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("arx_units_per_glb_unit") = 10.0f,
          nb::arg("mount_mask") = resource_io::kAllResourceMounts,
          nb::arg("recover_case_collisions") = false)
      .def(
          "load_ambiance_file",
          [](const resource_io::Resources& self, nb::handle source, NativeTextMode text_mode, float units) {
            const auto path = nativePath(source);
            if (!path) throw nb::type_error("source must be str or PathLike");
            const resource_io::AmbianceLoadOptions options{.glb = {.arx_units_per_glb_unit = units},
                                                           .native_text_mode = text_mode};
            return loadTracked<Ambiance, AmbianceTracking>([&] { return self.loadAmbianceFile(*path, options); });
          },
          nb::arg("source"),
          nb::kw_only(),
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("arx_units_per_glb_unit") = 10.0f)
      .def(
          "load_cinematic",
          [cinematic_selector_type](const resource_io::Resources& self,
                                    nb::handle source,
                                    NativeTextMode text_mode,
                                    resource_io::ResourceMountMask mount_mask,
                                    bool recover_case_collisions) {
            const resource_io::CinematicLoadOptions options{.native_text_mode = text_mode};
            if (nb::isinstance<nb::str>(source)) {
              const std::string value = nb::cast<std::string>(source);
              if (paths::resourceSelectorKind(value) != ARX_RESOURCE_KIND_NONE) {
                const auto resource = parsedSelector(value);
                return loadTracked<Cinematic, CinematicTracking>([&] {
                  return self.loadCinematic(
                      resource, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
                });
              }
              return loadTracked<Cinematic, CinematicTracking>([&] {
                return self.loadCinematic(
                    value, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            if (nb::isinstance(source, cinematic_selector_type)) {
              const std::string name = nb::cast<std::string>(source.attr("name"));
              return loadTracked<Cinematic, CinematicTracking>([&] {
                return self.loadCinematic(paths::CinematicPathView{name},
                                          options,
                                          {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            if (const auto value = logicalPath(source)) {
              return loadTracked<Cinematic, CinematicTracking>([&] {
                return self.loadCinematic(
                    *value, options, {.mount_mask = mount_mask, .flags = ioFlags(recover_case_collisions)});
              });
            }
            throw nb::type_error("source must be a Cinematic selector or mounted logical path");
          },
          nb::arg("source"),
          nb::kw_only(),
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("mount_mask") = resource_io::kAllResourceMounts,
          nb::arg("recover_case_collisions") = false)
      .def(
          "load_cinematic_file",
          [](const resource_io::Resources& self, nb::handle source, NativeTextMode text_mode) {
            const auto path = nativePath(source);
            if (!path) throw nb::type_error("source must be str or PathLike");
            const resource_io::CinematicLoadOptions options{.native_text_mode = text_mode};
            return loadTracked<Cinematic, CinematicTracking>([&] { return self.loadCinematicFile(*path, options); });
          },
          nb::arg("source"),
          nb::kw_only(),
          nb::arg("text_mode") = NativeTextMode::kAuto)
      .def(
          "prepare_model_write",
          [model_selector_type](const resource_io::Resources& self,
                                const PythonModel& model,
                                nb::handle target,
                                const std::optional<NativeTextMode>& text_mode,
                                const std::optional<float>& units,
                                const std::optional<bool>& compress,
                                resource_io::ResourceOutputParts outputs,
                                resource_io::ExistingFilePolicy if_exists,
                                bool recover_case_collisions) {
            const std::string logical_target = mountedTarget(target, model_selector_type, "target");
            const auto options = modelWriteOptions(logical_target,
                                                   WriteTargetFormat::kNative,
                                                   text_mode,
                                                   units,
                                                   compress,
                                                   outputs,
                                                   recover_case_collisions);
            return prepareWritePlan(
                self, if_exists, [&] { return self.prepareModelOutputs(model, logical_target, options); });
          },
          nb::arg("model"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("compress") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError,
          nb::arg("recover_case_collisions") = false)
      .def(
          "prepare_model_write",
          [model_selector_type](const resource_io::Resources& self,
                                const ModelImportOutput& imported,
                                nb::handle target,
                                const std::optional<NativeTextMode>& text_mode,
                                const std::optional<float>& units,
                                const std::optional<bool>& compress,
                                resource_io::ResourceOutputParts outputs,
                                resource_io::ExistingFilePolicy if_exists,
                                bool recover_case_collisions) {
            const std::string logical_target = mountedTarget(target, model_selector_type, "target");
            resource_io::LoadedModel loaded = loadedModel(imported);
            const auto options =
                modelWriteOptions(logical_target,
                                  loaded.animations.empty() ? WriteTargetFormat::kNative : WriteTargetFormat::kGlb,
                                  text_mode,
                                  units,
                                  compress,
                                  outputs,
                                  recover_case_collisions);
            return prepareWritePlan(
                self, if_exists, [&] { return self.prepareModelOutputs(loaded, logical_target, options); });
          },
          nb::arg("model"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("compress") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError,
          nb::arg("recover_case_collisions") = false)
      .def(
          "prepare_model_file_write",
          [](const resource_io::Resources& self,
             const PythonModel& model,
             nb::handle target,
             const std::optional<NativeTextMode>& text_mode,
             const std::optional<float>& units,
             const std::optional<bool>& compress,
             resource_io::ResourceOutputParts outputs,
             resource_io::ExistingFilePolicy if_exists) {
            const auto path = nativePath(target);
            if (!path) throw nb::type_error("target must be str or PathLike");
            const auto options = modelWriteOptions(
                pathToUtf8(*path), WriteTargetFormat::kNative, text_mode, units, compress, outputs, false);
            return prepareWritePlan(
                self, if_exists, [&] { return self.prepareModelFileOutputs(model, *path, options); });
          },
          nb::arg("model"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("compress") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError)
      .def(
          "prepare_model_file_write",
          [](const resource_io::Resources& self,
             const ModelImportOutput& imported,
             nb::handle target,
             const std::optional<NativeTextMode>& text_mode,
             const std::optional<float>& units,
             const std::optional<bool>& compress,
             resource_io::ResourceOutputParts outputs,
             resource_io::ExistingFilePolicy if_exists) {
            const auto path = nativePath(target);
            if (!path) throw nb::type_error("target must be str or PathLike");
            resource_io::LoadedModel loaded = loadedModel(imported);
            const auto options =
                modelWriteOptions(pathToUtf8(*path),
                                  loaded.animations.empty() ? WriteTargetFormat::kNative : WriteTargetFormat::kGlb,
                                  text_mode,
                                  units,
                                  compress,
                                  outputs,
                                  false);
            return prepareWritePlan(
                self, if_exists, [&] { return self.prepareModelFileOutputs(loaded, *path, options); });
          },
          nb::arg("model"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("compress") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError)
      .def(
          "write_model",
          [model_selector_type](const resource_io::Resources& self,
                                const PythonModel& model,
                                nb::handle target,
                                const std::optional<NativeTextMode>& text_mode,
                                const std::optional<float>& units,
                                const std::optional<bool>& compress,
                                resource_io::ResourceOutputParts outputs,
                                resource_io::ExistingFilePolicy if_exists,
                                bool recover_case_collisions) {
            const std::string logical_target = mountedTarget(target, model_selector_type, "target");
            const auto options = modelWriteOptions(logical_target,
                                                   WriteTargetFormat::kNative,
                                                   text_mode,
                                                   units,
                                                   compress,
                                                   outputs,
                                                   recover_case_collisions);
            return finishWrite(
                self, if_exists, [&] { return self.prepareModelOutputs(model, logical_target, options); });
          },
          nb::arg("model"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("compress") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError,
          nb::arg("recover_case_collisions") = false)
      .def(
          "write_model",
          [model_selector_type](const resource_io::Resources& self,
                                const ModelImportOutput& imported,
                                nb::handle target,
                                const std::optional<NativeTextMode>& text_mode,
                                const std::optional<float>& units,
                                const std::optional<bool>& compress,
                                resource_io::ResourceOutputParts outputs,
                                resource_io::ExistingFilePolicy if_exists,
                                bool recover_case_collisions) {
            const std::string logical_target = mountedTarget(target, model_selector_type, "target");
            resource_io::LoadedModel loaded = loadedModel(imported);
            const auto options =
                modelWriteOptions(logical_target,
                                  loaded.animations.empty() ? WriteTargetFormat::kNative : WriteTargetFormat::kGlb,
                                  text_mode,
                                  units,
                                  compress,
                                  outputs,
                                  recover_case_collisions);
            return finishWrite(
                self, if_exists, [&] { return self.prepareModelOutputs(loaded, logical_target, options); });
          },
          nb::arg("model"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("compress") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError,
          nb::arg("recover_case_collisions") = false)
      .def(
          "write_model_file",
          [](const resource_io::Resources& self,
             const PythonModel& model,
             nb::handle target,
             const std::optional<NativeTextMode>& text_mode,
             const std::optional<float>& units,
             const std::optional<bool>& compress,
             resource_io::ResourceOutputParts outputs,
             resource_io::ExistingFilePolicy if_exists) {
            const auto path = nativePath(target);
            if (!path) throw nb::type_error("target must be str or PathLike");
            const auto options = modelWriteOptions(
                pathToUtf8(*path), WriteTargetFormat::kNative, text_mode, units, compress, outputs, false);
            return finishWrite(self, if_exists, [&] { return self.prepareModelFileOutputs(model, *path, options); });
          },
          nb::arg("model"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("compress") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError)
      .def(
          "write_model_file",
          [](const resource_io::Resources& self,
             const ModelImportOutput& imported,
             nb::handle target,
             const std::optional<NativeTextMode>& text_mode,
             const std::optional<float>& units,
             const std::optional<bool>& compress,
             resource_io::ResourceOutputParts outputs,
             resource_io::ExistingFilePolicy if_exists) {
            const auto path = nativePath(target);
            if (!path) throw nb::type_error("target must be str or PathLike");
            resource_io::LoadedModel loaded = loadedModel(imported);
            const auto options =
                modelWriteOptions(pathToUtf8(*path),
                                  loaded.animations.empty() ? WriteTargetFormat::kNative : WriteTargetFormat::kGlb,
                                  text_mode,
                                  units,
                                  compress,
                                  outputs,
                                  false);
            return finishWrite(self, if_exists, [&] { return self.prepareModelFileOutputs(loaded, *path, options); });
          },
          nb::arg("model"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("compress") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError)
      .def(
          "prepare_animation_write",
          [animation_selector_type](const resource_io::Resources& self,
                                    const PythonAnimation& animation,
                                    nb::handle target,
                                    NativeTextMode text_mode,
                                    resource_io::ResourceOutputParts outputs,
                                    resource_io::ExistingFilePolicy if_exists,
                                    bool recover_case_collisions) {
            const std::string logical_target = mountedTarget(target, animation_selector_type, "target");
            const resource_io::AnimationWriteOptions options{
                .resource = outputOptions(outputs, recover_case_collisions), .native_text_mode = text_mode};
            return prepareWritePlan(
                self, if_exists, [&] { return self.prepareAnimationOutputs(animation, logical_target, options); });
          },
          nb::arg("animation"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError,
          nb::arg("recover_case_collisions") = false)
      .def(
          "prepare_animation_file_write",
          [](const resource_io::Resources& self,
             const PythonAnimation& animation,
             nb::handle target,
             NativeTextMode text_mode,
             resource_io::ResourceOutputParts outputs,
             resource_io::ExistingFilePolicy if_exists) {
            const auto path = nativePath(target);
            if (!path) throw nb::type_error("target must be str or PathLike");
            const resource_io::AnimationWriteOptions options{.resource = outputOptions(outputs, false),
                                                             .native_text_mode = text_mode};
            return prepareWritePlan(
                self, if_exists, [&] { return self.prepareAnimationFileOutputs(animation, *path, options); });
          },
          nb::arg("animation"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError)
      .def(
          "write_animation",
          [animation_selector_type](const resource_io::Resources& self,
                                    const PythonAnimation& animation,
                                    nb::handle target,
                                    NativeTextMode text_mode,
                                    resource_io::ResourceOutputParts outputs,
                                    resource_io::ExistingFilePolicy if_exists,
                                    bool recover_case_collisions) {
            const std::string logical_target = mountedTarget(target, animation_selector_type, "target");
            const resource_io::AnimationWriteOptions options{
                .resource = outputOptions(outputs, recover_case_collisions), .native_text_mode = text_mode};
            return finishWrite(
                self, if_exists, [&] { return self.prepareAnimationOutputs(animation, logical_target, options); });
          },
          nb::arg("animation"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError,
          nb::arg("recover_case_collisions") = false)
      .def(
          "write_animation_file",
          [](const resource_io::Resources& self,
             const PythonAnimation& animation,
             nb::handle target,
             NativeTextMode text_mode,
             resource_io::ResourceOutputParts outputs,
             resource_io::ExistingFilePolicy if_exists) {
            const auto path = nativePath(target);
            if (!path) throw nb::type_error("target must be str or PathLike");
            const resource_io::AnimationWriteOptions options{.resource = outputOptions(outputs, false),
                                                             .native_text_mode = text_mode};
            return finishWrite(
                self, if_exists, [&] { return self.prepareAnimationFileOutputs(animation, *path, options); });
          },
          nb::arg("animation"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = NativeTextMode::kAuto,
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError)
      .def(
          "prepare_level_write",
          [level_selector_type](const resource_io::Resources& self,
                                const PythonLevel& level,
                                nb::handle target,
                                nb::handle llf,
                                nb::handle dlf,
                                std::optional<std::uint32_t>
                                    level_index,
                                const std::optional<NativeTextMode>& text_mode,
                                const std::optional<float>& units,
                                const std::optional<ArxVector3>& offset,
                                const std::optional<bool>& reconstruct_quads,
                                const std::optional<bool>& compress,
                                const std::optional<bool>& embed_lighting,
                                const std::optional<std::string>& signer,
                                resource_io::ResourceOutputParts outputs,
                                resource_io::ExistingFilePolicy if_exists,
                                bool recover_case_collisions) {
            const std::string logical_target = mountedTarget(target, level_selector_type, "target");
            const auto llf_path = optionalLogicalPath(llf, "llf");
            const auto dlf_path = optionalLogicalPath(dlf, "dlf");
            const WriteTargetFormat format = writeTargetFormat(logical_target, WriteTargetFormat::kNative);
            if (format == WriteTargetFormat::kGlb && (llf_path || dlf_path || level_index))
              throw nb::value_error("GLB output does not accept llf, dlf, or level_index");
            if (level_index && format != WriteTargetFormat::kJson)
              throw nb::value_error("level_index applies only to FTS JSON output");
            const auto options = levelWriteOptions(logical_target,
                                                   text_mode,
                                                   units,
                                                   offset,
                                                   reconstruct_quads,
                                                   compress,
                                                   embed_lighting,
                                                   signer,
                                                   outputs,
                                                   recover_case_collisions);
            return prepareWritePlan(self, if_exists, [&] {
              return self.prepareLevelOutputs(level,
                                              logical_target,
                                              llf_path ? std::optional<std::string_view>(*llf_path) : std::nullopt,
                                              dlf_path ? std::optional<std::string_view>(*dlf_path) : std::nullopt,
                                              level_index,
                                              options);
            });
          },
          nb::arg("level"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("llf") = nb::none(),
          nb::arg("dlf") = nb::none(),
          nb::arg("level_index") = nb::none(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("arx_offset") = nb::none(),
          nb::arg("reconstruct_quads") = nb::none(),
          nb::arg("compress") = nb::none(),
          nb::arg("embed_lighting") = nb::none(),
          nb::arg("signer") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError,
          nb::arg("recover_case_collisions") = false)
      .def(
          "prepare_level_file_write",
          [](const resource_io::Resources& self,
             const PythonLevel& level,
             nb::handle target,
             nb::handle llf,
             nb::handle dlf,
             std::optional<std::uint32_t>
                 level_index,
             const std::optional<NativeTextMode>& text_mode,
             const std::optional<float>& units,
             const std::optional<ArxVector3>& offset,
             const std::optional<bool>& reconstruct_quads,
             const std::optional<bool>& compress,
             const std::optional<bool>& embed_lighting,
             const std::optional<std::string>& signer,
             resource_io::ResourceOutputParts outputs,
             resource_io::ExistingFilePolicy if_exists) {
            const auto path = nativePath(target);
            if (!path) throw nb::type_error("target must be str or PathLike");
            const auto llf_path = optionalNativePath(llf, "llf");
            const auto dlf_path = optionalNativePath(dlf, "dlf");
            const std::string target_path = pathToUtf8(*path);
            const WriteTargetFormat format = writeTargetFormat(target_path, WriteTargetFormat::kUnknown);
            if (format == WriteTargetFormat::kGlb && (llf_path || dlf_path || level_index))
              throw nb::value_error("GLB output does not accept llf, dlf, or level_index");
            if (level_index && format != WriteTargetFormat::kJson)
              throw nb::value_error("level_index applies only to FTS JSON output");
            const auto options = levelWriteOptions(target_path,
                                                   text_mode,
                                                   units,
                                                   offset,
                                                   reconstruct_quads,
                                                   compress,
                                                   embed_lighting,
                                                   signer,
                                                   outputs,
                                                   false);
            return prepareWritePlan(self, if_exists, [&] {
              return self.prepareLevelFileOutputs(level, *path, llf_path, dlf_path, level_index, options);
            });
          },
          nb::arg("level"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("llf") = nb::none(),
          nb::arg("dlf") = nb::none(),
          nb::arg("level_index") = nb::none(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("arx_offset") = nb::none(),
          nb::arg("reconstruct_quads") = nb::none(),
          nb::arg("compress") = nb::none(),
          nb::arg("embed_lighting") = nb::none(),
          nb::arg("signer") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError)
      .def(
          "write_level",
          [level_selector_type](const resource_io::Resources& self,
                                const PythonLevel& level,
                                nb::handle target,
                                nb::handle llf,
                                nb::handle dlf,
                                std::optional<std::uint32_t>
                                    level_index,
                                const std::optional<NativeTextMode>& text_mode,
                                const std::optional<float>& units,
                                const std::optional<ArxVector3>& offset,
                                const std::optional<bool>& reconstruct_quads,
                                const std::optional<bool>& compress,
                                const std::optional<bool>& embed_lighting,
                                const std::optional<std::string>& signer,
                                resource_io::ResourceOutputParts outputs,
                                resource_io::ExistingFilePolicy if_exists,
                                bool recover_case_collisions) {
            const std::string logical_target = mountedTarget(target, level_selector_type, "target");
            const auto llf_path = optionalLogicalPath(llf, "llf");
            const auto dlf_path = optionalLogicalPath(dlf, "dlf");
            const WriteTargetFormat format = writeTargetFormat(logical_target, WriteTargetFormat::kNative);
            if (format == WriteTargetFormat::kGlb && (llf_path || dlf_path || level_index))
              throw nb::value_error("GLB output does not accept llf, dlf, or level_index");
            if (level_index && format != WriteTargetFormat::kJson)
              throw nb::value_error("level_index applies only to FTS JSON output");
            const auto options = levelWriteOptions(logical_target,
                                                   text_mode,
                                                   units,
                                                   offset,
                                                   reconstruct_quads,
                                                   compress,
                                                   embed_lighting,
                                                   signer,
                                                   outputs,
                                                   recover_case_collisions);
            return finishWrite(self, if_exists, [&] {
              return self.prepareLevelOutputs(level,
                                              logical_target,
                                              llf_path ? std::optional<std::string_view>(*llf_path) : std::nullopt,
                                              dlf_path ? std::optional<std::string_view>(*dlf_path) : std::nullopt,
                                              level_index,
                                              options);
            });
          },
          nb::arg("level"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("llf") = nb::none(),
          nb::arg("dlf") = nb::none(),
          nb::arg("level_index") = nb::none(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("arx_offset") = nb::none(),
          nb::arg("reconstruct_quads") = nb::none(),
          nb::arg("compress") = nb::none(),
          nb::arg("embed_lighting") = nb::none(),
          nb::arg("signer") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError,
          nb::arg("recover_case_collisions") = false)
      .def(
          "write_level_file",
          [](const resource_io::Resources& self,
             const PythonLevel& level,
             nb::handle target,
             nb::handle llf,
             nb::handle dlf,
             std::optional<std::uint32_t>
                 level_index,
             const std::optional<NativeTextMode>& text_mode,
             const std::optional<float>& units,
             const std::optional<ArxVector3>& offset,
             const std::optional<bool>& reconstruct_quads,
             const std::optional<bool>& compress,
             const std::optional<bool>& embed_lighting,
             const std::optional<std::string>& signer,
             resource_io::ResourceOutputParts outputs,
             resource_io::ExistingFilePolicy if_exists) {
            const auto path = nativePath(target);
            if (!path) throw nb::type_error("target must be str or PathLike");
            const auto llf_path = optionalNativePath(llf, "llf");
            const auto dlf_path = optionalNativePath(dlf, "dlf");
            const std::string target_path = pathToUtf8(*path);
            const WriteTargetFormat format = writeTargetFormat(target_path, WriteTargetFormat::kUnknown);
            if (format == WriteTargetFormat::kGlb && (llf_path || dlf_path || level_index))
              throw nb::value_error("GLB output does not accept llf, dlf, or level_index");
            if (level_index && format != WriteTargetFormat::kJson)
              throw nb::value_error("level_index applies only to FTS JSON output");
            const auto options = levelWriteOptions(target_path,
                                                   text_mode,
                                                   units,
                                                   offset,
                                                   reconstruct_quads,
                                                   compress,
                                                   embed_lighting,
                                                   signer,
                                                   outputs,
                                                   false);
            return finishWrite(self, if_exists, [&] {
              return self.prepareLevelFileOutputs(level, *path, llf_path, dlf_path, level_index, options);
            });
          },
          nb::arg("level"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("llf") = nb::none(),
          nb::arg("dlf") = nb::none(),
          nb::arg("level_index") = nb::none(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("arx_offset") = nb::none(),
          nb::arg("reconstruct_quads") = nb::none(),
          nb::arg("compress") = nb::none(),
          nb::arg("embed_lighting") = nb::none(),
          nb::arg("signer") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError)
      .def(
          "prepare_ambiance_write",
          [ambiance_selector_type](const resource_io::Resources& self,
                                   const PythonAmbiance& ambiance,
                                   nb::handle target,
                                   const std::optional<NativeTextMode>& text_mode,
                                   const std::optional<float>& units,
                                   resource_io::ResourceOutputParts outputs,
                                   resource_io::ExistingFilePolicy if_exists,
                                   bool recover_case_collisions) {
            const std::string logical_target = mountedTarget(target, ambiance_selector_type, "target");
            const auto options =
                ambianceWriteOptions(logical_target, text_mode, units, outputs, recover_case_collisions);
            return prepareWritePlan(
                self, if_exists, [&] { return self.prepareAmbianceOutputs(ambiance, logical_target, options); });
          },
          nb::arg("ambiance"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError,
          nb::arg("recover_case_collisions") = false)
      .def(
          "prepare_ambiance_file_write",
          [](const resource_io::Resources& self,
             const PythonAmbiance& ambiance,
             nb::handle target,
             const std::optional<NativeTextMode>& text_mode,
             const std::optional<float>& units,
             resource_io::ResourceOutputParts outputs,
             resource_io::ExistingFilePolicy if_exists) {
            const auto path = nativePath(target);
            if (!path) throw nb::type_error("target must be str or PathLike");
            const auto options = ambianceWriteOptions(pathToUtf8(*path), text_mode, units, outputs, false);
            return prepareWritePlan(
                self, if_exists, [&] { return self.prepareAmbianceFileOutputs(ambiance, *path, options); });
          },
          nb::arg("ambiance"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError)
      .def(
          "write_ambiance",
          [ambiance_selector_type](const resource_io::Resources& self,
                                   const PythonAmbiance& ambiance,
                                   nb::handle target,
                                   const std::optional<NativeTextMode>& text_mode,
                                   const std::optional<float>& units,
                                   resource_io::ResourceOutputParts outputs,
                                   resource_io::ExistingFilePolicy if_exists,
                                   bool recover_case_collisions) {
            const std::string logical_target = mountedTarget(target, ambiance_selector_type, "target");
            const auto options =
                ambianceWriteOptions(logical_target, text_mode, units, outputs, recover_case_collisions);
            return finishWrite(
                self, if_exists, [&] { return self.prepareAmbianceOutputs(ambiance, logical_target, options); });
          },
          nb::arg("ambiance"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError,
          nb::arg("recover_case_collisions") = false)
      .def(
          "write_ambiance_file",
          [](const resource_io::Resources& self,
             const PythonAmbiance& ambiance,
             nb::handle target,
             const std::optional<NativeTextMode>& text_mode,
             const std::optional<float>& units,
             resource_io::ResourceOutputParts outputs,
             resource_io::ExistingFilePolicy if_exists) {
            const auto path = nativePath(target);
            if (!path) throw nb::type_error("target must be str or PathLike");
            const auto options = ambianceWriteOptions(pathToUtf8(*path), text_mode, units, outputs, false);
            return finishWrite(
                self, if_exists, [&] { return self.prepareAmbianceFileOutputs(ambiance, *path, options); });
          },
          nb::arg("ambiance"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("arx_units_per_glb_unit") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError)
      .def(
          "prepare_cinematic_write",
          [cinematic_selector_type](const resource_io::Resources& self,
                                    const PythonCinematic& cinematic,
                                    nb::handle target,
                                    const std::optional<NativeTextMode>& text_mode,
                                    const std::optional<CinematicIllustrationFormat>& illustration_format,
                                    resource_io::ResourceOutputParts outputs,
                                    resource_io::ExistingFilePolicy if_exists,
                                    bool recover_case_collisions) {
            const std::string logical_target = mountedTarget(target, cinematic_selector_type, "target");
            const auto options =
                cinematicWriteOptions(logical_target, text_mode, illustration_format, outputs, recover_case_collisions);
            return prepareWritePlan(
                self, if_exists, [&] { return self.prepareCinematicOutputs(cinematic, logical_target, options); });
          },
          nb::arg("cinematic"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("illustration_format") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError,
          nb::arg("recover_case_collisions") = false)
      .def(
          "prepare_cinematic_file_write",
          [](const resource_io::Resources& self,
             const PythonCinematic& cinematic,
             nb::handle target,
             const std::optional<NativeTextMode>& text_mode,
             const std::optional<CinematicIllustrationFormat>& illustration_format,
             resource_io::ResourceOutputParts outputs,
             resource_io::ExistingFilePolicy if_exists) {
            const auto path = nativePath(target);
            if (!path) throw nb::type_error("target must be str or PathLike");
            const auto options =
                cinematicWriteOptions(pathToUtf8(*path), text_mode, illustration_format, outputs, false);
            return prepareWritePlan(
                self, if_exists, [&] { return self.prepareCinematicFileOutputs(cinematic, *path, options); });
          },
          nb::arg("cinematic"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("illustration_format") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError)
      .def(
          "write_cinematic",
          [cinematic_selector_type](const resource_io::Resources& self,
                                    const PythonCinematic& cinematic,
                                    nb::handle target,
                                    const std::optional<NativeTextMode>& text_mode,
                                    const std::optional<CinematicIllustrationFormat>& illustration_format,
                                    resource_io::ResourceOutputParts outputs,
                                    resource_io::ExistingFilePolicy if_exists,
                                    bool recover_case_collisions) {
            const std::string logical_target = mountedTarget(target, cinematic_selector_type, "target");
            const auto options =
                cinematicWriteOptions(logical_target, text_mode, illustration_format, outputs, recover_case_collisions);
            return finishWrite(
                self, if_exists, [&] { return self.prepareCinematicOutputs(cinematic, logical_target, options); });
          },
          nb::arg("cinematic"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("illustration_format") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError,
          nb::arg("recover_case_collisions") = false)
      .def(
          "write_cinematic_file",
          [](const resource_io::Resources& self,
             const PythonCinematic& cinematic,
             nb::handle target,
             const std::optional<NativeTextMode>& text_mode,
             const std::optional<CinematicIllustrationFormat>& illustration_format,
             resource_io::ResourceOutputParts outputs,
             resource_io::ExistingFilePolicy if_exists) {
            const auto path = nativePath(target);
            if (!path) throw nb::type_error("target must be str or PathLike");
            const auto options =
                cinematicWriteOptions(pathToUtf8(*path), text_mode, illustration_format, outputs, false);
            return finishWrite(
                self, if_exists, [&] { return self.prepareCinematicFileOutputs(cinematic, *path, options); });
          },
          nb::arg("cinematic"),
          nb::arg("target"),
          nb::kw_only(),
          nb::arg("text_mode") = nb::none(),
          nb::arg("illustration_format") = nb::none(),
          nb::arg("outputs") = resource_io::kResourceOutputAll,
          nb::arg("if_exists") = resource_io::ExistingFilePolicy::kError);
}

}  // namespace pistoris::python
