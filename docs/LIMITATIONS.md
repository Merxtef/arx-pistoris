# Known Limitations

Known fidelity trade-offs and encoding constraints per conversion.

## FTL <-> OBJ

### Per-face transval precision

OBJ MTL has one `d` (opacity) value per material, not per face. The exporter
groups faces by `(texture_id, face_type)` into a single material. If multiple
faces share that key but carry different `transval` values, the exporter averages
them and writes the average as `d`. On re-import every face in that material gets
the averaged value.

Workaround: ensure faces that share a texture and flags also share the same
transval before exporting.

### Texture stem `__` encoding

The exporter encodes face flags in material names using `__` as a separator
(e.g. `BODY__TRANS__WATER`). If a texture filename stem itself contains `__`,
`decodeMatName` on re-import will split at the first occurrence and misread the
rest as flags. Arx Fatalis asset stems never contain `__`, so this does not
affect any real game content.

## FTL+TEA <-> GLB

### Per-face transval precision

Same constraint as FTL <-> OBJ: GLTF materials carry one alpha value. The
exporter groups faces by `(texture_id, face_type)`; when faces sharing that key
disagree on `transval`, the exporter averages them and writes the average into
`baseColorFactor[3]`. On re-import every face in that material gets the
averaged value.

Workaround: ensure faces that share a texture and flags also share the same
transval before exporting.

### Texture stem `__` encoding

Identical to the OBJ case. The GLTF material name is built via the same
`matName(stem, flags)` helper and decoded with the same `decodeMatName` on
import; a texture stem containing `__` would be misread. Arx Fatalis asset
stems never contain `__`.

### Bone order depends on ordinal-prefixed names

GLB export writes FTL bone groups with numeric order prefixes such as
`000__root`, `003__chest`, or `042__hand`. The exporter pads to at least
three digits for readable sorting, but this is not a 999-bone limit; larger
ordinals expand as needed. On import, those ordinals restore the original FTL
group order before TEA animation tracks are decoded, and the prefix is stripped
from the resulting FTL group name. This keeps assets editable in DCC tools that
reorder internal joint arrays.

If a DCC tool removes prefixes, duplicates them, changes them to values outside
the bone count, or creates an order where a child appears before its parent,
the importer warns and falls back to glTF joint order/topology. The imported
model and animations may still be internally consistent, but the TEA group
order may no longer match the original asset.

### Unreferenced texture containers dropped

The exporter only emits GLTF materials for `(texture_id, face_type)` pairs
that appear in at least one face (`CollectMaterials`). An FTL
`texture_container` not referenced by any face is absent from the GLB, so it
does not survive a roundtrip. Face-to-container mapping for used textures is
preserved exactly; only dead entries are lost.

### Selection names depend on DCC attribute handling

FTL selections are exported as custom GLTF `VEC4` attributes, plus an ordered
`arx_selection_names` mesh extra carrying the original names. Exact names
roundtrip when that extra survives. If a DCC tool rewrites attributes to names
such as `COLOR_0` or changes custom attribute casing, the importer can still
recover the selection masks but the names may need repair.

Workaround: use the CLI `--rename-selections` option after GLB import to
rename selections by their imported order. Empty rename entries and trailing
selections omitted from the rename list are left unchanged.

### Synthetic origin/action vertices need selection affiliation

GLB import synthesizes FTL-only vertices for `header.origin`, bone origins, and
action points. DCC tools do not expose those vertices as ordinary editable mesh
vertices, so their selection membership cannot always be authored directly.
If a required synthetic vertex is not in the expected selection, game-side
merge/copy logic may skip it; for bone origins this can reset origins and break
animation.

Workaround: use `--reference-ftl` with `--copy-reference-affiliations` to copy
selection membership for synthetic vertices from a compatible base-game model.
The GLB importer warns when synthetic origin/action vertices are not in any
selection.

### Exact bone and action positions for merged models

Some game merge/equipment paths require bone origin positions to be byte-for-byte
compatible with the base model and may compare floats with `==`. A normal GLB
roundtrip preserves the edited model's own skeleton, but it cannot guarantee
exact compatibility with a separate base-game FTL.

Workaround: use `--reference-ftl` with `--snap-bone-origins-to-reference snap-origins`
and `--snap-action-points-to-reference` for same-kind models that are already
almost identical. Use `delta-deform` only as a simple per-group translation
baseline. `hierarchy-deform` is experimental and can severely distort models
whose proportions differ from the reference, even when group topology matches;
its optional step limit is mainly useful for debugging where deformation starts
diverging. Deformation modes use the single-owner group map from FTL extras
(`vertex_to_bone`), not raw overlapping group index lists. They are heuristic
repairs, not full ARAP/cage solves, and can distort if topology or group
ownership differs from the reference.

### Header origin position reset

The exporter shifts all vertex, bone-bind, and unbound-action-point positions
by `-header_origin.position` so the GLB mesh is centered at the entity pivot
(which both the Arx engine and arx-pistoris expect at `(0,0,0)`). The
importer synthesizes a new `header.origin` vertex at `(0,0,0)`. After a
roundtrip the entity pivot is always at the origin of the mesh-local frame.
For source FTLs whose pivot was offset, the absolute mesh-local coordinates
change; relative geometry, rig, and animation data are preserved. The engine
only cares about the pivot-to-vertex relationships, not absolute coords.
