# Model and Animation Authoring Reference

Back to the [Authoring Reference](../AUTHORING_REFERENCE.md). For worked
examples, use the [Model and Animation Authoring Guide](MODEL_GUIDE.md). Shared
notation, materials, and face flags are defined in
[Shared Authoring Conventions](CONVENTIONS.md).

This reference is for authors and tools that need the exact Model GLB,
Animation sidecar, Level preview, and static OBJ contracts.

## Model Level Preview

Static preview root and required direct child:

```text
arx_entity__000__<asset-name> (mesh: <asset-name>)
`-- CLASS_<entity-class-path>__<asset-name>
```

The preview contains Model mesh, materials, and textures only. It has no
skeleton, action points, selections, or Animations. It uses Level coordinates
and defaults to 100 Arx units per GLB unit.

`<entity-class-path>` may use a Model selector or an extensionless entity class
path. Direct class paths are normalized using the same rules as Level entity
classes. An empty class path writes the literal `<class-path>` placeholder. The
asset name defaults to `asset` and follows the Level identifier grammar.

Level GLB export may attach the same mesh representation to matching entity
roots. The preview mesh is placement context and is ignored on Level import.

[Worked preview example](MODEL_GUIDE.md#level-preview)

## Model Root and Coordinates

Optional semantic origin:

```text
arx_model_origin__<label>
```

At most one may exist. Its world transform defines the authoring origin and is
removed on import. When present, only eligible meshes, joints, action points,
and selection probes below it are imported. Meshes outside it are ignored with
one warning. When absent, scene identity is the origin and all eligible meshes
are imported.

Transforms below the semantic origin affect unskinned geometry directly.
Skinned geometry is evaluated in the joints' default scene pose; the skinned
mesh node's own transform has no effect. This evaluated pose becomes the Model
rest state. Scale the complete character rig in the DCC so mesh, joints, and
Animation data remain coherent. Joint default transforms and their resulting
default-pose skin transforms must contain only translation, rotation, and
positive uniform scale.

Model-origin selection membership and propelled Animation motion require the
semantic origin.

Model GLB uses the same `(x,-y,-z)` basis conversion as Level GLB and defaults
to 10 Arx units per GLB unit. Unskinned mesh nodes accept rotation, reflection,
and uniform scale without shear. Joint and default-pose skin transforms require
positive uniform scale without shear. The semantic-origin transform is removed,
and the skinned mesh node transform is ignored. At least one triangle is
required. Only vertices referenced by faces are exported. Corner normals use
the GLB `NORMAL` attribute; the independent Model face normal is regenerated
from triangle positions on import and cannot roundtrip through GLB.

Canonical export uses the ordinary `mesh`, `skeleton`, `bones_parent`, and
`animations_parent` wrappers. For rigged Models, `skeleton` contains all mesh
nodes, root joints, and unbound action-point or selection-probe helpers. Their
names are not required on import.

[Worked origin example](MODEL_GUIDE.md#coordinates-and-origin)

## Model Materials

Material names use the [shared face flags](CONVENTIONS.md#materials-and-face-flags)
and may carry exact transparency:

```text
<fallback-stem>[__<face-flag>]...[__TRANSVAL_<float>]
no_tex[__<face-flag>]...[__TRANSVAL_<float>]
```

`TRANSVAL` requires effective `TRANS`. A `TRANS` token always supplies it. With
`alphaMode=BLEND`, a `TRANSVAL` token or base alpha below `1` also supplies it;
without `TRANSVAL`, material alpha supplies `1 - baseColorFactor.a`. A
referenced base-color image supplies texture identity in preference to the
fallback stem, including on a `no_tex` material. `QUAD` is discarded with a
warning because Model faces are triangles. Unreferenced Model textures are not
emitted.

GLB properties also imply flags:

- `doubleSided=true` supplies `DOUBLESIDED`.
- `alphaMode=BLEND` with base alpha `1` and no `TRANS` or `TRANSVAL` is imported
  without native transparency. Texture alpha remains available as native
  cutout; a material without a texture becomes opaque. Import warns in either
  case.
- `alphaMode=MASK` must use base alpha `1` and cutoff `0.5`. Import treats it
  as image cutout without supplying `TRANS` and resets other values with a
  warning.

[Worked material example](MODEL_GUIDE.md#materials)

## Model Bones and Skin

Joint node:

```text
<index>__<bone-name>
```

Indices are exact zero-based positions. Every index in `[0,N)` must occur once,
`N` must not exceed 1024, and every parent must have a lower index than its
children. Multiple root joints form an ordered forest. With a semantic origin,
every joint must be below it. Bone names become unique normalized lowercase
names on import. Canonical export pads indices to at least three digits.

Any number of mesh objects may bind to the same coherent rig. Separate roots
may share an ordinary `skeleton` ancestor. Each skin is evaluated independently
in the shared rig's default pose. Inverse bind matrices are optional and
default to identity. Model stores one bone per vertex, so import keeps the
positive influence with the greatest weight. Vertices without a positive
influence remain unbound.

An unskinned mesh on or below a joint identified by another skin binds rigidly
to its nearest such joint. An unskinned mesh without one remains unbound. This
hierarchy fallback does not apply to zero-weight vertices in a skin. A
`<index>__<bone-name>` node not referenced by a skin does not establish a
skeleton.

Optional detached bone record:

```text
arx_bone__<bone-name>
```

It matches a normalized bone name and carries position-independent metadata.
Canonical export places one record per bone below `bones_parent` at scene level.
Unknown records warn and are ignored. If several records identify one bone,
the first is used.

Recognized direct children:

```text
ORIGIN_OWNER__<label>
SETTINGS__BLOB_SHADOW_<size>__<label>
```

At most one `ORIGIN_OWNER` helper may exist across all records. It assigns the
implicit Model origin to that bone. Without one, bone 0 owns the origin. Each
bone may have one nonnegative blob-shadow size in GLB units.

[Worked skeleton example](MODEL_GUIDE.md#skeleton-actions-and-selections)

## Model Actions

Terminal action point:

```text
arx_action__<action-name>__<label>
```

The nearest ancestor joint owns it. Without one, it is unbound. Its world
position relative to the semantic origin, or scene identity when no origin
exists, becomes the action-point position.

Action names become lowercase and may repeat. The label is ignored and safely
absorbs DCC suffixes. Canonical export uses `action_N`, where `N` is the
zero-based action-point index.

[Worked action example](MODEL_GUIDE.md#skeleton-actions-and-selections)

## Model Selections

Geometry membership uses custom `VEC4` primitive attributes:

```text
_<UPPERCASE_SELECTION_NAME>
```

The first component must be inside `[0,1]`; values above `0.5` select the
vertex. The same attribute name on different primitives identifies one
selection. A missing attribute means no members on that primitive. An all-zero
attribute on any primitive preserves a selection with no geometry members.

Selection names are stored in lowercase. They contain only ASCII letters,
digits, hyphens, and single interior underscores, cannot start or end with an
underscore, and have a maximum length of 63 characters. Import normalizes
nonconforming names and assigns the lowest available `_N` suffix after
collisions.

Direct point-membership helper:

```text
SELECTION_<selection-name>__<label>
```

Place it directly below a detached bone record, action point, or semantic Model
origin to include that item.

Terminal leading-point node:

```text
arx_selection_probe__<selection-name>__<label>
```

Its world position becomes the optional selection leading point. The nearest
ancestor joint owns it. Multiple probes for one selection warn; the first is
used.

[Worked selection example](MODEL_GUIDE.md#skeleton-actions-and-selections)

## Model Animations

GLB Animation channels are interpreted at 24 frames per second. Key timestamps
must lie on that frame grid. Import shifts a timeline forward when any channel
starts before zero, rounds every timestamp to the nearest frame, and collapses
distinct timestamps that land on the same frame. Import warns when one of
these operations changes timing.

The GLB animation name is its authoring name and uses the shared semantic-name
grammar. Optional metadata uses a matching terminal helper:

```text
arx_animation__<animation-name>
```

Canonical export places helpers below the ordinary scene-level
`animations_parent` node. Each helper may contain:

```text
PATH_<animation-resource>__<label>
SETTINGS__<setting>[__<setting>]...__<label>
SOUND__FRAMES_<frame>[_<frame>]...__<label>
`-- PATH_<sample-resource>__<label>
GROUPS[__VOID_<group-item>[_<group-item>]...][__CLAIM_<group-item>[_<group-item>]...]__<label>
```

`<setting>` is one of:

```text
EXTRA_FRAME
FRAME_LENGTH_<frame>
STEPS_<frame>[_<frame>]...
```

Each `SETTINGS` helper contains at least one setting. At most one direct
Animation `PATH` is allowed. It may be an Animation selector or logical game
path. The helper name must match the GLB animation name before import
normalization. Imported Animation names are normalized and made unique.
`SETTINGS` and `SOUND` may repeat. Each `SOUND` has exactly one direct `PATH`
child. Every `PATH` label is mandatory and ignored; parsing uses the final
`__`, so resource paths may contain `__`. Repeated frame lists are merged, but
one frame cannot identify two different sample resources.

`<group-item>` is one zero-based group index or an inclusive range such as
`8-10`. `GROUPS` contains `VOID`, `CLAIM`, or both. Multiple helpers merge;
one group cannot be both. `VOID` forces exact identity at that index. `CLAIM`
preserves sampled transforms without conversion-tolerance normalization and
keeps an exact-identity group in the baked TEA group count. Trailing unclaimed
exact-identity groups are removed after applying the helper state.

Sample paths are relative to the GLB. Audio remains in external sidecar files.

`EXTRA_FRAME` sets native frame length to the final GLB key plus one.
`FRAME_LENGTH` sets it exactly, and the two forms are mutually exclusive. If an
exact length falls between keys, import samples a key at that frame and drops
later keys. A longer exact length does not add a synthetic GLB key. With neither
token, the final key is the frame length.

For a rigged Model, propelled translation and rotation use the deepest shared
non-joint ancestor of all imported mesh nodes, root joints, and unbound
action-point or selection-probe helpers. Canonical export uses the ordinary
`skeleton` wrapper for this purpose. In Blender, animate the Armature object's
translation and rotation while leaving `arx_model_origin__*` static. An
unrigged Model uses the semantic origin itself as the motion carrier. A carrier
below the semantic origin must have positive uniform default scale. Its default
transform forms the bind pose and does not become propelled motion. Animated
carrier scale is unsupported. Indexed bones support translation, rotation, and
scale.
Animation groups bind by index
to the Model bones in their shared prefix; bone names are irrelevant. Export
discards groups beyond the Model with a warning. Import treats a group that
stays at effective identity within GLB conversion tolerance throughout the
animation as unused when no `GROUPS` state overrides that inference. Unused
groups below the last retained group remain as exact-identity placeholders.
Trailing unclaimed identity groups are removed. Canonical export writes `VOID`
for non-trailing unclaimed exact-identity groups. It also marks the trailing
Model bone range after the retained Animation prefix, merging adjacent indices
into ranges. It writes `CLAIM` for explicit claims or nonexact groups that
would otherwise normalize away.
Translation and rotation between the motion ancestor and indexed bones are
resolved into affected bones. Scale on those intermediary nodes cannot be
represented.

With a semantic origin, channels outside it are ignored and channels on
unrelated nodes below it invalidate that Animation. Without one, bone animation
still imports, but no channel is propelled translation or rotation.
`CUBICSPLINE` interpolation is unsupported. An Animation with unsupported
interpolation or invalid channel bindings is omitted with a warning.

[Worked Animation example](MODEL_GUIDE.md#animations)

## Static Model OBJ

OBJ represents static Model geometry, materials, and positional action points.
Skeletons, selections, Animations, action-point bone binding, and action-point
selection membership have no OBJ representation.

Material names use the [shared face flags](CONVENTIONS.md#materials-and-face-flags):

```text
newmtl <fallback-stem>[__<face-flag>]...[__TRANSVAL_<float>]
newmtl no_tex[__<face-flag>]...[__TRANSVAL_<float>]
```

Material-library and texture paths:

```text
mtllib <material-library-path>...
map_Kd [<map-option>...] <texture-path>
```

Texture identity precedence:

```text
map_Kd
decoded fallback stem
```

Material libraries and texture images are relative to the OBJ. `map_Kd`
accepts `-blendu`, `-blendv`, `-boost`, `-bm`, `-cc`, `-clamp`, `-imfchan`,
`-texres`, `-type`, `-mm`, `-o`, `-s`, and `-t`. Texture paths may contain
spaces after the options. A supplied `map_Kd` path wins over the fallback stem,
including `no_tex`; this combination warns.

MTL `d` stores `1 - TRANSVAL`. A non-opaque value also enables `TRANS` when the
material name does not specify it. Export creates separate materials for
distinct values. `TRANSVAL` remains authoritative and is emitted for every
`TRANS` material. Values inside `[0,1]` are also written as `d`; values outside
that range remain in the material name without a `d` line.

Without `map_Kd`, unknown material-name tokens are errors. With `map_Kd`, they
warn and are ignored because texture identity does not depend on the fallback
stem. Empty tokens and malformed or repeated `TRANSVAL` remain errors.

Explicit `vn` values are authoritative. Without them, smoothing follows the
active OBJ `s` directive. `off` and `0` are flat; `on`, numbered groups, and
named groups smooth shared positions only within the same group. Smoothing is
off before the first `s` directive.

Action-point extension, with either comment spacing:

```text
# arx_action <action-name> <x> <y> <z>
#arx_action <action-name> <x> <y> <z>
```

OBJ positions and normals use Model coordinates rotated 180 degrees around X.
OBJ `v=0` addresses the bottom texture edge while Model `v=0` addresses the
top; conversion flips V without clamping tiled coordinates. No OBJ origin
directive exists.

[Worked OBJ example](MODEL_GUIDE.md#static-obj)

[Back to the Authoring Reference](../AUTHORING_REFERENCE.md)
