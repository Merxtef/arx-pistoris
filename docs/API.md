# API Guide

This guide is for C and C++ callers integrating the Pistoris library. CLI
filesystem behavior is documented separately in the [CLI Guide](CLI.md).

Pistoris exposes a C++20 API and a C ABI. Both accept in-memory input and
return carriers, handles, or encoded buffers in memory. They do not open files,
discover sidecars, resolve mounts, create directories, or apply overwrite
policy.

The API is pre-1.0. Source and ABI compatibility are not promised between
minor releases.

## Build Targets and Headers

The current CMake project provides build-tree integration:

| Surface | CMake target | Umbrella header | Library |
| --- | --- | --- | --- |
| C++20 | `arx_pistoris_cpp` | `arx_pistoris/pistoris.hpp` | static |
| C | `arx_pistoris_c` | `arx_pistoris/arx_pistoris.h` | shared |

The C++ target defines `ARX_PISTORIS_CPP_API`, which the umbrella header
requires. The produced C shared library is named `arx_pistoris`.

Focused C++ headers are also available:

```text
arx_pistoris/ambiance.hpp
arx_pistoris/ambiance/bake.hpp
arx_pistoris/animation.hpp
arx_pistoris/animation/bake.hpp
arx_pistoris/binary.hpp
arx_pistoris/cinematic.hpp
arx_pistoris/cinematic/bake.hpp
arx_pistoris/cinematic/glb.hpp
arx_pistoris/cinematic/sound.hpp
arx_pistoris/glb.hpp
arx_pistoris/level.hpp
arx_pistoris/level/bake.hpp
arx_pistoris/level/images.hpp
arx_pistoris/model.hpp
arx_pistoris/model/bake.hpp
arx_pistoris/model/glb.hpp
arx_pistoris/model/obj.hpp
arx_pistoris/native.hpp
arx_pistoris/native/amb.hpp
arx_pistoris/native/cin.hpp
arx_pistoris/native/dlf.hpp
arx_pistoris/native/ftl.hpp
arx_pistoris/native/fts.hpp
arx_pistoris/native/llf.hpp
arx_pistoris/native/tea.hpp
arx_pistoris/paths.hpp
arx_pistoris/runtime.hpp
arx_pistoris/sound.hpp
arx_pistoris/texture.hpp
```

Shared public value types are available without including a resource API:

```text
arx_pistoris/base/flags.h
arx_pistoris/base/audio.h
arx_pistoris/base/image.h
arx_pistoris/base/indices.h
arx_pistoris/base/math.h
arx_pistoris/base/math.hpp
arx_pistoris/base/status.h
arx_pistoris/base/string_view.h
arx_pistoris/runtime/types.h
```

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

## Return Codes

Fallible operations return `ArxReturnCode`. `ARX_OK` is zero. Always check the
return code before using output values.

C++ functions returning `ArxReturnCode` are `noexcept`: allocation failure
returns `ARX_BAD_ALLOC`, while an unexpected internal exception returns
`ARX_INTERNAL_ERROR` and emits one error log. Constructors, copying, assignment,
and value-returning C++ helpers follow normal C++ allocation behavior and may
throw.

```cpp
ArxReturnCode rc = level.validate();
if (rc != ARX_OK) {
  throw std::runtime_error(pistoris::errorString(rc));
}
```

```c
ArxReturnCode rc = arx_pistoris_level_validate(level);
if (rc != ARX_OK) {
  fprintf(stderr, "%s\n", arx_pistoris_strerror(rc));
}
```

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
| `7000-9999` | reserved intermediate formats |
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

pistoris::Fts fts;
ArxReturnCode rc = pistoris::readFts(source, fts);
if (rc != ARX_OK) {
  return rc;
}

std::vector<std::uint8_t> encoded;
rc = pistoris::writeFts(fts, encoded);          // compressed
rc = pistoris::writeFts(fts, encoded, false);   // raw
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

Native carrier resource references use the spelling the game uses for lookup,
not the exact serialized spelling. Readers lowercase ASCII, use `/` separators,
resolve `.` and `..`, and remove a lookup suffix where the engine does. Writers
require this canonical carrier form and reconstruct wire-only spelling such as
protective trailing dots.

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
ArxReturnCode rc = arx_pistoris_fts_read(data, size, &fts);
if (rc != ARX_OK) {
  return rc;
}

uint8_t* encoded = NULL;
size_t encoded_size = 0;
rc = arx_pistoris_fts_write(fts, 1, &encoded, &encoded_size);
if (rc == ARX_OK) {
  /* consume encoded */
  arx_pistoris_free_bytes(encoded);
}
arx_pistoris_fts_destroy(fts);
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

Native writers serialize canonical carrier resource paths. Writing succeeds
with a warning when a reference resolves outside Libertatis' default loose
resource roots (`editor`, `game`, `graph`, `localisation`, `misc`, `sfx`, and
`speech`), because a loose installation may not discover it. Archive-backed
resources may still make the reference available.

JSON is an arx-convert compatibility serialization of native carriers. It is
not a separate Pistoris intermediate and is not promised to preserve data that
the compatibility schema cannot represent. This includes AMB JSON through the
same native carrier API used by FTL, TEA, FTS, DLF, and LLF.
JSON text is always UTF-8. A JSON conversion's `NativeTextMode` controls only
decoding text from the source carrier or encoding text into the destination
carrier.

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
won the engine's lookup. The library does not search for or read referenced
image files. A caller can attach encoded image data with `setTextureImage`
before baking.

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

`LanguageId` identifies a registered speech language. Value `kSoundEffects`
(`ARX_SOUND_EFFECTS_LANGUAGE_ID`) is reserved for effect encodings. Effect
sounds can have one encoding under that value; speech sounds can have one
encoding per registered nonzero language. Logical sound identities do not
require encoded audio, and speech paths can exist before any language is
registered. Language names are portable identifiers and must be unique by
case-insensitive path identity.

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
pistoris::Level level;
ArxReturnCode rc = pistoris::Level::importNative(level, fts, &llf, &dlf);
if (rc != ARX_OK) {
  return rc;
}

std::vector<std::uint8_t> glb;
pistoris::Level::GlbExportOptions glb_options;
glb_options.arx_units_per_glb_unit = 100.0f;
rc = level.exportGlb(glb, glb_options);
```

Level GLB export can attach static Model previews to matching entities:

```cpp
const std::array<const pistoris::Model*, 2> previews = {&human, &spider};
ArxLevelModelPreviewReport preview_report;
rc = level.exportGlb(glb, previews, glb_options, &preview_report);
```

Each non-anonymous Model is mapped from its FTL resource identity to an entity
class path. Duplicate identities keep the first Model. Invalid, anonymous,
unmappable, and duplicate Models are skipped and counted in the report.
Every entity instance with the same matching class path shares one preview
mesh. The Models are borrowed only for the duration of the call.

`importNative` requires FTS. LLF and DLF pointers are optional. `importGlb` builds
Level directly from a GLB byte span. GLB import and export own DCC coordinate
conversion; Level itself remains in native Arx coordinates with -Y up.
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

`renderMinimapPng` projects the stored rectangle from the referenced Level
bounds. Its Arx-unit projection offset and fill color are caller supplied.
Projection-based setting and rendering require referenced Level geometry.
`renderGameMinimapPng` performs the same projection and overwrites the final
one-pixel perimeter with its caller-supplied border color without changing the
image dimensions.
`renderCompactMinimapPng` chooses an Arx-unit offset for a tight projection
without leading padding and returns both that offset and the PNG.

`generateMinimap` stores a `640 x 640` PNG sampled from the full `0..16000`
Level X/Z domain at 25 Arx units per pixel. The default palette uses dark blue
foreground, lighter blue background, light brown water, cyan lava, and a
five-pixel white halo. The options overload can replace those colors, provide
sampler images, or change the halo. Rendering projects the image from the
top-left anchor at the minimum referenced X and maximum referenced Z, then
applies the requested offset. This can crop or pad the rendered PNG, so its
dimensions can differ from the stored image. Each sampler uses its color
directly when its image is empty; otherwise its image is stretched to
`640 x 640` and multiplied by that color. Sampler alpha is ignored and the
output is opaque. Downward-facing surfaces and slopes above 85 degrees are
ignored. The highest remaining surface determines whether the pixel is
foreground, water, or lava; the lower face index wins an exact-height tie. The
halo uses square pixel-distance rings around all occupied pixels; radius zero
disables it. Failed generation leaves the stored minimap unchanged.

The optional loading screen stores an encoded PNG, JPEG, BMP, or TGA image.
`renderLoadingScreenPng` produces the normal `320 x 390` layout and
`renderFullscreenLoadingScreenPng` produces the `640 x 480` fullscreen layout.
`transcodeLoadingScreenPng` preserves the source dimensions while producing
PNG.

The focused `level/images` API performs the same output operations on detached
encoded images. It can reproject a minimap between two Arx-unit projection
offsets, optionally with the game-layout one-pixel border, or render a loading
screen at original, normal, or fullscreen dimensions without a `Level`
instance. `projectionOffsetFromMiniOffset` and
`miniOffsetFromProjectionOffset` convert the values used by `mini_offsets.ini`;
the C API exposes equivalent functions.

Level GLB preserves a valid minimap image and placement. Invalid or ambiguous
minimap payloads are omitted with a warning. Loading screens have no Level GLB
representation. Native baking does not include either image sidecar; callers
render them and choose destinations with
`paths::levelMinimap`, `paths::levelLoadingScreen`, and
`paths::minimapResourceLevel`.

### Reading Collections

Level collections use caller-owned count-and-copy access. Query the count,
allocate a caller-owned array, and copy the requested range:

```cpp
std::vector<ArxLevelRoom> rooms(level.roomCount());
if (!rooms.empty()) {
  rc = level.copyRooms(0, rooms.size(), rooms.data());
}
```

The C ABI uses the same count-and-copy model:

```c
size_t room_count = 0;
ArxReturnCode rc = arx_pistoris_level_room_count(level, &room_count);
if (rc != ARX_OK) {
  return rc;
}

ArxLevelRoom* rooms = malloc(room_count * sizeof(*rooms));
if (room_count != 0 && rooms == NULL) {
  return ARX_BAD_ALLOC;
}
if (room_count != 0) {
  rc = arx_pistoris_level_copy_rooms(level, 0, room_count, rooms);
}
free(rooms);
```

Copied scalar and fixed-size fields are independent values. `ArxStringView`
and `ArxEncodedImageView` members borrow from Level. Any non-const Level
operation invalidates all previously returned borrowed views and collection
indices.

Indices are zero-based positions in the current collection, not persistent
object identities. An add operation returns an index valid for the resulting
Level state. Do not retain indices across another editing operation.

### Editing and Generation

Level provides add, set, remove, clear, and replacement operations for all
authored data. Mesh operations maintain room assignments and corner colors.
Room- or portal-topology changes discard room distances; ordinary geometry
edits do not.
Explicit compaction removes unreferenced vertices or textures.

Convenience operations include:

- room-aware vertex welding
- navigation-surface generation or floor filtering
- navigation-island pruning
- anchor generation, connection generation, and island pruning
- room-distance generation
- static corner-lighting generation

These operations are explicit. Import does not silently regenerate authored
navigation or connectivity.

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
ArxReturnCode rc =
    arx_pistoris_level_import_native(
        fts, llf, dlf, &level, NULL, ARX_NATIVE_TEXT_AUTO);
if (rc != ARX_OK) {
  return rc;
}

ArxLevelGlbExportOptions options =
    ARX_LEVEL_GLB_EXPORT_OPTIONS_INIT;
uint8_t* glb = NULL;
size_t glb_size = 0;
rc = arx_pistoris_level_export_glb(
    level, NULL, 0, &options, NULL, &glb, &glb_size);
if (rc == ARX_OK) {
  /* consume glb */
  arx_pistoris_free_bytes(glb);
}
arx_pistoris_level_destroy(level);
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
one-to-three-slot footprint or `-1` to derive each axis from the image. The
default derives both axes, retaining a native footprint within 3x3 and fitting
larger images proportionally to three slots on their longest axis. Deriving one
axis uses the explicit other axis and the source aspect ratio. The resolved
one-to-three-slot footprint is stored with the unchanged encoded image.

`renderIconPng` returns a detached PNG whose dimensions are exactly 32 pixels
per resolved slot, or empty output when the Model has no icon. A zero render
dimension uses the stored value, `-1` derives that axis from the source image,
and values from one to three are explicit. Two derived axes preserve the native
footprint when it fits, otherwise the longest axis becomes three slots.
`InventoryIconLayout` places aspect-preserving content at the center or one of
the four corners, or stretches it to fill the footprint. Center is the default.
Unused pixels are transparent. BMP rendering applies the engine black color
key and antialiasing. The stored image is unchanged.

### Native, OBJ, and GLB Conversion

```cpp
pistoris::Model model;
ArxReturnCode rc = pistoris::Model::importNative(model, ftl);
if (rc != ARX_OK) {
  return rc;
}

rc = model.setResourcePath("model:npc:human_base");
if (rc != ARX_OK) {
  return rc;
}

pistoris::NativeModelBundle baked;
rc = model.bakeNativeBundle({}, baked);

pistoris::ObjBundle obj;
rc = model.exportObj("human_base", obj);

pistoris::Model static_model;
rc = pistoris::Model::importObj(static_model, obj.text, obj.mtl);

std::vector<std::uint8_t> glb;
rc = model.exportGlb(glb);

pistoris::Model::LevelPreviewGlbOptions preview_options;
preview_options.class_path = "model:npc:human_base";
preview_options.asset_name = "human";
rc = model.exportLevelPreviewGlb(glb, preview_options);

pistoris::Model authored;
rc = pistoris::Model::importGlb(authored, glb);
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

Model uses caller-owned count-and-copy access like Level. Selection records
are addressed by stable `SelectionId` values and expose a name plus an optional
leading position and bone. Membership is queried and copied separately for
geometry vertices, bones, action points, and the implicit origin.

```cpp
std::vector<ArxModelBone> bones(model.boneCount());
if (!bones.empty()) {
  rc = model.copyBones(0, bones.size(), bones.data());
}

std::size_t member_count = 0;
rc = model.selectionVertexCount(selection_id, member_count);
std::vector<pistoris::VertexIndex> members(member_count);
if (rc == ARX_OK && !members.empty()) {
  rc = model.copySelectionVertices(
      selection_id, 0, members.size(), members.data());
}
```

Copied scalar fields are independent values. String and encoded-image members
are borrowed views into Model. Every non-const operation invalidates borrowed
views and collection indices. Selection IDs remain stable until their
selection is removed. Replacing or resetting Model invalidates all selection
IDs. Removing a selection clears its memberships without changing any other
selection ID.

`updateSelectionMembers` can replace any supplied membership category in one
transaction. `{nullptr,0}` leaves that category unchanged. A non-null pointer
with count zero clears it. Dedicated clear functions are also available.
Object setters preserve existing memberships, while replacing an entire mesh,
skeleton, or action-point collection clears the matching membership category.

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
Explicit compaction removes unreferenced vertices or textures.

`applyReference` requires at least one requested operation. It can copy exact
bone positions, replace bone-origin selection memberships, replace action-point
selection memberships, or combine those operations in one transaction.
Snapping and bone-origin membership copying require identical bone counts and
parent topology. Action-point membership copying does not require matching
skeletons. Bones correspond by index; name differences warn without preventing
the operation. Selections correspond by name. Reference-only selections are not
created, and target-only memberships of a copied category are cleared.
Repeated action-point names correspond by occurrence order. Unmatched target
action-point memberships clear, while selection memberships on unmatched
reference action points are omitted with a warning. The Model changes nothing
on failure.

`inferBoneOriginSelections` replaces bone-origin memberships using geometry
owned directly by each bone. A bone joins a selection when at least 90% of its
owned vertices belong to that selection. Unbound vertices are ignored, and a
bone without owned geometry receives no memberships. Vertex, action-point,
origin, and leading-vertex memberships are unchanged.

Native baking preserves copied Model-space bone positions exactly relative to
the emitted origin, which supports engines that compare replacement-model bone
origins by exact component equality.

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
`arx_pistoris_model_infer_bone_origin_selections` exposes the same inference
helper. `ARX_MODEL_REFERENCE_OPTIONS_INIT` selects no operation; enable at
least one field before calling `arx_pistoris_model_apply_reference`.
`ARX_MODEL_INVENTORY_ICON_SET_OPTIONS_INIT` derives both footprint axes.
`ARX_MODEL_INVENTORY_ICON_RENDER_OPTIONS_INIT` selects the stored footprint,
preserves aspect ratio, and centers content. The rendered PNG is released with
`arx_pistoris_free_bytes`.
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
instead reports the authored format path. The library does not search the
filesystem.

Animation resource identity is optional. `setResourcePath` accepts an Animation
selector or any portable logical `.tea` path. Selectors expand to registered
game paths; direct paths are normalized but retain their directory layout.

Keyframes use count-and-copy access. `copyKeyframes` returns independent Sound
indices, while `copySoundViews` returns borrowed logical paths and encoded-audio
views. `copyGroupTransforms` copies one contiguous group range for a selected
keyframe. `replaceKeyframes` changes the complete timeline in one transaction.
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
affected Animation. GLB import returns owning `std::unique_ptr<Animation>`
values.

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

Collection access uses count-and-copy operations. Returned texture, sound,
language, and encoding views borrow storage from Cinematic. Non-const calls
invalidate collection indices, `SoundHandle` values, and borrowed views.
Removing a referenced illustration or sound fails; explicit compaction removes
unused textures or sounds and remaps retained references.

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
lookup suffix. The mapping is not retained by Ambiance. The library never
searches for sound files.

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

Ambiance uses the same caller-owned count-and-copy model as Level.
`copyTracks` returns independent Sound indices; `copySoundViews` returns
borrowed path and encoded-audio views; key copies contain independent values.
Input strings, bytes, and key arrays are copied by editing calls. Every
non-const operation invalidates prior borrowed views, Sound indices, and track
indices.
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
Animation search locations depend on a registered selector type; their helpers
return `false` for an unsupported type. Level, Cinematic, and Ambiance have
fixed search locations.

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
expected native-format extension. Filesystem mounts are a CLI concern.

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
Model layout. A Model selector type describes that storage layout; it does not
override the engine's runtime classification of the complete class path.
`entityClassKind` reports the engine classification of a normalized class
path. `itemIconFromEntityClass` returns `<class>[icon]` for item classes, an
empty path for other classes, and rejects malformed class paths.

`resourceSelectorKind` and `arx_pistoris_path_resource_selector_kind`
classify the reserved selector prefix. They do not validate the selector
payload; use the corresponding `*FromSelector` parser for that.

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
