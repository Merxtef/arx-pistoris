# Core API Guide

This guide is for C and C++ callers integrating the in-memory Pistoris core.
Mounted filesystem lookup is documented in the
[Resource I/O API Guide](RESOURCE_IO_API.md); CLI policy is documented in the
[CLI Guide](CLI.md).

Pistoris exposes a C++20 API and a C ABI. Core operations accept in-memory
input and return carriers, handles, or encoded buffers in memory. They do not
open source references or choose filesystem layouts.

The API is pre-1.0. Source and ABI compatibility are not promised between
minor releases.

## Conversion Layers

Pistoris keeps serialization, format carriers, and semantic editing separate:

```text
native bytes <-> native carrier <-> editing class <-> authoring projection
```

- **Native bytes** are encoded FTL, TEA, FTS, DLF, LLF, AMB, or CIN data.
- **Native carriers** closely represent one decoded format. Read and write
  them for binary inspection or compatible-JSON interchange.
- **Editing classes** combine and validate data as a usable Model, Animation,
  Level, Ambiance, or Cinematic. Their native `import` and `bake` operations
  are the semantic boundary between carriers and editable resources.
- **Authoring projections** always pass through an editing class rather than
  preserving native layout. GLB covers Level, Model, Ambiance, and Cinematic,
  with Animation authored through Model GLB. OBJ covers static Models only.

Images and audio remain explicit sidecars in core conversions. Import
overloads with source outputs return caller-facing lookup references. Native
imports provide canonical
logical resource paths; external formats may preserve their authored lookup
spelling. Bake and export results return encoded sidecar files for the caller
to place. Core conversions do not infer mounts or open those paths. A direct
native-carrier conversion avoids an editing-class rebuild; generation,
validation, rebasing, GLB, and OBJ work on the semantic resource and can
therefore canonicalize native structure.

Logical resource paths identify game resources and use portable `/`
separators. Filesystem paths belong to the caller. The companion resource-I/O
library can resolve canonical game layouts; the CLI also resolves paths
relative to loose inputs and writes returned sidecars into the selected output
layout.

## Build Targets and Headers

The current CMake project provides build-tree integration:

| Surface | CMake target | Umbrella header | Library |
| --- | --- | --- | --- |
| C++20 | `arx_pistoris_cpp` | `arx_pistoris/pistoris.hpp` | static |
| C | `arx_pistoris_c` | `arx_pistoris/arx_pistoris.h` | shared |

The C++ target defines `ARX_PISTORIS_CPP_API`, which the umbrella header
requires. The produced C shared library is named `arx_pistoris`.

Focused public headers are grouped as follows:

| Concern | C++20 headers | C headers |
| --- | --- | --- |
| Editing resources | `ambiance.hpp`, `animation.hpp`, `cinematic.hpp`, `level.hpp`, `model.hpp` | `{ambiance,animation,cinematic,level,model}.h` and matching `types.h` files |
| Resource conversion | `{ambiance,animation,cinematic,level,model}/bake.hpp`; `cinematic/glb.hpp`; `model/glb.hpp`; `model/obj.hpp`; `level/images.hpp` | `level/images.h` |
| Locations and errors | `error.hpp`; `{ambiance,animation,cinematic,level,model}/location.hpp`; `glb/location.hpp`; `json/location.hpp`; `model/obj_location.hpp` | `base/error.h` |
| Native carriers | `native.hpp`; `native/{amb,cin,dlf,ftl,fts,llf,tea}.hpp`; `native/location.hpp`; `native/text.hpp` | `native.h`; `native/text.h` |
| Binary, paths, and media | `binary.hpp`, `paths.hpp`, `sound.hpp`, `texture.hpp`, `glb.hpp`, `cinematic/sound.hpp` | `binary.h`, `paths.h`, `sound.h`, `texture.h`, `glb.h`, and `paths/types.h` |
| Runtime and shared values | `runtime.hpp`; `base/{image,indexed_view,location,math,result}.hpp` | `runtime.h`, `runtime/types.h`, and `base/{abi,audio,buffer,error,flags,image,indices,math,status,string_view}.h` |
| Optional Level debug API | `debug/level.hpp`, `debug/level/diagnostics.hpp` | - |

Paths in the table are below `arx_pistoris/`; braces list alternative file or
directory names. Include the umbrella header unless a focused dependency is
useful.

`arx_pistoris/glb.h` and `arx_pistoris/glb.hpp` expose the inclusive supported
range for Arx-units-per-GLB-unit options. Resource-specific defaults remain on
their corresponding option types.

`arx_pistoris/base/buffer.h` declares the deallocators for buffers returned by
the C ABI. `arx_pistoris/base/abi.h` contains declaration macros used by the C
headers and normally does not need to be included directly.

Public `.h` files are C-compatible. Public `.hpp` files require C++20.

## C Initialization

C options with an `ARX_*_INIT` macro use the same defaults as their C++
counterparts. Use that macro instead of zero-initializing the option record.

Other C input records are fully specified values unless their declaration
states otherwise. Zero initialization is not a semantic initializer. Set
sentinel fields explicitly with `ARX_INVALID_INDEX`, `ARX_NO_TEXTURE`, or
`ARX_NO_SOUND`, use `ARX_NO_SOUND_HANDLE` for an absent Cinematic sound, and
use `ARX_QUAT_IDENTITY_INIT` for identity quaternions. Cinematic illustration
and language sentinels are `ARX_INVALID_CINEMATIC_ILLUSTRATION` and
`ARX_INVALID_LANGUAGE_ID`.

Each editing-class conversion operation has one C entry point. Optional source
path, related-asset, report, and sidecar output pointers control whether those
results are produced; pass `NULL` to omit them. The primary converted asset
output remains required.

Null GLB, OBJ export, Level welding, Level generation, native Sound bake,
Cinematic native bake, DLF write, and LLF write option pointers select
defaults. Other option pointers are required unless documented otherwise.

Model GLB import produces its animation report and sound-source lookup paths
only while importing Animations. Requesting either output requires non-null
`out_animations`.

## Binary Utilities

### Text Encoding

`pistoris::binary::classifyTextEncoding` classifies counted bytes as ASCII,
UTF-8, or ISO-8859-1 (Latin-1). Empty input and input containing only bytes in
`0x00-0x7f` are ASCII. Other strictly valid UTF-8 is UTF-8; every remaining
byte sequence is Latin-1. A Latin-1 sequence can also be valid UTF-8, so UTF-8
wins that ambiguous case. Use an explicit native text mode when the source
encoding is known.

`latin1ToUtf8` accepts every byte sequence. `utf8ToLatin1` returns
`ARX_TEXT_INVALID_UTF8` for malformed UTF-8 and `ARX_TEXT_NOT_LATIN1` when a
valid code point is above `U+00FF`. Both preserve embedded NUL bytes and change
the output string only on success. They perform no path normalization, case
conversion, Unicode normalization, or byte-order-mark removal.

The C API exposes the same operations through
`arx_pistoris_binary_classify_text_encoding`,
`arx_pistoris_binary_latin1_to_utf8`, and
`arx_pistoris_binary_utf8_to_latin1`. Conversion output is caller-owned,
NUL-terminated, accompanied by its byte length excluding that terminator, and
freed with `arx_pistoris_free_string`. The length remains authoritative when
the result contains embedded NUL bytes.

### Encoded Media

`pistoris::binary::validateEncodedAudio` and
`arx_pistoris_binary_validate_encoded_audio` validate complete encoded WAV,
MP3, or Ogg Vorbis data without retaining it. Empty, malformed, unrecognized,
and unsupported encoded data returns `ARX_AUDIO_BAD_DATA`. Supported codecs
with unsupported channel counts return `ARX_AUDIO_UNSUPPORTED_CHANNELS`;
decoded data beyond the safety cap returns `ARX_AUDIO_TOO_LARGE`.
`inspectEncodedAudio` and its C counterpart perform the same validation and
return the detected format, channel count, sample rate, and frame count.

`pistoris::binary::validateEncodedImage` and
`arx_pistoris_binary_validate_encoded_image` validate complete encoded PNG,
JPEG, BMP, or TGA data. Empty, malformed, and unrecognized data returns
`ARX_IMAGE_BAD_DATA`. `inspectEncodedImage` and its C counterpart perform the
same validation and return the detected format, dimensions, and component
count. Image width and height are each limited to 8192 pixels.

## Results and Return Codes

The C++ editing and conversion API returns `Result<T, Location>`. Public aliases
name the relevant location domain, including `LevelResult<T>`, `ModelResult<T>`,
`AnimationResult<T>`, `AmbianceResult<T>`, `CinematicResult<T>`,
`GlbResult<T>`, `ObjResult<T>`, and the native-format results such as
`FtlResult<T>` and `TeaResult<T>`. Native byte readers use distinct aliases
such as `FtlBinaryResult<T>` and `TeaBinaryResult<T>`; carrier validation and
writing use `FtlResult<T>` and `TeaResult<T>`. `Result<void, Location>`
represents a fallible operation without a value.

Test a result before reading it. `code()` returns `ARX_OK` on success or the
stable `ArxReturnCode` category on failure. `error()` then exposes the same
code, an optional typed location, and optional human-readable detail. Locations
identify the relevant resource element, semantic native-format element, GLB
object, or OBJ source line. Editing and conversion failures use the narrowest
stable location available; exception translation and violated internal
invariants may be unlocated. Error data is immutable. Detail text is diagnostic,
not a machine-readable contract. Code constructing a failed generic `Result`
must pass either a location or `std::nullopt` explicitly.

Location indices are zero-based. `kNoInputIndex` and `kNoElementIndex` mark
index fields that do not apply or cannot be identified. `ResourceLocation`
describes an editing resource: `resource_path` identifies the owning or
referenced resource when known, `input_index` identifies an entry in a composite
input array, `element` selects the kind of data, and `index`/`subindex` identify
that element and an optional nested item. `label` preserves a useful authored
name when one is available. Empty identity strings mean that the caller did not
supply that identity; they do not replace the numeric location.

`CinematicLocation` uses `sound_handle` for sound and sound-encoding failures,
and `language_id` when a language is known. Their unavailable values are
`kNoSoundHandle` and `kInvalidLanguageId`. Use the `SoundHandle` helpers to
inspect a reported handle; do not reinterpret its representation. Other
Cinematic elements use the ordinary `index` and `subindex` fields.

`NativeLocation` uses `element`, `index`, and `subindex` for the corresponding
native record and nested item; `field` names the relevant structure member
when one can be identified. `NativeBinaryLocation` additionally identifies the
stored or decoded byte region, byte offset, and requested byte count. A
`byte_offset` of `kNoElementIndex` means that the failure was identified after
the bytes were read, such as during canonicalization or validation. A DLF binary
location can name either a DLF record or its embedded LLF record.

`GlbLocation` locates the relevant GLB element and may further provide its
authored `label` and failing `property`.
For a primitive, `index` selects the mesh and `subindex` selects its primitive.
For an animation channel or sampler, `index` selects the animation and
`subindex` selects the nested channel or sampler. Other GLB elements use
`index` for their top-level array and leave `subindex` unset unless documented
by that operation.
`ObjLocation::line` is one-based; zero means that no source line applies.
`source_index` selects a material-library input and is `kNoInputIndex` for the
OBJ document itself. Results whose work spans multiple resource kinds expose a
`std::variant` location; inspect its active alternative before interpreting the
indices.

Within Pistoris operations, the site that originates a typed failure emits one
`ARX_LOG_DEBUG` source trace after recording the available code, operation,
typed location, and detail. Constructing a generic `Result::failure` directly is
silent, as are propagation, remapping, and resource-identity enrichment.
Result-returning operations do not emit user-facing error logs: the caller
decides whether a returned failure is fatal, recoverable, or ignored. Use
`describeError(*result.error())` for the stable code, typed location, and
diagnostic detail in one human-readable string. If storing the complete error
itself fails, Pistoris emits a minimal debug trace and returns an allocation or
internal failure without a location. Low-level utilities that return only
`ArxReturnCode` do not expose typed location data.
The typed `errorElementName(location.element)` overloads return the same
readable element labels exposed by `ArxErrorLocation::element_name` in the C
API.

C++ editing and conversion entry points are `noexcept`: allocation failure
returns `ARX_BAD_ALLOC`, while an unexpected internal exception returns
`ARX_INTERNAL_ERROR` and emits one debug source trace. Constructors and copying follow
normal C++ allocation behavior and may throw. Move construction and move
assignment of the five editing classes are `noexcept`.

```cpp
const pistoris::LevelResult<void> result = level.validate();
if (!result) {
  std::fprintf(stderr, "%s\n", pistoris::describeError(*result.error()).c_str());
  if (result.error()->location()) {
    const pistoris::LevelLocation& location = *result.error()->location();
    // Inspect location.element, location.index, and location.label as needed.
  }
}
```

Dereference a successful value result or move its value into caller-owned
storage. Optional report and source-path output pointers are assigned only when
the operation succeeds.

```cpp
auto imported = pistoris::Level::importGlb(glb_bytes);
if (!imported) {
  return imported.code();
}
pistoris::Level level = std::move(*imported);
```

The C ABI returns `ArxReturnCode`; `ARX_OK` is zero. Fallible resource,
conversion, and native-format functions accept an optional trailing
`ArxError*`. Initialize caller-owned storage with `ARX_ERROR_INIT`. On failure,
the snapshot repeats the code and, when available, identifies the location,
element name, indices, authored identity, binary field, JSON pointer, or source
line. On success, the same object is cleared. Its string views remain valid
until the next operation receiving that object or `arx_pistoris_error_clear`.
Do not copy an initialized `ArxError`; clear it before its lifetime ends.
Passing `NULL` requests only the return code.

Inspect location fields only when `location.kind` is not
`ARX_ERROR_LOCATION_NONE`. `element` is a stable `ArxErrorElement`; use its
symbolic values rather than assuming that it matches a C++ enum ordinal.
`input_index`, `index`, `subindex`, and `byte_offset` are zero-based and use
`SIZE_MAX` when unavailable. Cinematic sound and language locations use
`sound_handle` and `language_id`; their unavailable values are
`ARX_NO_SOUND_HANDLE` and `ARX_INVALID_LANGUAGE_ID`. OBJ source lines are
one-based and use zero when unavailable. `requested_bytes` is nonzero only when
a binary read could not consume the requested field. A JSON pointer with
`{NULL, 0}` is unavailable; a non-null view of length zero identifies the
document root.

```c
ArxError error = ARX_ERROR_INIT;
ArxReturnCode rc = arx_pistoris_level_validate(level, &error);
if (rc != ARX_OK) {
  fprintf(stderr, "%s\n", arx_pistoris_strerror(rc));
  if (error.detail.size != 0) {
    fwrite(error.detail.data, 1, error.detail.size, stderr);
    fputc('\n', stderr);
  }
  if (error.location.kind != ARX_ERROR_LOCATION_NONE) {
    fputs("location: ", stderr);
    if (error.location.element_name.size != 0) {
      fwrite(error.location.element_name.data, 1,
             error.location.element_name.size, stderr);
    } else {
      fprintf(stderr, "element=%d", (int)error.location.element);
    }
    if (error.location.index != SIZE_MAX) {
      fprintf(stderr, " index=%zu", error.location.index);
    }
    if (error.location.field.size != 0) {
      fputs(" field=", stderr);
      fwrite(error.location.field.data, 1, error.location.field.size, stderr);
    }
    if (error.location.label.size != 0) {
      fputs(" label=", stderr);
      fwrite(error.location.label.data, 1, error.location.label.size, stderr);
    }
    fputc('\n', stderr);
    if (error.location.resource_path.size != 0) {
      fprintf(stderr, "resource: ");
      fwrite(error.location.resource_path.data, 1,
             error.location.resource_path.size, stderr);
      fputc('\n', stderr);
    }
  }
}
arx_pistoris_error_clear(&error);
```

### Editing Object Lifetime

`Level`, `Model`, `Animation`, `Ambiance`, and `Cinematic` are deep-copyable and
have `noexcept` move operations. A moved-from object remains destructible,
assignable, and resettable but is disengaged: read-only inspection returns
empty or neutral values, clear operations are no-ops, and fallible operations
return `ARX_INVALID_STATE`. Copying a disengaged object preserves that state.
Call `reset()` to create fresh empty state before reusing it.

Public code ranges are grouped by concern:

| Range | Concern |
| ---: | --- |
| `1-99` | common preconditions |
| `100-899` | runtime, storage, compression, and internal failures |
| `900-999` | binary data validation and text conversion |
| `1000-1999` | native formats |
| `2000-2999` | Level |
| `3000-3999` | Model |
| `4000-4999` | Animation |
| `5000-5999` | Ambiance |
| `6000-6999` | Cinematic |
| `10000+` | external formats |

Use symbolic values rather than depending on a numeric assignment in pre-1.0
code.

## Native Formats

The C++ API exposes native carriers as value types:

```cpp
#include "arx_pistoris/native.hpp"

#include <cstdint>
#include <vector>

std::vector<std::uint8_t> source = /* caller-owned file bytes */;

auto fts = pistoris::readFts(source);
if (!fts) {
  return fts.code();
}

auto compressed = pistoris::writeFts(*fts);
auto raw = pistoris::writeFts(*fts, false);
```

`readFtl`, `readFts`, `readDlf`, and `readLlf` accept raw and supported PKWARE
DCL-compressed data. Their writers compress by default. TEA, AMB, and CIN are
always raw. `readCin` accepts versions 1.75 and 1.76; `writeCin` emits canonical
1.76 data. `readAmb` accepts exact AMB versions 1.000 through 1.003 and maps
them to one carrier representation; unused track names, padding, and flag bits are
discarded. Inactive pan or position settings and interval or flags unused by
constant settings are stored as zero. AMB validation requires this form.
`writeAmb` emits version 1.001 with only game-observable flag bits.
`readDlf` can return embedded lighting separately.

FTS binary carriers do not contain a semantic level number. The
arx-convert-compatible JSON adapter exposes that required schema value
separately: `toFtsJson` accepts a level number, while `fromFtsJson` returns
`FtsJsonImport` containing both `fts` and `level`. The C API mirrors this with
the `level` and `out_level` arguments of `arx_pistoris_fts_to_json` and
`arx_pistoris_fts_from_json`.

Native carrier resource references store the field-local value produced by the
engine's normalization, not the exact serialized spelling or the complete path
later assembled by a loader. Readers lowercase ASCII, use `/` separators,
resolve `.` and `..`, and remove a lookup suffix where the engine does. Writers
require this canonical carrier form and reconstruct wire-only spelling such as
protective trailing dots. Carrier fields do not absorb prefixes, filenames, or
extensions added later during lookup. When Pistoris exposes construction of a
complete logical resource path, that operation belongs to `pistoris::paths`.
For example, a DLF carrier stores the normalized scene directory while
`ftsFromDlfScene` resolves that directory against `game/` and appends
`fast.fts`. A leading `..` may consume `game/`; paths that would remain above
the resource root are invalid.

`Level`, `Model`, `Animation`, `Ambiance`, and `Cinematic` store semantic text
as UTF-8. Native import uses the binary text classification above: ASCII and
UTF-8 are copied, while other bytes decode as ISO-8859-1 (Latin-1). Pass
`NativeTextMode::kUtf8` or `kLatin1` to resolve ambiguous input explicitly;
forced UTF-8 rejects invalid sequences. Source paths returned by import are
canonical UTF-8 lookup paths. Native baking uses UTF-8 for `kAuto` and `kUtf8`;
`kLatin1` rejects characters it cannot encode. The selected mode controls the
native carrier regardless of whether sidecars are requested; sidecar paths
remain UTF-8. Latin-1 is a legacy compatibility mode: ASCII references are
portable, but resolution of non-ASCII references depends on the target
runtime, resource provider, and filesystem normalization. Pistoris guarantees
the encoding, not that lookup environment. Native conversion preserves
semantic content, not arbitrary serialized bytes.

The C API exposes the same policy through `ArxNativeTextMode` import arguments
and native bake options.

The C ABI exposes opaque `ArxAmb`, `ArxCin`, `ArxFtl`, `ArxTea`, `ArxFts`,
`ArxDlf`, and `ArxLlf` types through pointers:

```c
#include "arx_pistoris/native.h"

ArxFts* fts = NULL;
ArxError error = ARX_ERROR_INIT;
ArxReturnCode rc = arx_pistoris_fts_read(data, size, &fts, &error);
if (rc != ARX_OK) {
  arx_pistoris_error_clear(&error);
  return rc;
}

uint8_t* encoded = NULL;
size_t encoded_size = 0;
rc = arx_pistoris_fts_write(fts, 1, &encoded, &encoded_size, &error);
if (rc == ARX_OK) {
  /* consume encoded */
  arx_pistoris_free_bytes(encoded);
}
arx_pistoris_fts_destroy(fts);
arx_pistoris_error_clear(&error);
```

For DLF, `arx_pistoris_dlf_read` returns embedded LLF through a separate
output handle; that handle is `NULL` when no valid embedded lighting exists.
DLF and LLF write options accept optional printable-ASCII signer text. Native
writers truncate the resulting metadata to the native field capacity.

Native carriers expose binary I/O and validation through `native.hpp` and
`native.h`. FTL, TEA, FTS, DLF, LLF, and AMB also expose compatible JSON I/O;
CIN does not. The umbrella headers re-export those declarations. OBJ and GLB
conversion use the coherent Model surface; GLB accepts and returns Animation
sidecars.

The C++ JSON functions name their carrier format explicitly: `toAmbJson` and
`fromAmbJson`, `toDlfJson` and `fromDlfJson`, and likewise for FTL, FTS, LLF,
and TEA. The C and Python APIs provide the same operations under their existing
format-specific function names and modules.

Native writers serialize canonical carrier resource paths. Writing succeeds
with a warning when a reference resolves outside Libertatis' default loose
resource roots (`editor`, `game`, `graph`, `localisation`, `misc`, `sfx`, and
`speech`), because a loose installation may not discover it. Archive-backed
resources may still make the reference available.

JSON is an arx-convert compatibility serialization of native carriers. It is
not a separate Pistoris intermediate and is not promised to preserve data that
the compatibility schema cannot represent. This includes AMB JSON through the
same native carrier API used by FTL, TEA, FTS, DLF, and LLF.

FTS `uniqueHeaders` records contain legacy source checks rather than runtime
Level data. Binary readers validate their count and framing before discarding
the records. When `uniqueHeaders` is present in JSON input, readers validate
its field types and sizes before discarding it. Binary writers emit no source
checks, and JSON writers emit an empty `uniqueHeaders` array.

JSON text is always UTF-8. A JSON conversion's `NativeTextMode` controls only
decoding text from the source carrier or encoding text into the destination
carrier. LLF JSON has no native text fields and therefore does not accept a
text mode.

## Textures and Native Sidecars

Level, Model, and Cinematic use the shared `ArxTextureView` value. Its path,
optional encoded image, and optional external-image extension are copied by
editing calls. Views returned by copy operations borrow all three fields from
the owning object.

`pistoris::paths::textureDirectory()` and
`arx_pistoris_path_texture_directory` expose the canonical game texture
resource directory as a borrowed view.

The path is a logical game resource identity, not a physical image filename.
Stored identities are lowercase, use `/` separators, and omit the physical
image-format suffix. A dot supplied through the editing API remains part of
the identity. For example,
`textures/item.pie` with a PNG image is emitted as the physical sidecar
`textures/item.pie.png`. Identities are unique within their owner under
case-insensitive path comparison. Spaces, square brackets, parentheses, and
ampersands are preserved inside texture path components. Repeated underscores
are valid and preserved in logical texture identities.
Editing operations normalize portable spelling and resolve collisions. Input
that cannot form a relative resource path is rejected rather than assigned a
placeholder identity.

`external_image_extension` stores the lowercase physical suffix for a path-only
texture and includes its leading dot, such as `.jpg`. It must be empty when
encoded image bytes are present. Attaching bytes clears the hint; clearing
bytes retains the detected image format as the hint.

Native import supplies the canonical lookup identity, but no physical suffix
hint or image bytes. The native format does not preserve which file extension
won the engine's lookup. Core import does not search for or read referenced
image files. A caller can attach encoded image data with `setTextureImage`
before baking. `setTexturePath` and `setTextureExternalImageExtension` update
those fields without copying the encoded image payload.

Model and Level import overloads can also return texture source paths. The
returned vector has one entry per texture in `TextureIndex` order. Native
entries are canonical engine lookup paths. External image references retain
their authored format path; embedded GLB images and material-name fallbacks use
an empty entry. When several format paths resolve to one texture, the first path
is retained. These paths are caller-owned file-resolution inputs and are not
retained by Model or Level.

The Level, Model, and Cinematic `rebaseTexturePaths` methods move every identity
under one logical resource directory, keep only each basename, normalize it as
a game texture path, and resolve collisions by appending `_N` after the full
identity. An empty directory keeps only basenames; a trailing separator is
accepted. Invalid relative path structure rejects the atomic operation.

`NativeModelBakeOptions::include_texture_files=false` and
`Level::NativeBakeOptions::include_texture_files=false` retain native references
but omit encoded image sidecars. Cinematic controls the same behavior through
`NativeCinematicBakeOptions::include_illustration_files`.

`NativeTextureFile` reports the source texture index, relative sidecar path,
and encoded bytes. Level, Model, and Cinematic bundles own these values. The C
ABI exposes the same collection through `ArxNativeTextureFiles`; returned file
views remain valid until that handle is destroyed. Import lookup paths are
exposed through the owned `ArxTextureSourcePaths` handle. Its position is the
texture index, and returned string views remain valid until that handle is
destroyed.

Native FTS, FTL, and CIN writers use a protective trailing `.` when serializing
dotted identities so the engine does not interpret a meaningful inner dot as
an image suffix. FTS baking may shard one Level texture across several native
aliases when room rendering limits require it.

GLB external image URIs append the preserved extension to the full logical
identity. When neither bytes nor an extension hint are available, GLB export
assumes `.png` and warns. Embedded GLB images always use the format detected
from their bytes.

## Sound Identities

Animation and Ambiance expose zero-based `SoundIndex` values from their effect
sound collection. Cinematic supports separate effect and speech collections,
so it uses `SoundHandle` as the public reference carried by keyframes. A handle
combines `SoundKind` with a per-kind `SoundIndex`; use `soundHandle`,
`soundHandleKind`, and `soundHandleIndex` rather than inspecting its bits.
`SoundHandle` invalidates together with `SoundIndex` when a sound collection is
edited.

Animation and Ambiance `setSoundPath` update a sound identity without copying
its encoded audio. Cinematic exposes the equivalent operation by
`SoundHandle`.

`LanguageId` identifies a registered speech language. Value `kSoundEffects`
(`ARX_SOUND_EFFECTS_LANGUAGE_ID`) is reserved for effect encodings. Effect
sounds can have one encoding under that value; speech sounds can have one
encoding per registered nonzero language. Logical sound identities do not
require encoded audio, and speech paths can exist before any language is
registered. Language names are stored as lowercase portable identifiers and
must be unique by case-insensitive path identity. Editing and lookup accept
any ASCII letter case.

## Level

`pistoris::Level` is the coherent public editing surface for levels. It owns an
optional DLF resource identity, geometry, textures, room topology, navigation,
lights, player spawn, entities, fogs, zones, spline paths, minimap, and loading
screen. It keeps these data coherent as they are edited.

`setResourcePath` accepts a Level selector or any portable logical `.dlf` path.
A selector expands to its registered game path; a path is normalized to
lowercase with `/` separators without imposing the registered Level layout.
Passing an empty path clears the identity. Native and GLB imports remain
anonymous because byte conversion has no filesystem context.

### Import and Export

```cpp
auto imported = pistoris::Level::importNative(fts, &llf, &dlf);
if (!imported) {
  return imported.code();
}
pistoris::Level level = std::move(*imported);

pistoris::Level::GlbExportOptions glb_options;
glb_options.arx_units_per_glb_unit = 100.0f;
auto glb = level.exportGlb(glb_options);
if (!glb) {
  return glb.code();
}
```

Level GLB export can attach static Model previews to matching entities:

```cpp
const std::array<const pistoris::Model*, 2> previews = {&human, &spider};
ArxLevelModelPreviewReport preview_report;
auto glb = level.exportGlb(previews, glb_options, &preview_report);
```

Each non-anonymous Model is mapped from its FTL resource identity to an entity
class path. Duplicate identities keep the first Model. Invalid, anonymous,
unmappable, and duplicate Models are skipped and counted in the report.
Every entity instance with the same matching class path shares one preview
mesh. The Models are borrowed only for the duration of the call.

`importNative` requires FTS. LLF and DLF pointers are optional. `importGlb` builds
Level directly from a GLB byte span. GLB import and export own DCC coordinate
conversion; Level itself remains in native Arx coordinates with -Y up.
Level GLB can preserve a complete, nonempty room-distance collection when
every Level room has exported geometry. It can also preserve anchor
connections. Both use opaque, best-effort round-trip data. Room distances are
restored or discarded as a complete set when the stored room or portal
structure no longer matches. Import never runs generation implicitly. The
editing API nevertheless exposes one value for every unordered pair of
distinct rooms. An unavailable pair has distance `-1` and invalid portal
indices.
The exact authored hierarchy is documented in the
[Level authoring reference](authoring/LEVEL_REFERENCE.md).

`bakeNativeBundle` returns FTS, LLF, DLF, and encoded texture sidecars in
memory. `bakeDlf` rebuilds only scene data. The caller decides where any
returned resource is stored.

### Minimap and Loading Screen

The optional minimap stores an encoded PNG, JPEG, BMP, or TGA image and the
world X/Z rectangle covered by that image. `setMinimap` accepts those values
directly. `setMinimapFromProjection` derives the rectangle from the image,
current Level geometry, and an Arx-unit projection offset. Minimap projection
uses 25 Arx units per image pixel. Image setters require non-empty, valid data;
use the corresponding clear function to remove an image.

`renderMinimap` projects the stored rectangle from the referenced Level bounds
and returns both the effective Arx-unit projection offset and the encoded
image. Plain mode derives a tight projection without leading padding when no
offset is supplied, or uses an explicit offset when one is supplied. Game mode
requires an explicit offset and overwrites the final one-pixel perimeter with
the requested border color; an omitted border color selects white. A border is
invalid in plain mode. Output may be PNG, BMP, or TGA; JPEG is accepted as
input but is not an output format. Projection-based setting and rendering
require referenced Level geometry.

`generateMinimap` stores a `640 x 640` PNG sampled from the full `0..16000`
Level X/Z domain at 25 Arx units per pixel. The default palette uses dark blue
foreground, lighter blue background, light brown water, cyan lava, and a
five-pixel white halo. The options overload can replace those colors, provide
sampler images, or change the halo. Rendering projects the image from the
top-left anchor at the minimum referenced X and maximum referenced Z, then
applies the requested offset. This can crop or pad the rendered image, so its
dimensions can differ from the stored image. Each sampler uses its color
directly when its image is empty; otherwise its image is stretched to
`640 x 640` and multiplied by that color. Sampler alpha is ignored and the
output is opaque. Downward-facing surfaces and slopes above 85 degrees are
ignored. The highest remaining surface determines whether the pixel is
foreground, water, or lava; the lower face index wins an exact-height tie. The
halo uses square pixel-distance rings around all occupied pixels; radius zero
disables it. Failed generation leaves the stored minimap unchanged.

The optional loading screen stores an encoded PNG, JPEG, BMP, or TGA image.
`renderLoadingScreen` can preserve the source dimensions, produce the normal
`320 x 390` layout, or produce the `640 x 480` fullscreen layout. Output may be
PNG, BMP, or TGA.

The focused `level/images` API performs the same output operations on detached
encoded images. It can reproject a minimap between two Arx-unit projection
offsets, optionally with the game-layout one-pixel border, or render a loading
screen at original, normal, or fullscreen dimensions without a `Level`
instance. `projectionOffsetFromMiniOffset` and
`miniOffsetFromProjectionOffset` convert the values used by `mini_offsets.ini`.
`projectionOffsetForLevel` additionally applies the offsets the engine uses for
levels with built-in overrides. The C API exposes equivalent functions.

Level GLB preserves a valid minimap image and placement. Invalid or ambiguous
minimap payloads are omitted with a warning. Loading screens have no Level GLB
representation. Native baking does not include either image sidecar; callers
render them and choose destinations with
`paths::levelMinimap`, `paths::levelLoadingScreen`, and
`paths::minimapResourceLevel`.

### Reading Collections

The C++ API exposes named read-only random-access views. Values with only scalar
or fixed-size fields are independent copies; string and encoded-media members
borrow from Level. Nested collections return a result because the parent index
is checked.

```cpp
for (const ArxLevelRoom room : level.rooms()) {
  // Inspect room.
}

auto perimeter = level.zonePerimeter(zone);
if (!perimeter) {
  return perimeter.code();
}
```

The C ABI uses count-and-copy access:

```c
size_t room_count = 0;
ArxReturnCode rc = arx_pistoris_level_room_count(level, &room_count, NULL);
if (rc != ARX_OK) {
  return rc;
}

ArxLevelRoom* rooms = malloc(room_count * sizeof(*rooms));
if (room_count != 0 && rooms == NULL) {
  return ARX_BAD_ALLOC;
}
if (room_count != 0) {
  rc = arx_pistoris_level_copy_rooms(level, 0, room_count, rooms, NULL);
}
free(rooms);
```

Every non-const Level operation invalidates collection views, their iterators,
borrowed members, and collection indices.

Indices are zero-based positions in the current collection, not persistent
object identities. An add operation returns an index valid for the resulting
Level state. Do not retain indices across another editing operation.

### Editing and Generation

Level and Model expose per-collection geometry replacement through
`replaceVertices` and `replaceFaces`. C++ accepts borrowed scalar spans; C uses
pointer/count pairs, with parallel face arrays grouped in `ArxLevelFacesInput`
or `ArxModelFacesInput`. Counts are scalar counts. Buffers are borrowed only
until the call returns; failures leave existing state unchanged.

| Input | Scalar layout |
| --- | --- |
| Vertex positions | float `3V`, xyz triples |
| Face vertex indices | uint32 `3F`, triangle indices |
| Corner UVs | float `6F`, uv pairs |
| Corner normals | float `9F`, xyz triples |
| Texture indices | uint32 `F`, existing texture or `ARX_NO_TEXTURE` |
| Transvals | float `F` |
| Face normals | float `3F`, or empty to derive |
| Face flags | uint32 `F`, or empty for zero |
| Level corner colors | float `9F`, or one RGB triplet broadcast to all corners |

Bulk replacement normalizes supplied finite normals of length greater than
`1e-4`. Validation requires unit face and corner normals within `1e-4` and does
not modify them. Stored face normals remain authorable independently of corner
normals and vertex positions.

Replacement creates new identities even when values are equal. Replacing or
clearing vertices discards faces and Model vertex bone/selection links.
Replacing or clearing Level faces discards navigation surfaces, anchors, and
anchor connections. Bulk-replaced Level faces start with `ARX_NO_ROOM`; assign rooms
separately with `replaceFaceRooms`. This sentinel is valid during full
validation. Clearing rooms preserves faces as unassigned and removes portals
and distances. Clearing textures preserves faces and resets their texture
links. Texture and room affiliation replacements preserve face identity.

Selections, bones, action points, textures, rooms, and portals use individual
insertion and clearing. Model selection affiliations have separate uint64 mask
replacements for vertices, bones, and action points. Each mask may contain only
occupied selection bits. Bone affiliations take one uint32 index per vertex or
action point, with `ARX_INVALID_INDEX` meaning unassigned.

Python exposes the same raw collections with destination-only `copy(...)`
methods. Requested outputs are writable, aligned, native-endian C-contiguous
buffers of the matching scalar type and count; flat arrays and semantic-shaped
arrays are both accepted. Each call preflights every output before writing,
rejects overlapping destinations, and leaves resource state and live
references unchanged. Pass `None` to skip an output, and request at least one
output. NumPy is optional; `array.array` and `memoryview` also work. See the
[Python buffer reference](../bindings/python/README.md#bulk-geometry-buffers)
for the complete collection and field list.

Every public face exposes a color for each corner. When no lighting has been
authored, those colors are neutral gray (`0.5, 0.5, 0.5`); this does not expose
whether the Level uses compact default storage internally. `resetCornerColors`
restores all corners to that default.
Room- or portal-topology changes reset room distances to their unavailable
sentinels; ordinary geometry edits do not. Clearing room distances has the
same semantic effect. Setting one pair leaves the values of all other pairs
unchanged.
Explicit compaction removes unreferenced vertices or textures.

Convenience operations include:

- room-aware vertex welding
- portal-quad flattening
- conservative geometry snapping to portal surfaces
- navigation-surface generation or floor filtering
- navigation-island pruning
- anchor generation, connection generation, and island pruning
- room-distance generation
- static corner-lighting generation

These operations are explicit. Import does not silently regenerate authored
navigation or connectivity.

Portal flattening preserves the plane-defining quad vertices `0`, `1`, and
`3`, then projects vertex `2` orthogonally onto that plane. Triangles and
already-planar quads are unchanged. The operation validates every projected
portal and its Level bounds before publishing any changes. Changing any quad
discards room distances; performing a no-op flatten preserves them.

Portal snapping moves nearby geometry vertices onto the bounded portal surface.
A vertex is eligible only when every incident face belongs to one of the
portal's two rooms. Vertices shared with unrelated rooms, ambiguous matches,
and moves that would collapse or reverse a face remain unchanged. The radius
must be a positive finite 3D distance in Arx units and defaults to `1`.

Portal snapping preserves all other Level data, including room distances,
navigation, anchors, corner lighting, and the minimap. Regenerate any derived
data made stale by the geometry edit explicitly.

Call `validate()` to validate the complete Level, or use a focused validator
when only one area needs checking.

### Coordinates and Bounds

Level uses native Arx coordinates and -Y up. Geometry and active portal X/Z
coordinates must be inside inclusive `[0,16000]`. Navigation support remains
finite-only.

GLB conversion accepts a finite Arx-units-per-GLB-unit ratio in inclusive
`[1,1000]`, defaulting to `100`. Import can choose a 100-unit-aligned X/Z
offset automatically; `ArxLevelGlbImportInfo` reports the applied value.

### C ABI

`ArxLevel` is an opaque handle. The C ABI mirrors the supported C++ Level
capabilities with creation, cloning, import, export, validation, collection
copying, editing, generation, and native baking functions.

```c
ArxLevel* level = NULL;
ArxError error = ARX_ERROR_INIT;
ArxReturnCode rc =
    arx_pistoris_level_import_native(
        fts, llf, dlf, &level, NULL, ARX_NATIVE_TEXT_AUTO, &error);
if (rc != ARX_OK) {
  arx_pistoris_error_clear(&error);
  return rc;
}

ArxLevelGlbExportOptions options =
    ARX_LEVEL_GLB_EXPORT_OPTIONS_INIT;
uint8_t* glb = NULL;
size_t glb_size = 0;
rc = arx_pistoris_level_export_glb(
    level, NULL, 0, &options, NULL, &glb, &glb_size, &error);
if (rc == ARX_OK) {
  /* consume glb */
  arx_pistoris_free_bytes(glb);
}
arx_pistoris_level_destroy(level);
arx_pistoris_error_clear(&error);
```

Level native bake option pointers are required.

The Model array accepted by `arx_pistoris_level_export_glb` is borrowed for the
call and mirrors the C++ preview matching and reuse behavior. Pass `NULL, 0`
when previews are not needed. The optional `ArxLevelModelPreviewReport`
receives skip counts.

All C entry points prevent C++ exceptions from crossing the ABI boundary.
Failures are returned as `ArxReturnCode`.
Null opaque input handles return `ARX_INVALID_HANDLE`. Null required data and
output pointers return `ARX_INVALID_DATA_POINTER`.

## Model

`pistoris::Model` is the coherent editing surface for one FTL model. It owns an
optional FTL resource identity and inventory icon, indexed geometry and
textures, an ordered skeleton, action points, and up to 64 generic named
selections.

The inventory icon is optional project data rather than FTL, OBJ, GLB, or JSON
content. `inventoryIcon` exposes the encoded PNG, JPEG, BMP, or TGA image and
its stored inventory footprint. Empty input is rejected; use
`clearInventoryIcon` to remove the icon. `setInventoryIcon` accepts an explicit
one-to-three-slot footprint or an unspecified dimension. In C++, unspecified
dimensions are `std::nullopt`; the C option structs use zero. With neither axis
specified, the footprint is derived from the image, retaining a native
footprint within 3x3 and fitting larger images proportionally to three slots on
their longest axis. With one axis specified, the other follows the source
aspect ratio. The resolved footprint is stored with the unchanged encoded
image.

`renderIcon` returns a detached PNG, BMP, or TGA whose dimensions are exactly
32 pixels per resolved slot, or empty output when the Model has no icon. With
neither render dimension specified, it uses the stored footprint. With one
specified, it derives the other from the source aspect ratio; with both
specified, it uses both. PNG is the default output format. JPEG output is not
supported because inventory-icon rendering can produce transparency.
`InventoryIconLayout` places aspect-preserving content at the center or one of
the four corners, or stretches it to fill the footprint. Center is the default.
Unused pixels are transparent. RGB BMP input applies exact-black color keying
and the engine default morphological antialiasing before either output
encoding. BMP and TGA output retain the projected alpha channel. The stored
image is unchanged.

### Native, OBJ, and GLB Conversion

```cpp
pistoris::FtlResult<pistoris::Model> imported =
    pistoris::Model::importNative(ftl);
if (!imported) {
  return imported.code();
}
pistoris::Model model = std::move(*imported);

if (auto result = model.setResourcePath("model:npc:human_base"); !result) {
  return result.code();
}

auto baked = model.bakeNativeBundle({});
if (!baked) {
  return baked.code();
}

auto obj = model.exportObj("human_base");
if (!obj) {
  return obj.code();
}

auto static_model = pistoris::Model::importObj(obj->text, obj->mtl);
if (!static_model) {
  return static_model.code();
}

auto glb = model.exportGlb();
if (!glb) {
  return glb.code();
}

pistoris::Model::LevelPreviewGlbOptions preview_options;
preview_options.class_path = "model:npc:human_base";
preview_options.asset_name = "human";
auto preview = model.exportLevelPreviewGlb(preview_options);
if (!preview) {
  return preview.code();
}

auto authored = pistoris::Model::importGlb(*glb);
if (!authored) {
  return authored.code();
}
```

`ObjBundle::text` and `ObjBundle::mtl` contain the main OBJ and generated MTL.
By default, `ObjBundle::texture_files` contains one owned sidecar for every
referenced texture whose encoded image is available. Its path is the same
relative path written to `map_Kd`. Set `ObjExportOptions::include_files` to
`false` to omit those copies while retaining the MTL references.

The OBJ export stem names the generated material library. It must be non-empty
and cannot contain whitespace, control characters, or `#`.

OBJ import returns texture source paths for caller-managed file loading.
Exported `ObjBundle::texture_files` provide the matching path and encoded-image
pairs when the source Model contains image data.

`objMaterialLibraryPaths` reads `mtllib` references without accessing files.
After the caller obtains those bytes, the `Model::importObj` overload taking
`ObjMaterialLibraryView` accepts all named MTL documents together. Returned
texture source paths preserve the raw `map_Kd` spelling at the corresponding
Model texture index while Model texture identities are normalized and made
unique.

Native import creates an anonymous Model. `setResourcePath` accepts a Model
selector or any portable logical `.ftl` path. Selectors expand to registered
game paths; direct paths are normalized but retain their directory layout.
Passing an empty path clears the identity. Identity is optional and does not
otherwise change baking.

The native FTL origin becomes Model coordinate zero. Import subtracts the
native origin from geometry, bones, action points, and selection leading
vertices.
Baking emits a synthetic origin at zero. Per-corner Model normals are split
into as many native FTL vertices as required. The independent native face
normal is preserved separately from both corner normals and the normal derived
from vertex positions.

Degenerate native faces are discarded. Native vertices left unreferenced by
retained faces are not represented as geometry vertices. Bone and action-point
names are normalized to ASCII lowercase. Bone names are made unique: the first
normalized spelling keeps the base name, and later collisions receive the
lowest available `_N` suffix. Duplicate action-point names are preserved.

Native FTL selections remain generic Model selections. Names are normalized
to ASCII lowercase and made unique with `_N` suffixes. Membership can include
geometry vertices, bones, action points, and the implicit origin. The first
member of each exact `cut_head`, `cut_torso`, `cut_larm`, `cut_rarm`,
`cut_lleg`, or `cut_rleg` selection is imported separately as that selection's
optional leading vertex. Native vertices that have neither geometry nor
semantic roles are discarded.

Native vertex normals used by retained faces enter Model as normalized corner
normals. Nonfinite, zero, or near-zero values use the generated geometric face
normal. Repairs are reported as warnings.

Finite negative native bone blob-shadow sizes become zero because both values
disable that bone's shadow in the game. Nonfinite sizes fail conversion. Model
editing and GLB authoring require nonnegative sizes.

On native baking, a stored leading vertex is emitted first. A nonempty exact
`cut_*` selection without one infers it from the first geometry, bone,
action-point, or origin member and logs the inferred position. Empty
selections remain valid editing data but are omitted from FTL.

Model GLB is an authoring projection. An optional `arx_model_origin__*` node
defines the local Model origin, ordinary DCC armature binding carries the
ordered skeleton, and custom color attributes and helper nodes carry selection
membership. Detached `arx_bone__*` records carry position-independent bone
metadata. Multiple mesh objects may bind to the same coherent rig. For a rigged
Model, propelled Animation motion uses the shared ancestor of all imported
meshes, root joints, and unbound positional helpers; canonical export uses the
ordinary `skeleton` wrapper. An unrigged Model uses the semantic origin itself
as the motion carrier. With a semantic origin, import rebases represented
positions to it; without one, scene identity is used and propelled Animation
motion is not imported. Import and export accept an
Arx-units-per-GLB-unit ratio from `1` through `1000`; the default is `10`.

An unskinned mesh on or below a joint identified by an imported skin binds
rigidly to its nearest such joint. Other unskinned geometry remains unbound.
Skinned vertices without a positive influence remain unbound rather than using
this hierarchy fallback.

The exact Model and Animation hierarchy is documented in the
[Model and Animation authoring reference](authoring/MODEL_REFERENCE.md).

`exportLevelPreviewGlb` emits only static mesh, materials, and textures inside
a ready-to-place Level entity. Skeletons, selections, action points, and
Animation sidecars are not represented. Its default is `100` Arx units per GLB
unit. The class path is taken from `LevelPreviewGlbOptions::class_path` when
provided. A Model tweak selector resolves to its base Model class. An empty
class path uses the visible `<class-path>` placeholder. The asset name defaults
to `asset` and is normalized for the Level entity grammar.

### Reading and Editing

Model exposes named read-only random-access views. Selection records are
addressed by stable `SelectionId` values and expose a name plus an optional
leading position and bone. Membership uses checked nested views for geometry
vertices, bones, and action points; origin membership is a scalar query.

```cpp
for (const ArxModelBone bone : model.bones()) {
  // Inspect bone.
}

auto members = model.selectionVertices(selection_id);
if (!members) {
  return members.code();
}
for (pistoris::VertexIndex vertex : *members) {
  // Inspect membership.
}
```

View elements are returned by value. String and encoded-image members inside
those values borrow from Model. Every non-const operation invalidates views,
their iterators, borrowed members, and collection indices. Selection IDs remain stable until their
selection is removed. Replacing or resetting Model invalidates all selection
IDs. Removing a selection clears its memberships without changing any other
selection ID.

`updateSelectionMembers` can replace any supplied membership category in one
transaction. `{nullptr,0}` leaves that category unchanged. A non-null pointer
with count zero clears it. Dedicated clear functions are also available.
Object setters preserve existing memberships, while replacing an entire mesh,
skeleton, or action-point collection clears the matching membership category.
Replacing or clearing a skeleton also unbinds every vertex, the origin, every
action point, and every selection leading vertex from the old bones. Their
positions and non-bone selection memberships remain unchanged.

Bone names are unique after ASCII uppercase-to-lowercase normalization. A
nonempty skeleton is an ordered forest: it may have multiple roots, and every
parent has a lower index than its children. Action-point names are normalized
to lowercase but need not be unique: collection order and `ActionPointIndex`
distinguish repeated semantic tags. Names remain open-ended because scripts and
game logic may address custom values.

Selection names are stored in lowercase and may contain ASCII letters, digits,
hyphens, and single interior underscores. They cannot start or end with an
underscore or exceed 63 characters. Names supplied through native or GLB import
and editing operations are normalized to this form. Collisions receive the
lowest available `_N` suffix.

Geometry, textures, bones, action points, the implicit origin, and selections
have focused editing operations. Removing a bone fails while any vertex,
child bone, action point, origin, or selection leading vertex references it.
Removing a selection clears all of its memberships.
Batch vertex insertion validates the complete input before making changes and
returns contiguous indices for the appended vertices.
Level and Model faces are triangles; submitted `QUAD` bits are stripped because
that bit belongs to native FTS polygon encoding rather than either editing
surface.
Explicit compaction removes unreferenced vertices or textures. Vertex welding
merges nearby vertices only when their bone and complete selection membership
match. The default preserves every face by cancelling merges that would make
it degenerate; callers can instead reject the operation or discard those
faces.

`applyReference` requires at least one requested operation. It can copy exact
bone positions, replace bone selection memberships, replace action-point
selection memberships, or combine those operations in one transaction.
Snapping and bone membership copying require identical bone counts and
parent topology. Action-point membership copying does not require matching
skeletons. Bones correspond by index; name differences warn without preventing
the operation. Selections correspond by name. Reference-only selections are not
created, and target-only memberships of a copied category are cleared.
Repeated action-point names correspond by occurrence order. Unmatched target
action-point memberships clear, while selection memberships on unmatched
reference action points are omitted with a warning. The Model changes nothing
on failure.

`inferBoneSelectionMemberships` replaces bone selection memberships using geometry
owned directly by each bone. A bone joins a selection when at least 90% of its
owned vertices belong to that selection. Unbound vertices are ignored, and a
bone without owned geometry receives no memberships. Vertex, action-point,
origin, and leading-vertex memberships are unchanged.

Native baking preserves copied Model-space bone positions exactly relative to
the emitted origin, which supports engines that compare replacement-model bone
positions by exact component equality.

A Model skeleton contains at most 1024 bones. Other Model collection limits
follow the intermediate representation rather than FTL encoding limits. A
coherent Model may temporarily exceed an FTL limit. Native baking validates the
final FTL representation and reports an error until editing or compaction makes
it representable.

`validate()` checks the complete Model for coherence. Focused validators cover
the mesh, skeleton, action points, and selections independently.

`scale`, `rotate`, and `translate` transform all positional Model state in
place. Scale is one positive uniform factor and also scales bone blob-shadow
sizes. Rotation accepts any finite nonzero quaternion and normalizes it before
rotating positions and stored normals. Translation affects positions only.
Each operation validates its complete result before changing Model state.

### C ABI

`ArxModel` is an opaque handle. The `arx_pistoris_model_*` API mirrors the C++
Model surface, including native, OBJ, and GLB conversion, range copies, focused
validation, and editing. OBJ conversion publishes NUL-terminated OBJ and MTL
strings released with `arx_pistoris_free_string`. Owned handles expose declared
MTL paths and exported texture sidecars; their borrowed views remain valid
until the handle is destroyed. `ArxModelGlbImportOptions` and
`ArxModelGlbExportOptions` use the same defaults and limits as the C++ options.
`ArxModelLevelPreviewGlbOptions` and
`arx_pistoris_model_export_level_preview_glb` mirror static Level-preview
export. `ArxModelReferenceOptions` and
`arx_pistoris_model_apply_reference` mirror the C++ reference operation.
`arx_pistoris_model_infer_bone_selection_memberships` exposes the same inference
helper. `ARX_MODEL_REFERENCE_OPTIONS_INIT` selects no operation; enable at
least one field before calling `arx_pistoris_model_apply_reference`.
`ARX_MODEL_INVENTORY_ICON_SET_OPTIONS_INIT` derives both footprint axes.
`ARX_MODEL_INVENTORY_ICON_RENDER_OPTIONS_INIT` selects the stored footprint,
preserves aspect ratio, and centers content. PNG and BMP buffers returned by
the render functions are released with `arx_pistoris_free_bytes`.
All input strings, images, and arrays are copied during calls. All C entry
points prevent C++ exceptions from crossing the ABI boundary.

## Animation

`pistoris::Animation` is the coherent editing surface for one animation. It
owns an authoring name, an optional TEA resource identity, logical Sounds, an
ordered keyframe timeline, and a dense group transform for every group at every
keyframe. Group positions correspond to Model bone indices; names do not bind
Animation data to bones.

`scale` applies one positive uniform factor to root and group translations.
It does not change group scale channels. `rotate` normalizes a finite nonzero
quaternion, rotates translations, and conjugates root and group rotations.
Both operations validate the complete result before changing Animation state.

Frame numbers are strictly increasing. `frameLength()` is the native duration
in 24 Hz frames and is at least the final keyframe number. Each group transform
contains rotation, translation, and multiplicative scale. Keyframes also carry
root translation and rotation, the footstep event, and an optional Sound index.
One Sound can therefore be referenced from multiple keyframes without
duplicating its logical path or encoded audio.

Group quaternion inputs with a negative scalar component are negated before
storage; root quaternion signs are retained.

A group is void when every stored transform is exactly identity and the group
is not claimed. `isGroupVoid` evaluates that effective state; `isGroupClaimed`
reports the explicit claim independently. `claimGroup` and `unclaimGroup`
change ownership intent without changing transforms. `voidGroup` writes exact
identity transforms and removes the claim.

`importNative` creates an anonymous resource identity and takes the authoring
name from TEA. The `arx-pistoris/` producer prefix is removed when present.
`bakeNative` writes the current name under that prefix. Sparse native root
transforms are resolved into the dense editing representation; baking writes
their equivalent values explicitly. Trailing groups that are exact identity
and unclaimed are omitted without changing the Animation. A native group whose
otherwise-neutral timeline uses a negative-identity guard becomes explicitly
claimed. Baking a claimed exact-identity group retains it in the TEA group
count and writes the guard again.

Native sample names enter Animation as
lowercase logical `sfx/*.wav` paths. An optional `SoundSourceReference` vector
reports each distinct decoded, canonical native lookup path for caller-owned
file discovery; case and separator aliases may map to one Sound. GLB import
instead reports the authored format path. Core Animation import does not search
the filesystem.

Animation resource identity is optional. `setResourcePath` accepts an Animation
selector or any portable logical `.tea` path. Selectors expand to registered
game paths; direct paths are normalized but retain their directory layout.

`keyframes()` returns independent keyframe values, while `sounds()` returns
values whose logical paths and encoded-audio members borrow from Animation.
`groupTransforms(keyframe)` returns a checked nested random-access view.
`replaceKeyframes` changes the complete timeline in one transaction.
The first added keyframe establishes the group count; later keyframes must
match it until the timeline is cleared or replaced. An Animation contains at
most 1024 groups. Sound editing, compaction, and rebasing follow the same
ownership and invalidation rules as Ambiance.

`bakeNativeBundle` projects referenced Sounds into TEA sample names and
optional PCM16 WAV sidecars below `sfx/`. A logical path already below `sfx/`
is not prefixed again. Extension changes and collision-safe `_N` suffixes do
not modify Animation. Each Animation is baked independently; the caller owns
output-path conflicts between sidecars from different assets.

Model GLB conversion accepts Animation pointers as sidecars. Transform groups
bind to Model bones by index over their shared prefix. Animations with fewer
groups remain shorter. Canonical GLB helpers mark the remaining Model bones
`VOID` instead of padding the Animation. Export
warns when it discards Animation groups beyond the Model. GLB group helpers
preserve explicit claims and can force sampled groups to exact identity. After
applying that metadata, import removes trailing exact-identity groups that are
not claimed; `CLAIM`
keeps an exact-identity group in the imported Animation's group count. Without
group metadata, import also normalizes transforms within GLB identity
tolerance. Invalid Animations are skipped with warnings;
`ArxAnimationConversionReport` reports converted and skipped counts.
`exportGlbBundle` emits audio as external files with paths relative to the GLB.
Different Animations can request the same sidecar path; the caller decides
which payload to write. `importGlb` can return authored format paths for caller
lookup. Case and separator aliases may map to one Sound while distinct source
spellings remain available for lookup. Unrepairable sound paths skip only the
affected Animation. `importGlbWithAnimations` returns one owning Model and an
owning vector of Animation values.

### C ABI

The C ABI mirrors this through opaque `ArxAnimation` handles and
`arx_pistoris_animation_*`. Owned Sound file and source-reference handles carry
native and Model GLB sidecars. Model GLB import publishes an owning
`ArxAnimationList`. Animations returned by its indexed getter are borrowed and
remain valid until the list is destroyed.

## Cinematic

`pistoris::Cinematic` is the coherent editing surface for one cinematic. It
owns an optional CIN resource identity, an illustration timeline, shared
textures, effect and speech paths, registered languages, and optional encoded
audio per sound and language.

An illustration references one texture and a positive subdivision scale.
Keyframes are ordered by distinct frame numbers, begin at frame zero, and
reference an illustration plus an optional `SoundHandle`. A valid Cinematic
has at least one illustration, at least two keyframes, a positive end frame and
frame rate, and no keyframe beyond the end frame. The final keyframe may precede
the declared end frame. Outgoing speed must be positive except on the final
keyframe.

`importNative` converts CIN bitmap entries into illustrations and merges shared
texture identities. It keeps only sounds referenced by effective keyframes.
Effect and speech paths use independent namespaces; both are stored as
lowercase portable paths without native prefixes or a physical extension.
Optional illustration source paths provide caller lookup inputs. Native
sound source references report each imported handle and normalized logical
path; GLB references preserve each distinct authored spelling. The handle
identifies whether lookup belongs below `sfx/` or a localized `speech/`
directory. Neither result is retained by Cinematic, and the library does not
search for sidecar files.

Native CIN does not preserve a speech language. Imported speech paths therefore
remain valid logical references without registered languages or audio.
Register a language and attach an encoding when a localized sidecar should be
emitted. Keyframes continue to reference one speech handle independently of
the available language encodings.

`bakeNative` emits only canonical CIN carrier data. Illustration and sound paths
exclude wire-only prefixes and protective suffixes; `cin::Sound::speech`
distinguishes speech from effects. `writeCin` reconstructs CIN lookup spelling,
including illustration and speech markers and protective trailing dots.
`bakeNativeBundle` can additionally emit attached illustration and audio
sidecars. Compatible BMP and TGA illustration bytes are retained by default,
except grayscale-alpha TGA is rewritten as RGBA TGA
to preserve transparency in the game. Other supported images are converted to TGA.
`illustration_format` can instead request BMP or TGA for every emitted
illustration, including images already encoded in another format. Attached
audio is converted to PCM16 WAV. Effect files are placed below `sfx/`; speech
files are placed below `speech/<language>/`. Path-only resources remain native
references without emitted files. Only sounds referenced by keyframes enter
the native sound table, which is limited to 256 entries. Native baking checks
renderer grid capacity against attached image dimensions, including the next
illustration in a Dream crossfade; path-only
illustrations can be checked only against the minimum possible grid. The
declared FPS and every nonterminal key's outgoing speed must also produce a
finite timeline duration. CIN stores the multiplier on each key. Arx
Libertatis uses the following frame span and multiplier to calculate one total
duration, then advances uniformly across the complete declared frame range;
different outgoing speeds do not create different local playback rates. The
terminal key's value is retained but does not contribute to the duration.

`importGlb` reads one semantic Cinematic hierarchy. Illustration images must be
embedded; key positions map through each illustration's UV chart, while light
positions map through the key camera's screen. Positive illustration scale
applies to the mesh and its keys; positive key scale affects child lights but
not the camera itself. Off-axis keys are leveled with a warning; child lights
follow the effective rotation. It can return effect or speech source references.
Legacy CIN illustration grid transforms are discarded on import and zeroed on native
baking. Canonical GLB export uses 100 image pixels per GLB unit and has no scale
option; import derives camera depth from the scaled illustration geometry, UV
chart, and vertical field of view. Camera aspect is not stored in Cinematic;
horizontal framing depends on the playback viewport and letterbox mode. GLB
export requires valid encoded image bytes on every referenced illustration
texture because images are embedded in the GLB.
`exportGlb` emits only GLB bytes. `exportGlbBundle` additionally returns
referenced attached audio in its existing WAV, MP3, or Ogg Vorbis encoding.
Effect paths use `<path>.<extension>`; speech paths use
`<path>[<registered-language-name>].<extension>`. Illustration images are
embedded as PNG or JPEG, with other supported formats converted to PNG. Bundle
export rejects effect and localized speech encodings that map to the same
generated relative sidecar path.

C++ collection access uses named read-only random-access views. Texture, sound,
language, and encoding values contain members that borrow storage from
Cinematic. Non-const calls invalidate collection indices, `SoundHandle`
values, views, their iterators, and borrowed members. The C ABI retains
count-and-copy access.
Removing a referenced illustration or sound fails; explicit compaction removes
unused illustration images or sounds and remaps retained references.

### C ABI

The C ABI mirrors the editing and native conversion surface through opaque
`ArxCinematic` and `ArxCin` handles. `ArxCinematicSoundFiles` owns audio
sidecars returned by native baking or GLB export, while
`ArxCinematicSoundSourceReferences` owns import lookup
references. The common `ArxNativeTextureFiles` and `ArxTextureSourcePaths`
handles carry illustration sidecars and source paths. Borrowed views remain
valid until their owning handle is destroyed.

Cinematic has no OBJ authoring projection or compatible JSON representation.
The exact GLB hierarchy is documented in the
[Cinematic Authoring Reference](authoring/CINEMATIC_REFERENCE.md); filesystem
conversion is documented in the [CLI Guide](CLI.md#cinematic-workflows).

## Ambiance

`pistoris::Ambiance` is the coherent editing surface for one ambiance resource.
It owns an optional AMB resource identity, a collection of logical Sounds, and
an ordered collection of tracks. Every track references a Sound index and is
either panned or positioned, so one track cannot contain a mixture of both key
layouts. A Sound owns its logical resource path and optional encoded WAV, MP3,
or Ogg Vorbis bytes.

Each nonempty Ambiance has exactly one master track. Adding the first track
selects it automatically. Removing the master selects track zero when tracks
remain. An empty Ambiance stores zero as its master index, but has no selected
track. A reset Ambiance is blank and becomes valid after adding at least one
valid track.

`trimTracksToMaster` shortens non-master tracks from the end so their longest
nominal duration fits the master's shortest nominal duration. It reduces the
last retained key's play count before removing later keys. The operation
requires attached audio for every participating Sound and commits only when
every track can retain at least one play. The optional output reports the
number of changed tracks.

Native import creates an anonymous Ambiance. `setResourcePath` accepts an
Ambiance selector or any portable logical `.amb` path. A selector expands below
`sfx/ambiance`; a direct path is normalized but may remain anywhere in the
logical resource namespace. Passing an empty path clears the identity. Identity
is optional and does not otherwise change baking.

Automation is represented semantically as constant, step, random step,
interpolated, or random interpolated. Public keys expose a play count rather
than the native `loop_minus_one` encoding. `importNative` and `bakeNative`
perform those mappings and discard only native fields that do not affect game
behavior.

Sound paths are stored as lowercase portable relative paths with `/`
separators. Absolute paths, traversal, and empty components are rejected.
Repairable spelling, including backslashes, uppercase letters, portable device
names, and host-invalid punctuation, is normalized before storage. Resulting
collisions receive extension-aware `_N` suffixes.
`addSound`, `setSound`, encoded-data editing, explicit compaction, and path
rebasing maintain Sound and track coherence. Removing a referenced Sound
fails. Rebasing keeps basenames and resolves collisions with `_N` suffixes. An
empty directory keeps only basenames; a trailing separator is accepted.
Invalid relative path structure rejects the atomic operation.

Native and GLB imports can return `SoundSourceReference` values that map each
normalized Sound index to a caller lookup path. GLB returns the authored path;
native input returns the canonical path used by the engine, which may omit a
lookup suffix. The mapping is not retained by Ambiance. Core Ambiance import
does not search for sound files.

`bakeNativeBundle` emits AMB plus optional sound files. Encoded audio is
decoded and written as PCM16 WAV. Positioned tracks, nonzero panning, and
dynamic panning use mono audio. A stereo Sound needed by both centered panned
and spatial tracks produces separate stereo and mono files with collision-safe
paths. Existing `.wav` paths are reserved before converted outputs are named
and are never renamed. A stereo `.wav` needed only as mono retains its original
file and emits a suffixed mono copy. Path-only Sounds remain path-only
references.

Native baking warns when attached audio shows that a non-master track can
outlast the master. This timing check is advisory and does not prevent output.

`importGlb` and `exportGlb` convert one standalone Ambiance GLB. Both directions
default to 10 Arx units per GLB unit. GLB import creates an anonymous
Ambiance, sorts track and key ordinals, and validates the complete result
before publishing it. The GLB projection exposes tracks, key timing, every
automation mode, and positioned or panned centers through node names and
transforms relative to the Ambiance root.
The exact hierarchy is documented in the
[Ambiance authoring reference](authoring/AMBIANCE_REFERENCE.md).

`exportGlbBundle` returns external sound sidecars with their original encoded
bytes. GLB references the logical paths and does not transcode or embed audio.

GLB export may receive an optional reference Model after its options. Its mesh
is exported at the same scale without authoring metadata or Animation
sidecars. The first `view_attach` action point places the Ambiance root; a
Model without one remains a reference around the Model origin. The reference
mesh is visual context and is ignored by Ambiance import.

Ambiance exposes named read-only random-access views. `tracks()` returns
independent Sound indices, `sounds()` returns values with borrowed path and
encoded-audio members, and `pannedKeys(track)` or `positionedKeys(track)`
returns a checked nested key view. The C ABI retains count-and-copy access.
Input strings, bytes, and key arrays are copied by editing calls. Every
non-const operation invalidates prior views, iterators, borrowed members,
Sound indices, and track indices.
`clearTracks` removes every track and resets the master index while preserving
resource identity and Sounds. `compactSounds` removes Sounds that are no longer
referenced.

### C ABI

The C ABI exposes the equivalent operations through opaque `ArxAmbiance` and
`ArxModel` handles and `arx_pistoris_ambiance_*` functions. Owned
`ArxSoundFiles` and `ArxSoundSourceReferences` handles expose bundle output and
import lookup paths. The nullable reference Model follows the GLB export
options. C entry points catch C++ exceptions and report failures as
`ArxReturnCode`.

Ambiance converts independently to native AMB and standalone GLB. It is not
embedded in Level or Model GLB conversion.

## Buffer Ownership

- Readers, native JSON input functions, and importers do not retain
  caller-owned input storage after returning.
- Successful C byte and string outputs belong to the caller and must be
  released with the matching `arx_pistoris_free_*` function.
- Never release API allocations with `free` or `delete`.
- `ArxNativeTextureFiles` owns its views until
  `arx_pistoris_native_texture_files_destroy`.

## Resource Paths

`pistoris::paths` and the corresponding `arx_pistoris_path_*` C functions build
and parse registered Level, Model, Animation, Cinematic, Ambiance, and texture
resource paths. They operate on portable logical identities, never host
filesystem paths.

Primary resource helpers name their native carrier: `modelFtl`,
`animationTea`, `cinematicCin`, and `ambianceAmb`, with matching `*From*`
parsers. Level exposes `levelDlf`, `levelLlf`, and `levelFts` for its three
native files.

Resource-search helpers describe selector discovery without accessing the
filesystem. `ResourceSearchLocation::base_path` is the logical directory to
search and `max_discovery_depth` bounds recursive discovery. Model and
Animation paths use the closed `ModelPathType` and `AnimationPathType` enums.
`modelPathTypes` and `animationPathTypes` enumerate their supported non-`none`
values; the matching `*PathTypeName` and `*PathTypeFromName` helpers convert
canonical selector tokens. Search helpers accept these enums and return
`false` for an unsupported value. Level, Cinematic, and Ambiance have fixed
search locations. The C API provides the equivalent type enumeration and name
conversion functions.

`animationDirectory` accepts either an Animation path type or an interactive
Model path type. NPC Models map to the NPC Animation directory; other
Animation-bearing Model types map to `fix_inter`. Model types without an
Animation layout are rejected. The C API exposes the two inputs separately as
`arx_pistoris_path_animation_directory` and
`arx_pistoris_path_model_animation_directory`.

`textureDirectory`, `soundDirectory`, `ambianceSoundDirectory`, and
`cinematicIllustrationDirectory` return the canonical
`graph/obj3d/textures`, `sfx`, `sfx/ambiance`, and
`graph/interface/illustrations` resource directories. `normalizeZoneAmbiance`
normalizes the extensionless relative Ambiance name stored by a Level zone and
accepts one final `.amb` suffix.
`ambFromZoneAmbiance` maps that stored name to its canonical AMB path and
rejects the reserved `none` value. The C API exposes equivalent helpers.

A valid resource selector aliases the registered full resource path produced by
its helper. For example, `model:npc:human_base` identifies the same resource as
its registered Model path. Selectors cover only these registered layouts;
editing-class identities may use any normalized portable logical path with the
expected native-format extension. Filesystem lookup belongs to the companion
resource-I/O library or to the caller; the CLI builds its policy on that layer.

Editing-class setters expand selectors before applying the same normalization
used for literal paths. Stored identities are lowercase relative paths with
`/` separators and must satisfy the portable resource-path rules. Resource
paths may contain `__`; identifier and GLB token grammars define their own
restrictions. `#` is normalized to `-` and cannot remain in a stored identity.
Complete validation requires the stored form and never repairs it.

`isPortableResourcePathComponent` and
`arx_pistoris_path_is_portable_resource_path_component` check one case-preserving
component suitable for a mounted resource path. They reject separators and
characters that cannot be used portably. This is broader than
`isPortableFilename`, which applies the stricter generated-filename policy,
and does not require the lowercase form stored by editing classes.
Registered path builders and parsers apply this component policy without
repair and leave output values unchanged when recognition fails.

Model selectors additionally cover `ui-runes`, `ui-menus`, and `editor` FTLs
below `game/graph/interface/book/runes/`, `game/graph/interface/menus/`, and
`game/editor/obj3d/`. These single-file layouts do not accept tweaks or map to
Animation directories.

`entityClassFromFtl` and `ftlFromEntityClass` convert between a
`game/<class>.ftl` resource and its extensionless engine entity class. They
accept only classes recognized by the engine's item, NPC, fix, camera, or
marker classification. `entityClassFromModel` performs the same conversion for
the exact FTL selected by a Model identity, including tweaks.
`baseEntityClassFromModel` instead returns the base Model entity class and
ignores a valid tweak after validating the complete Model identity.
`modelFromEntityClass` succeeds only when the class maps back to one registered
Model layout. A Model path type describes that storage layout; it does not
override the engine's runtime classification of the complete class path.
`entityClassKind` reports the engine classification of a normalized class
path. `itemIconFromEntityClass` returns `<class>[icon]` for item classes, an
empty path for other classes, and rejects malformed class paths.

`resourceSelectorKind` and `arx_pistoris_path_resource_selector_kind`
classify the reserved selector prefix. They do not validate the selector
payload; use the corresponding `*FromSelector` parser for that.

C++ also exposes the owning `ResourceSelector` semantic record.
`parseResourceSelector` validates the complete selector and fills only the
fields used by its `kind`; it leaves the output unchanged on failure.
`resourceSelector` performs the inverse operation and rejects an incoherent
record. Python instead exposes immutable concrete `ModelSelector`,
`AnimationSelector`, `LevelSelector`, `CinematicSelector`, and
`AmbianceSelector` values. `ResourceSelector` is their annotation-only union;
`selector_from_string()` and `selector_from_path()` return a concrete value,
while `str(selector)` and `selector.to_path()` serialize it. The C API keeps
format-specific selector parsers and builders instead of an owning tagged
record.

C string builders use caller-provided buffers. Query the required character
count first, then provide space for that count plus the trailing NUL. Inverse
helpers return views borrowed from the caller-supplied path.

## Diagnostics

The supported Level API excludes its explicitly volatile diagnostic surface.
Diagnostics live under `pistoris::level_debug`. Opt in before including either
debug header:

```cpp
#define ARX_PISTORIS_ENABLE_LEVEL_DEBUG_API
#include "arx_pistoris/debug/level.hpp"
#include "arx_pistoris/debug/level/diagnostics.hpp"
```

This surface may change without compatibility treatment and has no C ABI
counterpart.

## Metadata and Logging

- `version` / `arx_pistoris_version` returns the library version.
- `buildTime` / `arx_pistoris_build_time` returns build metadata.
- `errorString` / `arx_pistoris_strerror` describes a return code.
- `setLogCallback` / `arx_pistoris_set_log_callback` redirects library logs.
- `arx_pistoris_layout_hash` identifies the compiled public C header
  layout for compatible dynamic-loading checks.

## Threading

Library operations are synchronous and do not create threads. Concurrent calls
using one handle require external synchronization, including const validation
calls. Independent handles may be used concurrently.

The log callback is process-wide. Install or replace it only while no library
call is active. Concurrent calls may invoke it concurrently, so the callback
and its user data must provide their own synchronization. The message pointer
is valid only for the callback invocation.
