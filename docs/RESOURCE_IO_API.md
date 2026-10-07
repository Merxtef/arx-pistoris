# Resource I/O API Guide

This guide covers the optional C++20 resource library. It resolves canonical
game layouts from ordered filesystem roots and provides complete mounted or
loose conversion with dependent media. Lower-level in-memory conversion remains
in the [Core API Guide](CORE_API.md); interactive overwrite, dry-run, and prompt
policy remain CLI responsibilities.

The API is pre-1.0. Source and ABI compatibility are not promised between
minor releases.

## Build Target And Headers

Link `arx_pistoris::resource_io` and include
`<arx_pistoris/resource_io.hpp>`. Focused declarations live under
`arx_pistoris/resource_io/`:

| Header | Purpose |
| --- | --- |
| `resource_mounts.hpp` | Mount configuration, live reads, writes, and directory traversal |
| `catalog.hpp` | Immutable selector catalog snapshots |
| `document.hpp` | Owned input bytes, format classification, and source provenance |
| `native_bundle.hpp` | Immutable native carriers with resolved companion-resource files |
| `resources.hpp` | Resource discovery, complete conversion, and output preparation |
| `output.hpp` | Output selection, destination conflicts, overwrite decisions, and write status |
| `location.hpp` | `ResourceIoResult<T>` and structured failure locations |
| `status.h` | C-compatible Resource I/O return codes and error strings |

Mount management, complete resource conversion, and write-plan orchestration
have C++ and Python surfaces. Owned documents and native bundles are currently
C++ APIs. The status constants and error-string function are
C-compatible so a later C operations surface can use the same contract. Python
exposes the subsystem as `pistoris.resource_io`.

## Mount Configuration

`ResourceMounts::open()` validates up to 64 ordered read roots and one
independent write root. Earlier read roots have higher priority. Each accepted
read root receives one stable single-bit ID, so any subset is a
`ResourceMountMask`. `kAllResourceMounts` selects every accepted root and mask
`0` searches none.

```cpp
#include <arx_pistoris/resource_io.hpp>

pistoris::resource_io::MountValidationReport report;
auto opened = pistoris::resource_io::ResourceMounts::open(
    {.read_mounts = {"user-data", "unpacked"}, .write_mount = "output"},
    &report);
if (!opened) {
  // Inspect opened.error().
}
```

Missing read roots are reported and omitted. Duplicate roots are reported and
do not consume IDs. A missing write root is accepted as prospective and is
created on the first write. `MountValidationReport` contains these non-fatal
configuration messages; operational failures use `ResourceIoResult<T>`.
Stored mount paths are absolute and canonicalize their deepest existing
ancestor; a prospective write suffix remains appended to that canonical path.

An empty `ResourceMounts` can be configured incrementally. `addReadMount()`
appends one existing root without changing earlier IDs. Adding the same physical
root again is a silent no-op. `setWriteMount()` replaces the independent write
root; pass `std::nullopt` to clear it. Each mutation validates into replacement
state and publishes only on success.

`addLibertatisMounts()` appends the platform Libertatis data root and then its
`unpacked/` child. Existing physical roots keep their original position and are
skipped, so repeated calls are idempotent. `setLibertatisWriteMount()` selects
the platform data root only for writes. Neither operation implies the other.
The namespace-level `libertatisResourceRoot()` returns the platform root
without configuring it.
Explicit mounts remain preferable for reproducible project-local tools.

```cpp
pistoris::resource_io::ResourceMounts mounts;
auto added = mounts.addReadMount("project-data", &report);
auto defaults = mounts.addLibertatisMounts(&report);
auto write = mounts.setWriteMount("output", &report);
```

Move construction and move assignment leave the source disengaged. Its mount
inspectors return empty values; filesystem operations fail with
`ARX_INVALID_STATE`. Copying a disengaged value produces another disengaged
value.

## Live Lookup

All lookup calls inspect the filesystem at call time:

- `resolve()` selects the highest-priority regular file for a logical path.
- `read()` resolves and reads that file, returning bytes, native path, and the
  supplying mount ID.
- `listDirectory()` returns immediate visible children for mounted browsers.
- `enumerate()` recursively returns visible files up to the requested depth.
- `resolveWritePath()` maps a logical path below the independent write root.
- `write()` creates parent directories and replaces the destination file.

Logical paths are relative, portable `/`-separated resource paths. Lookup is
ASCII case-insensitive. A higher-priority file masks a lower-priority
directory with the same name and vice versa. Equivalent entries of the same
kind aggregate their provider mask. Symbolic links below mount roots are not
followed for reads, and write paths containing one are rejected.
When a write path crosses existing directories, lookup remains ASCII
case-insensitive and preserves their native spelling instead of creating a
case-only parallel tree on case-sensitive filesystems.

Two entries in one native directory that differ only by ASCII case are
ambiguous under this lookup model. Operations fail with
`ARX_RESOURCE_IO_AMBIGUOUS_PATH` by default. Pass
`kResourceIoRecoverCaseCollisions` in the call's `ResourceIoFlags` to warn and
select the lexically first native spelling instead. This recovery is
deterministic but does not claim which entry the engine would select. Entries
with the same logical path in different mounts are normal overrides, not a
collision. Bits outside `kResourceIoFlagsAll` are rejected with
`ARX_INVALID_OPTIONS`.

Every read, listing, enumeration, catalog, and high-level load accepts a mount
subset through `ResourceLookupOptions { mount_mask, flags }`. The format's load
options precede this shared lookup object on high-level loaders. Explicit
filesystem and retained-document overloads do not accept lookup options because
they do not search mounts. Use `availableMounts()` to obtain accepted roots and
`highestPriorityMountId()` when one supplying root is needed from a provider
mask; it returns `std::nullopt` when no configured mount is selected.

## Directory Entries

`ResourceDirectoryEntry` contains the immediate name, provider mask, and a
`Kind`. Kinds distinguish directories, unknown files, native formats, external
formats, images, and audio. Classification is based on the visible filename;
it does not parse the file.

`ResourceFile` contains a complete logical path relative to the mounts and the
mask of equivalent providers. Neither type caches filesystem state.

## Catalogs

`Resources::scanCatalog()` performs a fresh scan on every call. It recognizes
registered Model, Animation, Level, Ambiance, and Cinematic layouts and returns
an immutable `ResourceCatalog` snapshot. Each entry contains:

- the canonical selector string;
- its parsed `paths::ResourceSelector`;
- the mask of roots that provide the resource.

The caller decides whether and how long to cache a snapshot. Call
`scanCatalog()` again to refresh it.

```cpp
pistoris::resource_io::Resources resources;
resources.mounts() = std::move(*opened);
auto catalog = resources.scanCatalog();
if (catalog) {
  for (const auto& entry : catalog->entries()) {
    // entry.selector, entry.resource, entry.mount_mask
  }
}
```

## Documents And Native Bundles

`readDocument()` reads a mounted logical path or selector, while
`readDocumentFile()` reads an explicit native filesystem path. Both return an
owned `ResourceDocument`: its bytes remain usable if the source file is later
changed or removed. The document also records its detected format and payload,
logical and native paths, mounted or loose layout, supplying mount, requested
mount mask, and lookup flags. Classification identifies what the bytes appear
to contain; the corresponding decoder remains authoritative and can still
reject malformed input.

`classifyResource()` performs the same byte-and-filename classification without
reading or decoding a resource. It returns
`ResourceIoResult<ResourceClassification>`: unrecognized input is a successful
`kUnknown` classification, while allocation or internal failures remain errors.

The document overloads of `loadModel()`, `loadAnimation()`, `loadLevel()`,
`loadAmbiance()`, and `loadCinematic()` consume that snapshot instead of
rereading the primary file. Related files are still discovered at conversion
time because they are not part of a lone document.

Native bundles provide the lower boundary when an application needs the
carrier structs and dependency data separately. The
`load*NativeBundle()` operations decode binary native files or their JSON
projections, discover structural Level companions where applicable, and
return:

- immutable carrier members with their source provenance and native text mode;
- one coalesced `NativeResourceSet` of fetched images and audio;
- references from carrier elements to those files, including authored paths,
  roles, owners, and the result of optional-resource lookup.

Missing optional resources remain represented by references with no file.
Unreadable related resources fail by default. Set
`NativeBundleLoadOptions::suppress_related_resource_errors` to retain their
failure status in the bundle and continue with one warning per failed lookup.
This option does not suppress errors in explicitly supplied carrier documents.
Encoded media is validated when a bundle is materialized by a complete loader,
not while the immutable carrier bundle is assembled.

A `ModelNativeBundle` accepts an explicit span of TEA or TEA JSON documents.
Animations are never guessed from a Model filename. The Model, those Animation
carriers, their textures, icon, and sound files share one resource set, so the
same physical file is fetched and stored once. `AnimationNativeBundle` remains
available when no Model aggregate is needed.

`LevelNativeBundle` may contain geometry, lighting, and scene members. A
mounted DLF discovers its mandatory FTS from the stored scene path and an
optional same-stem LLF. A loose FTS discovers omitted same-stem LLF and DLF
companions. Explicit companions must exist and decode successfully; omitted
optional companions may be absent. An external LLF takes precedence over
lighting embedded in a DLF. The bundle also exposes the resolved minimap
projection offset used by complete Level materialization. Mounted Levels apply
the engine's level-specific projection after reading any stored minimap offset;
missing offset metadata supplies zero. Loose Levels use only their filename
suffix offset, or zero when the suffix is absent.

## Complete Resource Conversion

`Resources` default-constructs and owns an empty `ResourceMounts` value, exposed
through `mounts()` for configuration. It also accepts a prepared mount value at
construction. Every loader returns the normal editable core class, except Model
loading returns `LoadedModel { model, animations }` so animations embedded in a
GLB are not discarded. Native, OBJ, and JSON Models have an empty animation
vector. Loaders accept:

- a `paths::ResourceSelector` or semantic path view for a canonical game resource;
- a logical path string resolved through the read mounts;
- a native filesystem path through the explicitly named `load*File()` overload.

Logical Model, Animation, Ambiance, and Cinematic paths may omit their native
extension. For example, `game/.../helmet` and `game/.../helmet.ftl` identify
the same mounted Model. Explicit extensions select the conversion format.

| Operation | Accepted primary formats | Loaded sidecars |
| --- | --- | --- |
| `loadModel()` / `loadModelFile()` | FTL, FTL JSON, OBJ, GLB | MTL, referenced texture images, inventory icon, GLB animation audio |
| `loadAnimation()` / `loadAnimationFile()` | TEA, TEA JSON | Referenced SFX audio |
| `loadLevel()` / `loadLevelFile()` | Mounted DLF, loose FTS or FTS JSON, GLB | LLF/DLF companions, referenced texture images, minimap, loading screen |
| `loadAmbiance()` / `loadAmbianceFile()` | AMB, AMB JSON, GLB | Referenced SFX audio |
| `loadCinematic()` / `loadCinematicFile()` | CIN, GLB | Illustrations, SFX, discovered speech audio |

Complete loaders always discover and populate texture, image, and audio data.
There is no partial-loading switch on these operations. Use the native-bundle
API when a caller needs immutable carriers, explicit missing-resource status,
or control over when carriers are materialized into an editable object.

Mounted native Level input is DLF-primary. `loadLevel(level)` resolves the
canonical DLF, reads its stored scene path to find the mandatory FTS, and uses
the same-stem LLF when present. A loose native Level is FTS-primary:
`loadLevelFile(fts, llf, dlf)` requires the FTS path. Each omitted
LLF or DLF path is discovered beside that primary. Binary and JSON encodings
are selected independently, so `level.fts.json`, `level.llf`, and
`level.dlf.json` form one valid loose Level. If both encodings of one omitted
companion exist, discovery fails as ambiguous; supplying its path explicitly
selects one. GLB is a single loose input and rejects native companions.
Loose Level writers likewise derive omitted LLF and DLF destinations beside
the FTS primary and match its binary or JSON encoding. Supply either path to
override that companion independently, or exclude `kResourceOutputCompanions`
to emit no structural companions.

A logical Level string dispatches by its classified payload. Extensionless and
binary `.dlf` paths use the mounted native game-layout flow. `.fts`, FTS JSON,
and `.glb` select a mounted primary but then use loose-file dependency rules
beside the resolved physical file. A generic `.json` filename is classified
from its schema or root fields and is always loose. DLF JSON and LLF JSON are
valid Level companions, not primaries. Logical writers mirror that dispatch
while placing their outputs below the write mount.

FTS binary contains no level identity. FTS JSON does: `loadLevelFile()` stores
its `levelIdx` as the corresponding canonical DLF `Level::resourcePath()`. Level
identity follows the primary document when that document provides one: FTS JSON
uses its `levelIdx`, while canonical FTS and DLF logical paths use their level
number. Otherwise a DLF companion may provide the missing identity through its
logical or stored scene path. Companion discrepancies do not replace primary
identity. When writing Level JSON, the logical and native-file writers use their
explicit `level_index` when supplied, otherwise they derive the index from a
canonical Level resource path. One of those sources is required. The explicit
value is authoritative; raw FTS conversion remains the lower-level API when no
semantic Level identity is available.

Level lighting uses an external LLF when present and otherwise uses lighting
embedded in the DLF. Mounted Level identity is its DLF logical path. A loose
binary FTS used alone and GLB input do not invent a game-resource identity;
binary FTS may inherit one from a DLF companion, and FTS JSON retains the
explicit identity described above.

Media lookup first tries the exact referenced extension, then the other
supported image or audio extensions. Loose lookup preserves the authored path
spelling while probing those extensions; normalization of the path stored in
the imported object does not provide a second filesystem lookup name. A
missing optional sidecar leaves the resource's path-only entry or optional
image empty. Once a candidate exists, an unreadable or invalid file fails the
load and its diagnostic carries the resolved absolute native path. Native
Cinematic speech discovers languages from immediate directories below
`speech/`; a language is registered only when matching audio is found there.
Cinematic GLB speech instead discovers adjacent
`<path>[<language>].<ext>` files and ignores a language suffix accidentally
left in the GLB reference itself.

Lookup has two distinct dependency domains. JSON projections are always loose:
mounts may discover a logical JSON primary, but all of its dependencies resolve
relative to that primary's physical directory.

- mounted native input resolves every dependency as a game-layout logical path
  through the selected mount mask;
- JSON, OBJ, and GLB input is loose after its primary file is found. An
  explicit `*File()` primary is resolved as a filesystem path and bypasses
  mounts. A logical primary may be found through the selected mounts, but its
  MTL, textures, audio, and other sidecars resolve only as physical paths
  relative to that resolved file. Authored absolute OBJ MTL and media paths are
  read directly; relative paths remain relative to the primary file.

External dependencies never fall through to another mount. This preserves the
self-contained loose project selected by the primary file. Consequently the
mount mask and case-collision flag apply to all dependencies of mounted native
input, but only to primary discovery for a logical external input. The
explicit `*File()` overloads accept neither option.

Mounted item Models load and write their canonical inventory-icon resource.
Game-native output emits both PNG and Libertatis-compatible BMP. Other Model
formats use a sibling `<primary-stem>[icon].png`. Mounted Levels use their
canonical minimap and loading-screen paths. Loose Levels use sibling
`<primary-stem>[map].png` and `<primary-stem>[loading].png`; a nonzero minimap
projection offset is encoded as
`<primary-stem>[map][offset_<x>_<y>].png` and restored on load.

The symmetric `writeModel*()`, `writeAnimation*()`, `writeLevel*()`,
`writeAmbiance*()`, and `writeCinematic*()` methods choose the output format
from the extension and write returned sidecars with the primary output.
Logical targets use the configured write mount; `*File()` targets use native
filesystem paths. Extensionless logical targets default to the native format.
Mounted Level output is canonical DLF-primary and writes the corresponding FTS
and LLF. Loose Level output is FTS-primary with optional LLF and DLF
destinations and the optional JSON `level_index` described above. Omitted
companion destinations use the primary encoding; explicit destinations choose
their encoding independently by extension. Different payloads targeting one
resolved output path fail before any file is written rather than silently
choosing one.

Each resource family has a matching write-options type. Its `resource` member
is a `ResourceOutputOptions` value that selects output categories and controls
logical-path resolution while candidates are generated. The remaining members
carry only conversion settings meaningful to that family: native text mode,
GLB unit scale and Level offset, Model and Level compression, Level quad
reconstruction and DLF/LLF metadata, or Cinematic illustration format.
Sidecar inclusion is not duplicated there; `resource.outputs` is authoritative.

Every format-specific preparation produces owned `ResourceOutput` records.
`ResourceOutputs` is the owning collection returned by `prepare*Outputs()` and
`prepare*FileOutputs()`. Each output
records whether its address is logical or native, whether it is the requested
primary, its bytes, media kind, and the semantic kind and identity that
produced it. This raw preparation boundary lets applications inspect or combine
outputs from several resources before any destination grouping or destination
collision inspection occurs. Preparation may still read source metadata needed
to render an output, such as the mounted Level minimap offsets. Logical outputs
also retain the I/O flags used to resolve their destinations, so combined
outputs do not require those flags to be supplied again.

`prepareWrite()` canonicalizes each destination's existing filesystem prefix,
groups outputs by the resulting native destination, and returns a
`ResourceWritePlan`. This makes alternate native spellings of the same target,
such as Windows short and long paths, one destination. Its optional
`ResourceWriteOptions` argument sets the plan's default
`existing_file_policy`. Each `ResourceWriteEntry` is one destination and
exposes all producer candidates for that file. Byte-identical candidates are
coalesced automatically. Different payloads require the caller to select a
candidate with `selectCandidate()`; this producer decision is independent from
whether an existing filesystem file may be replaced.

Call `ResourceWritePlan::preflight()` after candidate selection. It inspects
current destinations, updates each entry's `ResourceWriteStatus`, and returns
an immutable `ResourceWriteReport` snapshot:

- a missing destination becomes `kReady`;
- identical existing bytes become `kAlreadyCurrent`;
- differing existing bytes use the entry policy, falling back to the plan's
  default `ExistingFilePolicy`;
- an entry with no override inherits the plan default; the default `kError`
  becomes `kNeedsExistingFilePolicy` for differing bytes;
- preserving an existing file becomes `kPreserved`.

`ResourceWritePlan::execute()` repeats preflight immediately before writing and
returns a final `ResourceWriteReport`. Known unresolved
candidate or overwrite decisions return `ARX_RESOURCE_IO_DECISION_REQUIRED`
before any output is changed. Ready files are written through a temporary file
and atomically replace their destinations. A later I/O failure can therefore
leave earlier entries marked `kWritten`; retry the same plan after handling the
failure and those entries are skipped. `ResourceOutput::written` acknowledges
every identical producer candidate satisfied by that write.

Direct `write*()` methods perform the same grouping and execution internally.
They accept format-specific output options followed by a separate
`ResourceWriteOptions`. Its `existing_file_policy` applies to every differing
existing destination. The default `kError` returns
`ARX_RESOURCE_IO_DECISION_REQUIRED`; `kOverwrite` replaces it and `kPreserve`
keeps it. Successful direct writers return the same immutable report instead
of discarding per-destination outcomes. For per-file prompts or cross-resource
collision handling, combine the raw outputs, call `prepareWrite()`, then invoke
`preflight()` and `execute()` on the returned plan. The plan owns everything
needed for those operations; it does not retain or require its originating
`Resources`. Prompting, dry-run presentation, and retry limits remain
application policy rather than library behavior.

The remaining format-specific members configure conversion before output
planning:

- `ModelWriteOptions`: `glb`, `native_text_mode`, and `compress`;
- `AnimationWriteOptions`: `native_text_mode`;
- `LevelWriteOptions`: `glb`, `native_text_mode`, `reconstruct_quads`,
  `compress`, `embed_lighting`, and `signer`;
- `AmbianceWriteOptions`: `glb` and `native_text_mode`;
- `CinematicWriteOptions`: `native_text_mode` and `illustration_format`.

Except for Animation's universally applicable text setting, format-specific C++
members are `std::optional`. Omitted members select the target format's normal
default. Supplying a setting that is irrelevant to the selected extension
returns `ARX_INVALID_OPTIONS`; this prevents a typo or reused options object
from being silently ignored. Python exposes the same rule as `ValueError`.

`ResourceOutputOptions::outputs`, reached through the format-specific options'
`resource` member, selects output categories with the
`kResourceOutput*` bit flags. `kResourceOutputPrimary` is exactly the requested
target. `kResourceOutputCompanions` covers structural companions such as MTL,
FTS, and LLF. Textures, audio, and other images have independent flags. The
default is `kResourceOutputAll`. A primary-only native Level or OBJ may be
incomplete by design; selection changes what is emitted, not format validity.
`ResourceOutputOptions::io_flags` applies to logical destination resolution.
`ResourceWriteOptions::existing_file_policy` configures a direct writer or the
default policy of a manually assembled plan. Unknown output bits, I/O flags,
and `ExistingFilePolicy` values return `ARX_INVALID_OPTIONS` rather than being
ignored.

In Python, `Resources.write_*()` and `write_*_file()` default `if_exists` to
`ExistingFilePolicy.ERROR`, preflight the complete call before writing, and
return an immutable `resource_io.output.WriteReport`. Use the corresponding
`prepare_*_write()` method for per-destination decisions. Its mutable
`WritePlan` exposes `default_if_exists`, individual entries, candidate
selection, `preflight()`, and `execute()`; the plan owns the encoded outputs
and resolved destinations needed for execution.

Python calls the mount subset argument `mount_mask`, the supplying bit on a
single result `mount_id`, and the incremental mount method `add_read_mount()`.
Catalog snapshots provide exact `get(selector)` lookup plus callable,
read-only filtered views through `models()`, `animations()`, `levels()`,
`cinematics()`, and `ambiances()`.

## Errors And Concurrency

Every fallible operation returns `ResourceIoResult<T>`. Its
`ResourceIoLocation` identifies the operation, logical `resource_path`, native
filesystem `native_path`, and relevant mount mask. A field is empty when that
side of resolution is not known: a lookup miss has only a resource path, while
mount opening can have only a native path. Parse and validation failures after
a successful read carry both and preserve the parser or converter's typed leaf
location in `content_location`. The variant is flat: callers inspect one GLB,
OBJ, JSON, native-carrier, native-binary, or IR location without decoding a
nested error string. The outer `detail` remains the original detail rather than
a rendered copy of that inner error. The first function that creates a failure
also emits one debug log with the source location; propagation and
conversion-error remapping are silent.

Resource I/O owns the negative `ArxReturnCode` range `-1000` through `-1`.
Use the `ARX_RESOURCE_IO_*` symbolic names rather than their numeric values.
Complete resource conversions can also return a non-negative core code when
parsing, conversion, or IR validation fails. `resource_io::errorString()` describes
either kind; the equivalent C-compatible entry point is
`arx_pistoris_resource_io_strerror()`. Core's `arx_pistoris_strerror()` only
owns core codes and does not know about this dependent library.

Python exposes the same location data as `PistorisError.location.operation`,
using `pistoris.resource_io.Operation`, together with `resource_path`,
`native_path`, and `mount_mask`. Conversion errors additionally expose the
normalized fields from the typed content location, such as GLB element/index
or native field/byte offset. Python operations expose collision recovery as
the keyword-only `recover_case_collisions=False` argument on mounted methods.
Filesystem methods do not expose mount options, so no option can be silently
ignored.

Objects do not cache filesystem contents. Separate mount objects may be used
concurrently. Synchronize access while a mount configuration, the underlying
files, or the same write destination can change. Python releases the global
interpreter lock for mount configuration, platform-root discovery, reads,
resolution, writes, directory operations, catalog scans, and complete resource
conversions.

## Python Layout

`pistoris.resource_io` exposes the two principal classes, `ResourceMounts` and
`Resources`, together with `Operation`, `OutputPart`, and `ALL_MOUNTS`. Mount
records, listing records, and their enums live under
`pistoris.resource_io.mounts`; catalog snapshots and entries live under
`pistoris.resource_io.catalog`.

```python
from pathlib import Path

import pistoris

resources = pistoris.resource_io.Resources([Path("project-data")])
resources.mounts.add_libertatis_mounts()
resources.mounts.write_mount = Path("output")

for entry in resources.scan_catalog():
    print(str(entry.selector))

loaded = resources.load_model("model:npc:human_base")
resources.write_model(loaded.model, "game/graph/obj3d/interactive/npc/copy/copy")

loose = resources.load_model_file(Path("editing/model.glb"))
resources.write_model_file(loose, Path("editing/model-copy.glb"))
resources.write_model_file(loose.model, Path("editing/model.ftl"))
```

`Resources.mounts` is the live configuration owned by that `Resources`
instance and is read-only as a property; mutate the returned `ResourceMounts`.
`read_mounts` is an immutable tuple snapshot. Missing read roots emit
`UserWarning` and are omitted; repeated additions are silent. Assign `None` to
`write_mount` to clear it. `set_libertatis_write_mount()` selects the platform
write root without changing reads.

`ResourceMounts.list_directory()` returns immediate visible children, while
`list_files(max_depth=...)` recursively returns visible files. The latter name
is intentionally distinct from Python's built-in `enumerate()`. Provider masks
remain integers suitable for bitwise operations. Given such a mask,
`highest_priority_mount()` returns its first configured `mounts.Mount`, or
`None` when it selects no configured provider.

Python uses separate address-space methods. `load_*()` and `write_*()` accept
mounted logical `str`/`PathLike` values or the applicable concrete selector;
`load_*_file()` and `write_*_file()` accept `str` and `PathLike` native
filesystem paths. `ModelSelector`, `AnimationSelector`, `LevelSelector`,
`CinematicSelector`, and `AmbianceSelector` are validated semantic identities.
Catalog entries expose one of these concrete values through `selector`;
`pistoris.paths.ResourceSelector` is their annotation-only union. A mounted
external path still discovers its primary through the selected mounts and then
resolves sidecars beside the resolved physical file. File methods never consult
mounts.

Complete Python loaders expose native text mode and applicable GLB placement
settings as keyword-only arguments instead of exposing the C++ option-holder
structs. Writers likewise expose format-specific conversion settings alongside
`outputs=` and `if_exists=`. Defaults match the direct core conversion methods.
Mask-valued records use `provider_mask`; the configured aggregate is
`ResourceMounts.available_mount_mask`. A read or resolution result identifies
its single selected provider through `mount_id`.
