# API Integration

arx-pistoris exposes two integration surfaces:

- A stable C ABI in `arx_pistoris/arx_pistoris.h`, built by the CMake target `arx_pistoris_c`. The produced shared library is named `arx_pistoris`.
- A C++20 wrapper in `arx_pistoris/pistoris.hpp`, built as the static library `arx_pistoris_cpp`.

The public API uses only the standard C/C++ library. Format parsing and conversion functions return `ArxReturnCode`; metadata, logging setup, free functions, and simple accessors do not.

## Error Handling

Always check the return code before using output values.

Return code ranges:

- `0-79` - general errors
- `80-99` - internal errors such as `ARX_BAD_ALLOC` and `ARX_UNEXPECTED_EOF`
- `100+` - Arx format errors (`FTL=100+`, `TEA=200+`, `...`)
- `1000+` - external format errors (`OBJ=1000+`, `GLB=1100+`, `JSON=1200+`)

```c
ArxReturnCode rc = /* ... */;
if (rc != ARX_OK) {
    fprintf(stderr, "%s\n", arx_pistoris_strerror(rc));
}
```

## C API

The C API uses opaque handles. Parse or import a file into a handle, operate on the handle, then free it with the matching `arx_pistoris_*_free` function.

Returned `char*`, `uint8_t*`, and TEA handle arrays must be released with the paired `arx_pistoris_free_*` function from the API. Do not call `free()` or `delete[]` on returned buffers.

### FTL Model I/O

```c
#include "arx_pistoris/arx_pistoris.h"

ArxFtlHandle ftl = NULL;
ArxReturnCode rc = arx_pistoris_ftl_parse(data, size, &ftl);
if (rc != ARX_OK) {
    fprintf(stderr, "%s\n", arx_pistoris_strerror(rc));
    return;
}

uint8_t* ftl_bytes = NULL;
size_t ftl_size = 0;
rc = arx_pistoris_ftl_write(ftl, &ftl_bytes, &ftl_size);
if (rc == ARX_OK) {
    /* use ftl_bytes / ftl_size */
    arx_pistoris_free_bytes(ftl_bytes);
}

arx_pistoris_ftl_free(ftl);
```

### OBJ Export And Import

OBJ export is static geometry only; groups, actions, and selections are not represented in OBJ.

```c
char* obj_text = NULL;
char* mtl_text = NULL;

ArxReturnCode rc = arx_pistoris_ftl_to_obj(ftl, "model", &obj_text);
if (rc == ARX_OK) rc = arx_pistoris_ftl_to_mtl(ftl, &mtl_text);

arx_pistoris_free_string(obj_text);
arx_pistoris_free_string(mtl_text);
```

```c
ArxFtlHandle imported = NULL;
ArxReturnCode rc = arx_pistoris_obj_parse(
    obj_data, obj_size,
    mtl_data, mtl_size,   /* NULL/0 if no MTL; NULL/nonzero is invalid */
    "model.obj",          /* optional; may be NULL */
    &imported);
```

### TEA Animation I/O

```c
ArxTeaHandle tea = NULL;
ArxReturnCode rc = arx_pistoris_tea_parse(data, size, &tea);
if (rc == ARX_OK) {
    const char* name = arx_pistoris_tea_name(tea); /* lifetime tied to tea */
    printf("TEA name: %s\n", name ? name : "");
    arx_pistoris_tea_free(tea);
}
```

### JSON

Both FTL and TEA can be exported to and imported from JSON.

```c
char* json = NULL;
ArxReturnCode rc = arx_pistoris_ftl_to_json(ftl, 1, &json); /* pretty != 0 */
if (rc == ARX_OK) {
    /* use json */
    arx_pistoris_free_string(json);
}
```

### GLB

GLB export can include zero or more TEA animations. `NULL/0` means mesh and skeleton only.

```c
uint8_t* glb = NULL;
size_t glb_size = 0;
ArxReturnCode rc = arx_pistoris_to_glb(ftl, teas, tea_count, &glb, &glb_size);
if (rc == ARX_OK) {
    /* use glb / glb_size */
    arx_pistoris_free_bytes(glb);
}
```

GLB import returns one FTL handle and a heap array of TEA handles.

```c
ArxFtlHandle ftl = NULL;
ArxTeaHandle* teas = NULL;
size_t tea_count = 0;

ArxReturnCode rc = arx_pistoris_from_glb(data, size, "model.glb", &ftl, &teas, &tea_count);
if (rc == ARX_OK) {
    arx_pistoris_free_tea_array(teas, tea_count);
    arx_pistoris_ftl_free(ftl);
}
```

### C Utilities

- `arx_pistoris_version()` returns the library version string.
- `arx_pistoris_build_time()` returns the build timestamp string.
- `arx_pistoris_get_layout_hash()` returns a public-header layout hash for C ABI hot-reload checks.
- `arx_pistoris_set_log_callback()` redirects library log messages.
- `arx_pistoris_ftl_validate()` and `arx_pistoris_tea_validate()` validate loaded handles.
- `arx_pistoris_ftl_overwrite_texture_paths()` replaces existing FTL texture containers with one path in place. If the model has no texture containers, it is a no-op.
- `arx_pistoris_ftl_apply_xform()` and `arx_pistoris_tea_apply_xform()` apply rotate/scale/offset transforms in place.
- `arx_pistoris_ftl_snap_bone_origins_to_reference()` snaps target FTL bone origins to a reference FTL.
- `arx_pistoris_ftl_snap_action_points_to_reference()` snaps target FTL action points to matching reference action points.
- `arx_pistoris_ftl_copy_synthetic_selection_affiliations()` copies synthetic vertex selection affiliations from a reference FTL.

## C++ API

The C++ API uses transparent `pistoris::Ftl` and `pistoris::Tea` data structures plus `std::vector`, `std::span`, and `std::string` outputs. Include `pistoris.hpp` and link the `arx_pistoris_cpp` target; the target defines `ARX_PISTORIS_CPP_API`.

```cpp
#include "arx_pistoris/pistoris.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

std::vector<std::uint8_t> data = /* ... */;

pistoris::Ftl ftl;
ArxReturnCode rc = pistoris::readFtl(data, ftl);
if (rc != ARX_OK) {
    throw std::runtime_error(pistoris::errorString(rc));
}

pistoris::Obj obj;
rc = pistoris::exportObj(ftl, "model", obj);
if (rc == ARX_OK) {
    /* obj.text contains OBJ; obj.mtl contains MTL */
}
```

### C++ Surface

- Native I/O: `readFtl`, `writeFtl`, `readTea`, `writeTea`
- OBJ: `exportObj`, `importObj`
- GLB: `exportGlb`, `importGlb`
- JSON: `exportJson`, `importJson`
- Validation: `validate`
- Transforms: `makeAffineXform`, `applyTransform`
- FTL utilities: `overwriteTexturePaths`
- Reference repair helpers: `snapFtlBoneOriginsToReference`, `snapFtlActionPointsToReference`, `copyFtlSyntheticSelectionAffiliations`
- Metadata/logging: `version`, `buildTimeString`, `errorString`, `setLogCallback`

`Ftl::extras` is derived/cache-like state for advanced callers. Avoid relying on it unless you have just called `validate(ftl)` or a library operation that validates the model. Manual edits to `Ftl` may leave it stale, and future versions may change these derived fields.
