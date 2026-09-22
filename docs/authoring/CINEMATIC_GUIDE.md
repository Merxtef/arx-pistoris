# Cinematic GLB Authoring Guide

Back to the [Authoring Guide](../AUTHORING_GUIDE.md). For exact names and
validation rules, use the
[Cinematic Authoring Reference](CINEMATIC_REFERENCE.md).

Cinematic GLB turns a CIN-style illustration timeline into an editable scene.
Each illustration is a textured plane. Its timeline keys are camera nodes whose
placement controls the view into that image. Names and optional helper children
carry effects, lights, and sounds.

Export a mounted Cinematic to GLB and bake it back with:

```text
arx-pistor --auto-mount cinematic:intro intro.glb
arx-pistor --auto-mount intro.glb cinematic:intro
```

For a standalone CIN, use its absolute path. A relative CIN path uses the game
resource layout. The [CLI Guide](../CLI.md#cinematic-workflows) describes loose
paths, illustration and audio sidecars, lookup folders, and rebasing options.

## Start From An Export

Exporting an existing Cinematic gives you the canonical hierarchy, names, and
image proportions:

```text
arx_cinematic__cinematic
`-- arx_illustration__0__illustration_0
    +-- KEY_0__key_0
    `-- KEY_100__key_100
```

The illustration image is embedded in the GLB. Pistoris exports a centered
plane at 100 image pixels per GLB unit, so a 640 by 320 image becomes a 6.4 by
3.2 plane. In Blender, the image's top points toward +Y. Multiple illustrations
are arranged apart for editing; moving a whole illustration does not change
its Cinematic position. Import uses the plane's UV mapping rather than requiring
that exact size or shape. Use the image itself for the final appearance:
material tint and emission are not baked into it and warn on import.

The text after the final `__` is an organizational label. Keep it for clear
names in Blender. Import can recover a missing label on the root, illustrations,
`KEY`, `FLASH`, `LIGHT`, and `SOUND` nodes, but warns. A sound `PATH` node still
needs its label.
Use ordinary descriptive labels. Labels that resemble convention options, such
as `FINAL_KEY`, still import but produce an informational notice.

## Position Keys

Keep each `KEY` directly under the illustration it presents. Its camera should
look down at the plane. Move it across the plane to select a point in the image;
its distance from the plane controls how much of the image is visible. Rotation
around the view axis controls roll. Canonical export attaches a camera. Set
Blender's render aspect to the intended display when checking framing; use 4:3
for letterboxed playback. A wider non-letterboxed display shows more at the
sides without changing the vertical framing. Changing the camera's field of
view changes baked depth. A camera-less `KEY`, including an unrotated Empty,
uses the game's field of view. If a `KEY` does not face down, import levels it
with a warning, retains its inferred roll, and rotates child lights with it.
The tilted camera's Blender preview may not match the game: its former look
direction does not move the baked view center.

Moving or rotating an illustration node as a whole does not change the
imported image coordinates: its mesh and keys use the same local coordinate
frame. This lets you arrange several illustrations around the Blender scene
without changing the Cinematic. Positive scale is different: it changes the
plane and its keys together. Horizontal scale alone leaves the selected image
point and view size unchanged; scaling the plane toward the camera or along
its image-vertical axis changes baked camera distance. You can stretch a plane
to match an image's proportions without baking its scale. Positive scale on a
key camera is ignored for that camera, but it moves a child `LIGHT` helper.

Use the key name for timing and interpolation:

```text
KEY_40__BEZIER__SPEED_1.25__close_up
```

Author `KEY_0` to control the opening. If it is missing, import warns and holds
the earliest key's image, camera, and light from frame zero until that key;
its sound and non-light effects still start at the authored frame. Frames must
be distinct.
Linear interpolation and speed `1` are defaults, so canonical names omit them.
Each nonfinal key's `SPEED` is a native timing multiplier, but Arx Libertatis
uses all of those values to calculate one total duration and then advances
uniformly across the complete frame range. Different values therefore do not
make individual intervals play at different rates. Normally leave `SPEED` at
its default and change the root `FPS` to adjust overall playback speed. The
final key's value is retained but has no effect. glTF animation channels do
not create Cinematic keys and warn when they target the Cinematic hierarchy.

## Add Presentation Effects

Put base effects on the `KEY` name:

```text
KEY_0__FADE_IN__COLOR_1_1_1__SECONDARY_0_0_0__key
KEY_40__BLUR__DREAM__key
```

Fade effects require both RGB colors in the `0` to `1` range. `CROSSFADE` and
`DREAM` are independent switches.

Add a `FLASH` helper under a key when it starts a flash or hides an ongoing one:

```text
FLASH__RGB_1_0.86_0.71__DECAY_0.5__flash
FLASH__HIDDEN__flash
```

Add a `LIGHT` helper under a key for a cinematic light. Position it in the
camera's view; its projected screen position is baked, but its depth is not.
Scaling an illustration or key also moves the helper in the imported view.
The name supplies falloff, color, and intensity. No `LIGHT` means no light cue
on that key; `LIGHT__OFF__light` explicitly turns it off. The exact grammar is
listed in the reference.

## Reference Sounds

A key may have one `SOUND` helper with one `PATH` child:

```text
SOUND__EFFECT__sound
`-- PATH_doors/open__path

SOUND__SPEECH__sound
`-- PATH_hero/intro__path
```

These are logical identities, not filenames. Use paths without `sfx/` or
`speech/<language>/`. Canonical export does not add a physical extension, but
keeps one already present in a logical path. Effect and speech paths occupy
separate namespaces, so the same path may be used by both.

Place effect audio beside the GLB as `<path>.<extension>` and localized speech
as `<path>[<language>].<extension>`. The CLI finds matching language files and
writes sidecars only for sounds referenced by timeline keys.

## Distorted Illustration Planes

Rectangular, proportionally sized planes are easiest to edit. They may be
subdivided or split across primitives without changing the mapping when all
usable triangles agree on one UV transform. Import also accepts stretched,
skewed, and non-affine charts: UVs define how key positions map into the image.
A key outside a non-affine mesh extends the nearest triangle's UV mapping and
produces a warning.

A flat plane may be moved toward or away from its cameras in Edit Mode; camera
distance is measured from the plane. If the mesh is warped toward or away from
the camera, import uses the average depth of its vertices and warns, so
Blender's preview is only approximate.

Triangles without a usable UV mapping are excluded from the UV chart and
produce a warning. If no triangle provides a usable mapping, import maps the
mesh's local horizontal bounds to the image and extrapolates that mapping for
keys outside the bounds. A single collapsed axis maps to the image center and
warns. Geometry with no usable extent, a missing embedded image, or a key
with an invalid transform is rejected. Orthographic cameras are not supported.

Re-export always produces a clean rectangular plane at the image's natural
proportions. Distorted authoring geometry is therefore an input convenience,
not roundtrip state.
