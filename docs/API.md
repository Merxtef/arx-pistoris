# API Guide

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
arx_pistoris/level.hpp
arx_pistoris/level/bake.hpp
arx_pistoris/native/dlf.hpp
arx_pistoris/native/ftl.hpp
arx_pistoris/native/fts.hpp
arx_pistoris/native/llf.hpp
arx_pistoris/native/tea.hpp
arx_pistoris/paths.hpp
```

Public `.h` files are C-compatible. Public `.hpp` files require C++20.

## Return Codes

Fallible operations return `ArxReturnCode`. `ARX_OK` is zero. Always check the
return code before using output values.

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
| `100-999` | runtime, storage, compression, and internal failures |
| `1000-1999` | native formats |
| `2000-2999` | Level |
| `3000-9999` | reserved intermediate formats |
| `10000+` | external formats |

Use symbolic values rather than depending on a numeric assignment in pre-1.0
code.

## Native Formats

The C++ API exposes native carriers as value types:

```cpp
#include "arx_pistoris/pistoris.hpp"

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
DCL-compressed data. Their writers compress by default. TEA is always raw.
`readDlf` can return embedded lighting separately.

The C ABI exposes opaque `ArxFts`, `ArxDlf`, and `ArxLlf` handles:

```c
#include "arx_pistoris/arx_pistoris.h"

ArxFts* fts = NULL;
ArxReturnCode rc = arx_pistoris_fts_parse(data, size, &fts);
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

For DLF, `arx_pistoris_dlf_parse` returns embedded LLF through a separate
output handle; that handle is `NULL` when no valid embedded lighting exists.
DLF and LLF write options accept optional signer text. Native writers truncate
the resulting metadata to the native field capacity.

Direct FTL, TEA, OBJ, GLB, and JSON functions remain available through the
umbrella headers. These are legacy carrier-oriented surfaces until coherent
Model and Animations classes replace them.

JSON is an arx-convert compatibility serialization of native carriers. It is
not a separate Pistoris intermediate and is not promised to preserve data that
the compatibility schema cannot represent.

## C++ Level

`pistoris::Level` is the coherent public editing surface for levels. It owns
geometry, textures, room topology, navigation, lights, player spawn, entities,
fogs, zones, and spline paths. It maintains cross-module coherence, cached
bounds, and granular validation state.

### Import and Export

```cpp
pistoris::Level level;
ArxReturnCode rc = pistoris::Level::fromNative(level, fts, &llf, &dlf);
if (rc != ARX_OK) {
  return rc;
}

std::vector<std::uint8_t> glb;
pistoris::Level::GlbExportOptions glb_options;
glb_options.arx_units_per_glb_unit = 100.0f;
rc = level.exportGlb(glb, glb_options);
```

`fromNative` requires FTS. LLF and DLF pointers are optional. `fromGlb` builds
Level directly from a GLB byte span. GLB import and export own DCC coordinate
conversion; Level itself remains in native Arx coordinates with -Y up.

`bakeNativeBundle` returns FTS, LLF, DLF, and encoded texture sidecars in
memory. `bakeNativeDlf` rebuilds only scene data. The caller decides where any
returned resource is stored.

### Reading Collections

Level does not expose internal module storage. Query the count, allocate a
caller-owned array, and copy a requested range:

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

Convenience operations include:

- room-aware vertex welding
- navigation-surface generation or floor filtering
- navigation-island pruning
- anchor generation, connection generation, and island pruning
- room-distance generation
- static corner-lighting generation

These operations are explicit. Import does not silently regenerate authored
navigation or connectivity.

Call `validate()` for the complete Level or a focused validator when only one
area matters. Successful focused validation contributes to the cached
validation state; edits invalidate dependent checks.

### Coordinates and Bounds

Level uses native Arx coordinates and -Y up. Geometry and active portal X/Z
coordinates must be inside inclusive `[0,16000]`. Navigation support remains
finite-only.

GLB conversion accepts a finite Arx-units-per-GLB-unit ratio in inclusive
`[1,1000]`, defaulting to `100`. Import can choose a 100-unit-aligned X/Z
offset automatically; `GlbImportInfo` reports the applied value.

## C Level ABI

`ArxLevel` is an opaque handle. The C ABI mirrors the supported C++ Level
capabilities with creation, cloning, import, export, validation, collection
copying, editing, generation, and native baking functions.

```c
ArxLevel* level = NULL;
ArxReturnCode rc =
    arx_pistoris_level_from_native(fts, llf, dlf, &level);
if (rc != ARX_OK) {
  return rc;
}

ArxLevelGlbExportOptions options =
    ARX_LEVEL_GLB_EXPORT_OPTIONS_INIT;
uint8_t* glb = NULL;
size_t glb_size = 0;
rc = arx_pistoris_level_export_glb(
    level, &options, &glb, &glb_size);
if (rc == ARX_OK) {
  /* consume glb */
  arx_pistoris_free_bytes(glb);
}
arx_pistoris_level_destroy(level);
```

Passing `NULL` for documented Level generation or GLB option pointers selects
the C++ default options. Native bake option pointers are required. Use the
`ARX_*_INIT` macros when constructing options in C.

All C entry points prevent C++ exceptions from crossing the ABI boundary.
Failures are returned as `ArxReturnCode`.

## Buffer Ownership

- Parsers copy input bytes before returning.
- Successful C byte and string outputs belong to the caller and must be
  released with the matching `arx_pistoris_free_*` function.
- Never release API allocations with `free` or `delete`.
- `ArxNativeTextureFiles` owns its views until
  `arx_pistoris_native_texture_files_destroy`.
- `arx_pistoris_tea_name` and similar legacy accessors borrow from their
  owning handle.

## Resource Paths

`pistoris::paths` and the corresponding `arx_pistoris_path_*` C functions
build and parse canonical Level, Model, Animation, Cinematic, Ambiance, and
texture resource paths. They operate on portable logical identities, never
host filesystem paths.

A valid resource selector and its canonical full resource path are completely
interchangeable identities. For example, `model:npc:human_base` identifies the
same resource as its canonical model path. Filesystem mounts are a CLI concern.

`resourceShorthandKind` and `arx_pistoris_path_resource_shorthand_kind`
classify the reserved selector prefix. They do not validate the selector
payload; use the corresponding `*FromShorthand` parser for that.

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
#include "arx_pistoris/debug/level_diagnostics.hpp"
```

This surface may change without compatibility treatment and has no C ABI
counterpart.

## Metadata and Logging

- `version` / `arx_pistoris_version` returns the library version.
- `buildTimeString` / `arx_pistoris_build_time` returns build metadata.
- `errorString` / `arx_pistoris_strerror` describes a return code.
- `setLogCallback` / `arx_pistoris_set_log_callback` redirects library logs.
- `arx_pistoris_get_layout_hash` identifies the compiled public C header
  layout for compatible dynamic-loading checks.
