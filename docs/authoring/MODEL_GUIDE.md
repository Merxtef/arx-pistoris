# Model and Animation Authoring Guide

Back to the [Authoring Guide](../AUTHORING_GUIDE.md). For exact syntax and
constraints, use the
[Model and Animation Authoring Reference](MODEL_REFERENCE.md).

This guide is for authors creating rigged Model GLB, Animation sidecars, static
Level previews, or limited static Model OBJ. Converting a small original FTL
with one or two TEA files first provides the most useful working example.

Use Model GLB for complete authoring. OBJ represents only static geometry,
materials, and positional action points.

Keep the final descriptive labels shown in the GLB naming examples. Import can
recover labels from fixed Model and Animation helper names when omitted, but
warns; path-bearing helpers still need their label.

## Coordinates and Origin

Model GLB defaults to 10 Arx units per GLB unit. Create one semantic origin and
place all Model meshes and joints below it:

```text
arx_model_origin__human_base
`-- skeleton
    `-- mesh
```

The origin transform establishes Model coordinate zero and is removed on
import. Without it, scene identity is used and all eligible meshes are
imported, but Model-origin selection membership and propelled Animation motion
cannot be authored.

At least one triangle is required. Unskinned meshes may be mirrored, but their
transforms must otherwise use uniform scale without shear. Joint default poses
and default-pose skin transforms require positive uniform scale without shear.
Scale a complete rig below the semantic origin rather than its mesh object
alone. Multiple mesh objects may use the same rig.

[Exact origin and coordinate rules](MODEL_REFERENCE.md#model-root-and-coordinates)

## Materials

Model materials use the shared texture and face-flag syntax. A referenced
base-color image supplies texture identity, while the material name supplies
flags and remains the fallback.

For a transparent metal blade:

```text
sword_blade__TRANS__METAL__TRANSVAL_0.25
```

Use `no_tex` for untextured geometry. `TRANSVAL` preserves the native
transparency value.

[Exact Model material rules](MODEL_REFERENCE.md#model-materials)

## Skeleton, Actions, and Selections

Joint node names begin with exact zero-based indices. Every parent must precede
its children. A skeleton may have multiple roots. Skin geometry to its intended
bones; when several positive influences exist, only the strongest
survives. An unskinned mesh on or below a joint referenced by an imported skin
follows its nearest such joint rigidly. Without one, it remains unbound. A node
name alone does not establish a joint.

Action points and selection probes are positional nodes below their owning
joint. Position-independent bone settings use detached `arx_bone` records at
scene level.

For a two-bone model with a weapon attachment, blob shadow, and cut probe:

```text
arx_model_origin__human_base
`-- skeleton
    +-- mesh
    `-- 000__root
        `-- 001__chest
            +-- arx_action__weapon_attach__action_0
            |   `-- SELECTION_chest__membership
            `-- arx_selection_probe__cut_torso__cut_point
bones_parent
+-- arx_bone__root
|   `-- SETTINGS__BLOB_SHADOW_0.2__settings
`-- arx_bone__chest
    `-- ORIGIN_OWNER__origin
```

The action point and probe use their world positions relative to the Model
origin. Their nearest ancestor joint owns them. Detached bone records match the
normalized bone names and may stay anywhere at scene level.

Geometry selection membership uses custom `VEC4` color attributes. For a
selection named `chest`, create `_CHEST`; values above `0.5` in the first
component are members. An all-zero attribute preserves an otherwise empty
selection. Use `SELECTION_chest__membership` below a detached bone record,
action point, or the Model origin to include that item.

Selection names are stored in lowercase. Keep them to ASCII letters, digits,
hyphens, and single interior underscores.

[Exact skeleton and selection rules](MODEL_REFERENCE.md#model-bones-and-skin)

## Animations

Set the DCC scene to 24 frames per second. Indexed bones support translation,
rotation, and scale.

Give each GLB animation a plain authoring name. Add a matching scene-level
helper only when it needs a resource path, explicit duration, footsteps,
sound events, or explicit group ownership.

For `human_walk` with one extra duration frame, footsteps at frames 3 and 12,
and one sound on those frames:

```text
arx_model_origin__human_base
`-- skeleton
    `-- mesh
animations_parent
`-- arx_animation__human_walk
    +-- PATH_anim:npc:human_walk__path
    +-- SETTINGS__EXTRA_FRAME__STEPS_3_12__settings
    `-- SOUND__FRAMES_3_12__footsteps
        `-- PATH_sfx/footstep.wav__path
```

The helper name must match the GLB animation name. `path`, `settings`, and
`footsteps` are labels. `EXTRA_FRAME` makes the native duration one frame
longer than the last GLB key. Use an exact frame length only when the animation
requires one.

Group numbers match the zero-based indices in bone node names. To force groups
8 through 10 to identity and protect small authored motion on group 12:

```text
arx_animation__human_walk
`-- GROUPS__VOID_8-10__CLAIM_12__groups
```

`VOID` replaces each marked group's sampled transforms with exact identity.
`CLAIM` preserves sampled motion that is close enough to be treated as
conversion noise and retains the group when its transforms are exact identity.
Canonical export marks unclaimed exact-identity groups and Model bones beyond
the Animation's retained group range as `VOID`, merging adjacent indices into
ranges. Trailing unclaimed identity groups are removed on import and native
bake. `groups` is a label.

For a rigged Model, animate translation and rotation on the Armature object that
contains all imported meshes, root bones, and unbound action or selection-probe
helpers to author propelled motion. Keep `arx_model_origin__*` static; its
transform only places the complete Model in the authoring scene. For an
unrigged Model, animate the semantic origin instead. Bone pose channels remain
independent, including bone 0.

Audio stays external to GLB. For a GLB written as `project/human.glb`, the
example path is read from and exported to:

```text
project/sfx/footstep.wav
```

Without a semantic Model origin, bone animation remains usable but no channel
is interpreted as propelled translation or rotation.

[Exact Animation syntax](MODEL_REFERENCE.md#model-animations)

## Level Preview

A Model preview is a static one-entity Level GLB fragment. For
`model:npc:human_base` labeled `human`:

```text
arx_entity__000__human (mesh: human)
`-- CLASS_model:npc:human_base__human
```

The mesh is attached directly to the entity root. The preview contains
materials and textures but no rig, action points, selections, or Animations. It
uses Level coordinates and the Level default of 100 Arx units per GLB unit.

Create it with `--as-level-preview`. An unresolved class path remains visible as
`<class-path>` and must be replaced before importing the fragment as a Level.

[Exact preview structure](MODEL_REFERENCE.md#model-level-preview)

## Static OBJ

OBJ is suitable for static items that need geometry, materials, and positional
action points but no rig or selection data.

For a metal blade, reference an MTL from the OBJ:

```text
mtllib sword.mtl
```

Use a material and texture path in `sword.mtl`:

```text
newmtl sword_blade__METAL__NO_SHADOW
map_Kd textures/sword_blade.png
```

The corresponding image lives relative to the OBJ:

```text
textures/sword_blade.png
```

Without `map_Kd`, the decoded fallback stem becomes the texture path and
`no_tex` means no texture. A supplied `map_Kd` path defines the texture.
Combining it with `no_tex` produces a warning.

Add a shield attachment point by editing the OBJ text:

```text
# arx_action SHIELD_ATTACH 0.12 0 0
```

Both `# arx_action` and `#arx_action` are accepted. OBJ normals remain
authoritative when supplied. Otherwise, smoothing follows the active `s`
directive.

[Exact OBJ syntax](MODEL_REFERENCE.md#static-model-obj)

## Inventory Icon

An inventory icon is an optional image beside the authored Model rather than
part of GLB or OBJ. Name it after the Model with `[icon]` before the image
extension:

```text
sword.glb
sword[icon].png
```

PNG, JPEG, BMP, and TGA are accepted. The CLI discovers the adjacent icon
automatically; `--input-icon` selects an explicit image. GLB and OBJ outputs
write a PNG at 32 pixels per inventory slot. Loose FTL output writes BMP, while
game-layout FTL output writes PNG plus the BMP compatibility copy required by
stable Libertatis releases. By default the image determines its footprint, up
to three slots on its longest axis. Use
`--icon-slots <WIDTH> <HEIGHT>` to set either axis to one through three slots or
`-` to derive it. Content keeps its aspect ratio and is centered by default.
Use `--icon-layout` to place it at a corner or stretch it to fill the footprint.
RGB BMP input treats exact black as transparent during rendering. See the
[CLI Guide](../CLI.md#model-and-animation-workflows) for game-layout placement
and conversion details.

## Export Checklist

- Export Model GLB at 24 FPS when it contains Animations.
- Preserve object, material, bone, and Animation names.
- Export skinning, custom color attributes, hierarchy, and transforms.
- Keep the Model mesh and joints below the semantic origin when one is used.
- Keep `bones_parent` and `animations_parent` as scene-level organizational
  nodes.
- Keep Animation audio paths relative to the GLB and provide the files beside
  it at those paths.
- Use OBJ only when its deliberate losses are acceptable.

[Back to the Authoring Guide](../AUTHORING_GUIDE.md)
