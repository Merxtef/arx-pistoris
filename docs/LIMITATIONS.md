# Fidelity and Limitations

This document is for users choosing a conversion path or deciding whether a
roundtrip preserves the data they care about.

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
FTL, TEA, FTS, DLF, LLF, and AMB carriers; it is not a separate Pistoris
intermediate. CIN has no compatible JSON representation.

Data absent from the compatibility schema cannot roundtrip through JSON.
Prefer native carriers or the matching GLB authoring format when the JSON
ecosystem is not required.

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

### Independent face normals are derived

Level exposes one normal per face corner. Native FTS also stores independent
`norm` and `norm2` face normals, which Level regenerates from triangle geometry
during FTS import. The original native values therefore do not survive an
`FTS -> Level -> FTS` roundtrip.

GLB import also derives Level face normals from triangle geometry. Standard GLB
has no separate per-face normal attribute.

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

### Room distances and anchor connections

Room distances and anchor connections are editable Level data without a
vanilla-DCC-friendly GLB representation. Level GLB stores both as opaque
best-effort round-trip data. Room distances are recovered only when the stored
room and portal structure still matches; otherwise the complete stored set is
discarded while Level import continues. Editing tools may discard the opaque
data. Regenerate either collection explicitly after GLB import when its
authored values did not survive or may be stale after geometry edits.

Native baking writes default `-1` room distances where the data is missing or
incomplete and reports the omission.

### Vertex welding is explicit

Native import preserves polygon-local position identity. It does not infer
that coincident vertices are shared. Call Level vertex welding explicitly when
the desired topology is known.

Room-aware welding protects portal-adjacent boundaries and can preserve,
reject, or discard faces that would collapse. It remains an inference:
coincident positions alone cannot prove original topological identity.

### Portal flattening is canonicalization

The engine derives a portal plane from one triangle, but it retains the
original portal vertices for bounds and visibility calculations. Explicit
portal flattening therefore changes the represented portal rather than merely
reproducing a lossless engine load step. It preserves the three vertices that
define the canonical plane and projects the remaining quad vertex onto it.

Only portals already accepted by Level validation can be flattened. The
operation is intended to clean up small nonplanar deviations, not recover
malformed portal geometry. Changed portals discard stored room distances.

### Portal geometry snapping is conservative

Portal snapping moves existing vertices only. It uses the bounded triangle or
quad surface, checks the rooms of every incident face, and rejects ambiguous
matches and moves that would collapse or reverse a face. It does not split,
delete, or reshape faces to repair larger topology errors.

## Level and GLB

### Missing texture formats default to PNG

GLB external image paths retain a known physical suffix independently from
the logical texture identity. If a path-only texture has no retained suffix,
GLB export assumes `.png`. The URI remains a best-effort reference; Pistoris
does not read files or verify that the external image exists. The CLI can
resolve and attach the referenced image before export.

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

Cell layout and quads are rebuilt during native baking. Room distances and
anchor connections round trip through opaque data when the editing tool
preserves it and their structural checks pass. Their generation remains
explicit and replaces recovered data.

### Materials collapse face data by identity

Level preserves raw per-face `transval` through the material name token
`TRANSVAL_<value>`. Standard GLB preview alpha can show ordinary values between
zero and one, but GLB cannot reproduce every native blend mode for values
outside that range.

GLB material properties also imply native flags:

- `doubleSided=true` implies `DOUBLESIDED`
- `alphaMode=BLEND` implies `TRANS` when base alpha is below `1` or the material
  carries `TRANS` or `TRANSVAL`
- `alphaMode=MASK` is image cutout and does not imply `TRANS`

`BLEND` with base alpha `1` and no native transparency token imports without
`TRANS`. If the resulting material has a texture, any texture alpha remains
available as native cutout; otherwise the material is opaque. Import warns
about this normalization.

Level cannot retain arbitrary GLB alpha-mask parameters. MASK base alpha and
cutoff are normalized to `1` and `0.5`; other values warn and do not survive
the conversion.

For ordinary geometry materials, a referenced base-color image defines texture
identity. Face-flag tokens in the material name still define flags; the
fallback stem supplies texture identity only when no image is referenced.
Level semantic material names are reserved and do not follow this fallback
rule.

### Native texture dimensions

Arx rendering expects power-of-two texture dimensions in paths that use its
wrap-clamp behavior. Native Level and Model baking preserve already compatible
PNG, JPEG, BMP, or TGA bytes. A non-power-of-two image is resized independently
on each axis to the next power of two and encoded as PNG.

GLB export embeds PNG and JPEG directly and converts BMP and TGA to PNG because
core GLB supports only PNG and JPEG images.

### Zone meshes are semantic volumes

Zone import welds transformed positions before reconstructing the perimeter.
This tolerates ordinary UV, normal, and material splits. Intentionally distinct
zone columns cannot occupy exactly the same position. Zone top and bottom
surfaces are reconstructed as planes; deviations are flattened with a warning.

### Entity previews are not Level geometry

A static mesh attached to an `arx_entity__*` root is an authoring preview. It
is ignored on Level import and is not baked into FTS geometry or Level
textures.

## Model and FTL

Model retains every native FTL selection as a generic named selection.
Selection names are normalized to ASCII lowercase and made unique with the
lowest available `_N` suffix. Membership can cover geometry vertices, bones,
action points, and the implicit origin.

For exact `cut_head`, `cut_torso`, `cut_larm`, `cut_rarm`, `cut_lleg`, and
`cut_rleg` names, the first native member is represented as an optional
selection leading vertex. Native baking emits an explicit leading vertex
first. If a nonempty exact `cut_*` selection has no leading vertex, baking
infers one from its first available member and logs the inferred position.
Empty selections are valid Model data but are omitted from native FTL.

Model coordinates are relative to the native FTL origin. Import subtracts the
origin from all represented positions, including selection leading vertices.
Baking emits a synthetic origin at `(0,0,0)`. This preserves relative game
geometry but not the original model-local offset or unused roleless native
vertex data.

Model stores an independent face normal and one normal per face corner. Native
FTL face normals are preserved. Baking duplicates native vertices when one
Model vertex uses different corner normals. Native vertex numbering and unused
unreferenced vertices are not preserved.

Degenerate native faces are discarded during Model import. Native vertices
referenced only by discarded faces and no semantic role are not retained.
Bone and action-point names are normalized to ASCII lowercase. Bone-name
collisions receive the lowest available `_N` suffix after the first spelling;
duplicate action-point names are preserved.

## Model and GLB

Model GLB is an authoring projection, not an archive of native FTL layout.
Corner normal or UV differences may split one Model vertex into several GLB
vertices. Otherwise export shares vertex data across material primitives and
import preserves represented sharing. A DCC may still rewrite indices and
sharing. Only Model vertices referenced by faces and referenced textures are
emitted. Corner normals use the GLB `NORMAL` attribute. The independent Model
face normal has no GLB representation and is regenerated from triangle
positions on import. Model stores one bone per vertex; GLB import keeps only
the greatest positive skin influence. An unskinned mesh on or below a joint
identified by another skin binds rigidly to its nearest such joint. Other
unskinned geometry and zero-weight vertices in a skin remain unbound.

The exact Model GLB grammar is documented in the
[Model and Animation authoring reference](authoring/MODEL_REFERENCE.md).

## GLB Material Fallbacks

Logical texture paths and GLB image URIs may contain `__`. Material names use
that delimiter for face flags, so export collapses underscore runs in the
fallback stem and warns once per affected texture. For ordinary materials, the
referenced image remains authoritative. If that link is removed, the fallback
cannot reconstruct the original repeated underscores.

## Animation and GLB

Model GLB may carry Animation sidecars but is not an archive of native TEA
layout.
Without an `arx_model_origin__*` node, Model import uses scene identity and does
not recover propelled Animation translation or rotation.
For a rigged Model, propelled motion comes from translation and rotation on the
shared ancestor of all imported meshes, root joints, and unbound positional
helpers. An unrigged Model uses the semantic origin itself. A carrier below the
semantic origin must have positive uniform default scale; that scale is part of
the bind pose. Animated carrier scale is unsupported. Translation or rotation
below the carrier is instead resolved into affected bone groups.
Animation channels are sampled at 24 frames per second; native interval
layout and GLB interpolation modes are not preserved. `CUBICSPLINE`
interpolation is unsupported and causes the affected Animation to be skipped.
Timelines that begin before zero are shifted forward. Every timestamp is
rounded to the nearest 24 Hz frame, and distinct timestamps that land on the
same frame collapse. Pistoris warns when one of these operations changes
timing.
Translation and rotation on intermediary hierarchy nodes are resolved into
bone transforms, and bone scale is accounted for when recovering descendant
translations. Intermediary scale cannot be represented. Native frame length,
footsteps, and sample events use Model Animation helper nodes. Animation groups
bind to Model bones by index over the available prefix. Groups beyond the Model
are discarded with a warning. GLB import normalizes groups whose translation,
rotation, and scale remain effectively identity within conversion tolerance for
the whole animation. Non-trailing identity groups remain as exact-identity
placeholders when a later group is retained. Trailing identity groups are
removed unless explicit `CLAIM` metadata retains them. A group is normalized
only when every keyframe remains within that tolerance; sub-tolerance values in
a group that is active elsewhere remain unchanged. Invalid sidecars and those
with unrepresentable timestamps are skipped and reported.

Model GLB keeps available Animation audio in external sidecars using its
original WAV, MP3, or Ogg Vorbis encoding. Native Animation baking converts
attached audio to PCM16 WAV and stores the sample reference below `sfx/`.
Path-only Sounds remain valid references but are not decoded or validated by
the library.

## Cinematic and CIN

CIN versions 1.75 and 1.76 import into one Cinematic representation. Native
writing always emits version 1.76. Version 1.75 sound data is ignored because
the game does not use it. For version 1.76, only sound slot 3 on each keyframe
is effective. Other slots, duplicate unreferenced sound-table entries, legacy
authoring metadata, language tags, and saved playback state are discarded.

Import keeps the effective keyframe at each frame and orders the resulting
timeline. Keys outside the declared timeline are discarded. Unsupported
interpolation becomes linear with a warning. Negative crossfade values become
enabled; values above one use their low bit and warn. The resulting native
bytes are semantically equivalent, not byte-identical.
Inactive flash decay and light data are reset to defaults on native read and
write; active effects, camera and illustration grid transforms, and stored
timing values still require finite values.

Nonzero CIN illustration grid position and roll are discarded with a warning.
They are not part of Cinematic editing or GLB authoring; native baking writes
zero grid transforms. CIN files that depend on those transforms will not retain
their original framing.

CIN speech paths contain an authored language directory, but keyframes refer
to one language-independent speech identity. Import drops that directory and
cannot recover a language registration. Callers may register languages and
attach localized encodings before bundle baking. Effect and speech paths use
separate namespaces, so the same logical path can exist in both.

Illustration and audio files remain caller-owned sidecars. Import returns
lookup information but never opens them. By default, bundle baking retains BMP
and game-compatible TGA illustrations and converts others to TGA; attached audio
is converted to PCM16 WAV. Path-only references emit no sidecar. Only
keyframe-referenced sounds enter the native table, which is limited to 256
entries.
Callers can request BMP illustration sidecars when an existing BMP at the same
logical path would take priority over TGA in the game. Without attached image
dimensions, native baking cannot prove that a path-only illustration's grid
fits the renderer; it still rejects subdivision scales unsafe even for the
smallest image. Dream crossfades also constrain the next illustration's grid.

Cinematic GLB is a semantic illustration-timeline authoring projection. It
embeds illustration images, maps camera-backed keys through illustration UVs,
and returns effect and speech references for caller-side audio lookup. Canonical
export replaces unusual illustration geometry with a centered rectangular
plane at the image's natural proportions, so GLB geometry is not preserved as
roundtrip state. Cinematic has no compatible JSON form or OBJ authoring
projection.

## Model and OBJ

OBJ represents static Model geometry, materials, and positional action points.
Skeletons, selections, animations, action-point bone bindings, and action-point
selection membership are not representable. Face normals are regenerated from
triangle geometry on import. Missing corner normals follow OBJ smoothing
groups. OBJ positions and normals use the same fixed 180-degree X-axis basis
conversion as Model GLB. OBJ `v=0` addresses the bottom texture edge while
Model `v=0` addresses the top; conversion flips V without clamping tiled
coordinates.

MTL has one opacity value per material. Model export separates distinct
per-face `transval` values into multiple materials and preserves every value in
the `TRANSVAL` material-name token. Values inside `[0,1]` also emit MTL opacity;
other values have no MTL opacity representation.

For textured materials, `map_Kd` supplies texture identity and the decoded
fallback stem is the fallback. A `no_tex` material is untextured only without
`map_Kd`; a supplied path wins with a warning. The CLI resolves declared MTL
files and texture images relative to the OBJ.

Logical texture paths and `map_Kd` paths may contain `__`. Export collapses
underscore runs only in the material fallback stem and warns once per affected
texture. The `map_Kd` path remains authoritative; without it, the fallback
cannot reconstruct the repeated underscores.

## Model Reference Operations

Game-side model-part replacement compares bone-origin positions exactly. Model
authoring can change those positions even when the resulting skeleton remains
otherwise coherent.

Pair a compatible base FTL with one or more requested operations:

```text
--ftl-reference <PATH>
--snap-bone-origins
--copy-bone-selections
--copy-action-selections
```

Snapping and bone-origin selection copying require equal bone counts and parent
topology. Action-point selection copying does not require matching skeletons.
Bone-name mismatches warn but do not prevent the operation. Selection copy
matches names already present in the target Model; it does not create
reference-only selections. Copying replaces the requested membership category,
so target-only memberships clear. Repeated action points match by name and
occurrence order; unmatched target action-point memberships clear, while
selection memberships on unmatched reference action points are omitted with a
warning.

`--infer-bone-selections` replaces bone-origin selection memberships using a
90% threshold over geometry owned directly by each bone. It is an authoring
helper rather than authoritative recovery and cannot be combined with
`--copy-bone-selections`.

## Ambiance and Audio

AMB versions 1.000 through 1.003 map to the version 1.001 carrier used for
writing. Unused track names, padding, and flag bits are discarded. Settings
that are inactive or unused by a constant automation value are normalized to
zero.

Native Ambiance baking converts attached WAV, MP3, and Ogg Vorbis data to
PCM16 WAV. Positioned playback, nonzero panning, and dynamic panning require
mono audio. A stereo Sound needed by both centered panning and spatial
playback produces separate stereo and mono sidecars with collision-safe names.
An existing `.wav` path is retained; any required mono conversion is emitted
as a separate suffixed file.

Native baking can warn when a non-master track's longest nominal duration
exceeds the master's shortest nominal duration. The check requires attached
audio and uses pitch and delay bounds; path-only Sounds are skipped. Runtime
scheduling can still vary beyond this nominal envelope.

Ambiance GLB keeps the original encoded audio in external sidecars and does
not embed it. Path-only Sounds remain valid references but are not decoded or
validated by the library. The exact node grammar and spatial projection are
documented in the
[Ambiance authoring reference](authoring/AMBIANCE_REFERENCE.md).
