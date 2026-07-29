# Fidelity and Limitations

Pistoris aims for semantically lossless conversion between supported native
game formats and their coherent intermediate representation. It does not
promise byte-identical output: records may be reordered, geometry may be
resliced, compressed bytes will differ, and equivalent derived data may be
rebuilt.

GLB and OBJ are authoring surfaces. They preserve as much editable meaning as
their DCC-friendly structures allow, but they are not archives of every native
record.

## Compatible JSON

JSON support matches arx-convert schemas. JSON is converted to or from native
FTL, TEA, FTS, DLF, and LLF carriers; it is not a separate Pistoris
intermediate.

Data absent from the compatibility schema cannot roundtrip through JSON.
Prefer native carriers or Level GLB when the JSON ecosystem is not required.

## Level Native Conversion

### FTS remains the required Level carrier

Native-to-Level conversion requires FTS. LLF and DLF are optional companions.
An external LLF takes precedence over lighting embedded in DLF. Missing
lighting produces neutral corner colors and no dynamic lights.

### Empty rooms are omitted from GLB

Native-to-Level conversion retains real room slots even when they have no
faces. Level GLB export omits those rooms because they have no editable
geometry. Portals and room-distance records tied to omitted rooms cannot
survive that GLB projection. Pistoris warns when the discarded topology
contains positive room distances or portals that suggest meaningful data.

### Native layout is rebuilt

Level does not retain the original FTS cell arrays or polygon packing. Native
baking reslices Level geometry into 100 by 100 Arx cells and reconstructs
compatible native quads by default. Disable quad reconstruction only when
triangle-for-triangle native output is specifically needed.

Quad reconstruction joins only compatible adjacent triangles. It does not
change their independent corner normals or merge arbitrary fragmented
geometry.

### Native limits are enforced

Pistoris rejects FTS data that exceeds native representation limits, including:

- more than 254 real rooms
- more than 32768 room polygon references in one cell
- more than 65535 render corners for one room and texture resource, excluding
  `IGNORE` and `HIDE` faces as the engine does
- out-of-range native indices and counts

Level enforces its own collection and index limits, but it need not reject the
per-room texture-corner total. During Level baking, a texture that would exceed
that native limit is split across collision-free `_N` resource aliases. Every
alias references an identical image. Callers that skip texture sidecar export
must create those copies themselves.

### Room distances and anchor links

Room distances and anchor connections are editable Level data, but neither has
a vanilla-DCC-friendly GLB representation. Level GLB export omits them.
Regenerate them explicitly after GLB import, or retain the in-memory Level when
their exact authored values matter.

Native baking writes default `-1` room distances where the data is missing or
incomplete and reports the omission.

### Vertex welding is explicit

Native import preserves polygon-local position identity. It does not infer
that coincident vertices are shared. Call Level vertex welding explicitly when
the desired topology is known.

Room-aware welding protects portal-adjacent boundaries and can preserve,
reject, or discard faces that would collapse. It remains an inference:
coincident positions alone cannot prove original topological identity.

## Level and GLB

### Coordinates differ

Level uses native Arx coordinates with -Y up. Geometry and active portal X/Z
coordinates must be inside inclusive `[0,16000]`. GLB uses +Y up.

GLB conversion applies the selected unit ratio and offset at the boundary. The
ratio must be inside inclusive `[1,1000]` and defaults to 100. Automatic import
placement chooses a 100-unit-aligned X/Z offset. Keep the reported offset when
an exact inverse export matters.

### GLB topology cannot describe every seam

Level stores indexed positions while UVs, normals, and colors live on face
corners. GLB uses one index across all vertex attributes, so export duplicates
render vertices at attribute seams.

On import, a Level position is identified by its source node, `POSITION`
accessor, and accessor index. The same source position can remain shared across
primitives, but equal coordinates from different nodes or accessors are not
stitched because GLB cannot say whether the split was intentional or created
for rendering. Keep the original DCC or Level source when welded topology is
authoritative.

Portal import performs only a narrow positional weld needed to recover a
three- or four-point semantic perimeter from ordinary GLB render splits.

### GLB omits native build products

Level GLB does not retain:

- FTS cell slicing and packing
- FTS quad slots
- room-distance records
- anchor connections

Cell layout and quads are rebuilt during native baking. Room distances and
anchor links are generated only when explicitly requested.

### Materials collapse face data by identity

Level preserves raw per-face `transval` through the material name token
`TRANSVAL_<value>`. Standard GLB preview alpha can show ordinary values between
zero and one, but GLB cannot reproduce every native blend mode for values
outside that range.

GLB material properties also imply native flags:

- `doubleSided=true` implies `DOUBLESIDED`
- `alphaMode=BLEND` implies `TRANS`
- `alphaMode=MASK` is image cutout and does not imply `TRANS`

When a material references a base-color image, that image defines texture
identity. The material stem still defines flags.

### Texture names use `__` as syntax

`__` separates texture stems from face flags. Do not use it in newly authored
texture names.

The original game has exactly three known texture-path exceptions. Level maps
them to safe aliases and restores the native identities during baking:

```text
graph/obj3d/textures/l4_dwarf_[stone]__wall01
<-> graph/obj3d/textures/l4_dwarf_[stone]_wall01

graph/obj3d/textures/l4_dwarf_[stone]__wall24
<-> graph/obj3d/textures/l4_dwarf_[stone]_wall24

graph/obj3d/textures/npc_human__base_hero_head
<-> graph/obj3d/textures/npc_human_base_hero_head_1
```

No general escaping scheme exists for additional `__` texture names.

### Native texture dimensions

Arx rendering expects power-of-two texture dimensions in paths that use its
wrap-clamp behavior. Native Level baking preserves already compatible PNG,
JPEG, BMP, or TGA bytes. A non-power-of-two image is resized independently on
each axis to the next power of two and encoded as PNG.

GLB export embeds PNG and JPEG directly and converts BMP and TGA to PNG because
core GLB supports only PNG and JPEG images.

### Zone meshes are semantic volumes

Zone import welds transformed positions before reconstructing the perimeter.
This tolerates ordinary UV, normal, and material splits. Intentionally distinct
zone columns cannot occupy exactly the same position.

### Entity previews are not Level geometry

A static mesh attached to an `arx_entity__*` root is an authoring preview. It
is ignored on Level import and is not baked into FTS geometry or Level
textures.

## FTL and OBJ

OBJ represents static FTL geometry and materials only. Skeletons, action
points, vertex selections, and animations are not representable.

MTL has one opacity value per material. Faces that share a texture and flag set
also share the imported transparency value. Pistoris cannot preserve distinct
per-face `transval` values within that one material.

Texture identity is selected from `# arx_path`, then `map_Kd`, then the decoded
material stem.

## Legacy FTL and TEA GLB

### Transparency is material-wide

Legacy FTL GLB has no Level-style `TRANSVAL` name token. Faces grouped into one
GLB material share its alpha value. Distinct native `transval` values within
that material are averaged on export.

### Unused texture containers are omitted

Only textures referenced by FTL faces are emitted to GLB. Unreferenced native
texture-container records do not survive a GLB roundtrip. Referenced texture
identity remains stable.

### Bone order depends on indexed names

Exported bone names use exact zero-based prefixes such as `000__root`.
Import restores FTL group order from those prefixes when every index is unique,
complete, and topologically valid.

If a DCC removes or corrupts the prefixes, Pistoris warns and falls back to GLB
joint order. The resulting model may remain internally valid but no longer
match an existing TEA group order.

### Selection names depend on DCC behavior

FTL selections use custom GLB `VEC4` attributes plus an ordered
`arx_selection_names` mesh extra. Exact names survive when the DCC preserves
the extra. A DCC may rewrite attributes to positional names such as `COLOR_0`;
the masks can remain recoverable while names require CLI repair.

Use `--rename-selections` to restore names by imported position.

### Synthetic vertices need reference repair

GLB import synthesizes FTL-only vertices for the model origin, bone origins,
and action points. Their original selection membership is not always editable
through a DCC. Game-side model merge logic may depend on that membership and
on bit-exact bone or action positions.

Use a compatible base FTL with:

```text
--ftl-reference
--snap-bone-origins-to-reference
--snap-action-points-to-reference
--copy-synthetic-selection-affiliations
```

Reference deformation modes are repair heuristics, not general mesh
deformation solvers.

### The model pivot is canonicalized

FTL GLB export centers the mesh at the entity pivot. Import synthesizes the FTL
header origin at `(0,0,0)`. Relative geometry, skeleton, and animation data are
preserved, but an originally offset mesh-local coordinate frame does not
roundtrip byte-for-byte.
