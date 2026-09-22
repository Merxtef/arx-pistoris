# Cinematic GLB Authoring Reference

Back to the [Authoring Reference](../AUTHORING_REFERENCE.md). For a practical
workflow, use the [Cinematic Authoring Guide](CINEMATIC_GUIDE.md). Shared
notation and naming terms are defined in
[Shared Authoring Conventions](CONVENTIONS.md).

This reference defines the standalone Cinematic GLB contract. Cinematic GLB
represents exactly one Cinematic. Canonical export uses 100 image pixels per
GLB unit; import derives depth from the authored illustration and camera.

## Layout

```text
arx_cinematic[__FPS_<float>][__END_<frame>]__<label>
`-- arx_illustration__<ordinal>[__SUBDIVISION_<positive-int>]__<label>
    `-- KEY_<frame>[__NONE | __BEZIER][__SPEED_<float>]
            [__FADE_IN | __FADE_OUT | __BLUR]
            [__COLOR_<r>_<g>_<b>][__SECONDARY_<r>_<g>_<b>]
            [__CROSSFADE][__DREAM]__<label>  (optional perspective camera)
        +-- FLASH__RGB_<r>_<g>_<b>__DECAY_<float>__<label>
        |   or FLASH__HIDDEN__<label>
        +-- LIGHT__FALL_IN_<float>__FALL_OUT_<float>__RGB_<r>_<g>_<b>
        |          __INTENSITY_<float>__RANDOM_<float>__<label>
        |   or LIGHT__OFF__<label>
        `-- SOUND__EFFECT__<label> | SOUND__SPEECH__<label>
            `-- PATH_<logical-path>__<label>
```

Exactly one reachable Cinematic root is required. Illustration nodes are its
direct children, and key nodes are direct children of their illustration.
`FLASH`, `LIGHT`, and `SOUND` are each optional and may appear at most once.

Malformed recognized Cinematic nodes fail import. Unrecognized root,
illustration, key, or sound children are ignored with a warning. The final
label is ignored and canonical export always writes one. Import accepts a
missing label on nodes with a fixed grammar and warns. Semantic options are
parsed before label recovery, so a final option remains an option and a
malformed reserved value fails. `PATH` still requires a label because its
logical path may contain `__`. Labels that resemble convention tokens follow
the shared informational-notice rule.

## Root And Timeline

`FPS` defaults to `25`. `END` defaults to the highest key frame. Canonical
export omits either token when it equals that default. Both tokens may appear
in either order and at most once.

The resulting timeline must satisfy the Cinematic API contract: it has at
least two keys, begins at frame zero, uses distinct increasing nonnegative
frames, and has a positive frame rate and end frame. If the earliest authored
key is at a positive frame, GLB import warns and inserts a static visual hold
at frame zero. It copies the first key's illustration, camera, and light state.
The original key stays at its authored frame; sound and non-light effects are
not copied into the hold. The added interval uses speed `1` and no
interpolation; it increases playback duration. Negative and duplicate frames
still fail. The final key may precede `END`.

The root may appear under organizational nodes. Multiple independent reachable
Cinematic roots make the document ambiguous; nesting one reserved Cinematic
root below another is invalid. Root transforms are organizational and do not
change imported Cinematic values.

## Illustrations

`<ordinal>` is a unique sortable unsigned value. Gaps are valid. Import sorts
illustrations by ordinal and remaps their keys to dense Cinematic illustration
indices. `SUBDIVISION` defaults to `1` and must be positive. Native output can
reject a subdivision that is too dense for the image, especially when Dream
or a Dream crossfade uses it; lower the value if native baking reports this.
For Dream crossfades, use matching subdivision grids on both illustrations;
matching image dimensions and `SUBDIVISION` values guarantee this. Different
grids are accepted but can make the second illustration distort unevenly.
Native baking warns when both images are attached.

An illustration has one or more triangle primitives with positions and PBR
materials. Every primitive must reference byte-identical embedded base-color
image data; separate GLB image entries with identical data are accepted.
Buffer-view and data-URI images are accepted; external image URIs are not.
Morph targets are unsupported. `KHR_texture_transform` and its selected texture
coordinate set are honored.

The illustration mesh defines an X/Z chart. UVs map that chart into image
coordinates with the image center as `(0,0)`. Illustration-node translation
and rotation only arrange the authoring workspace. Positive illustration scale
is applied to the mesh and its child key positions before import.

If all usable triangles agree on one affine position-to-UV transform, that
mapping is evaluated everywhere, regardless of mesh subdivision or primitive
boundaries. Otherwise, a point inside geometry uses its containing triangle.
A point outside uses the nearest triangle's unbounded affine mapping and
warns. Overlapping triangles with different results warn; one result is
selected.

Triangles without a usable UV mapping are excluded and warn. If no triangle
has a usable mapping, local X/Z bounds map to the full image; points outside
those bounds extrapolate the same mapping and warn. One collapsed axis maps to
the image center and warns. Both axes collapsed, inconsistent primitive
images, or an unreadable image fail import. Varying mesh Y and non-affine UVs
warn but remain importable. Image and geometry aspect ratios need not agree;
the UV chart determines image coordinates. Material base-color tint and emission
are not baked into the image; nondefault values warn. An image's alpha channel
is imported regardless of the material alpha mode. A non-BLEND mode on an
image with an alpha channel warns because canonical export will use BLEND.

Canonical export embeds PNG or JPEG. Other supported source image formats are
converted to PNG. It writes one centered, rectangular, unlit, double-sided
plane at 100 image pixels per GLB unit with the image's natural aspect ratio.
Illustration nodes are laid out row by row in a grid with at least a 0.5
GLB-unit gap between cells. This placement is organizational and has no
Cinematic meaning.

## Keys And Transforms

`KEY_<frame>` stores the frame. Omitted interpolation is linear; `NONE` and
`BEZIER` select the other two modes. `SPEED` defaults to `1` and accepts any
finite value. It must be positive on every nonterminal key. Arx Libertatis
uses each value when calculating one total duration:

```text
duration = sum(
    (next.frame - current.frame)
  / (FPS * current.SPEED)
)
```

Playback then advances uniformly through the declared frame range:

```text
current_frame = elapsed_seconds * END / duration
```

Consequently, different per-key values affect the total duration but do not
give individual intervals different playback rates. The terminal key's value
is retained but is excluded from the duration. Each option may appear once;
their order is not significant.

The key's position, after illustration scale is applied, maps through the
illustration UV chart to the Cinematic camera position. Its Y distance from
the scaled average Y of the mesh's referenced vertices maps to camera depth
using the image's UV scale along the mesh Z axis and the attached camera's
vertical field of view. With a usable mesh Z extent, scaling the illustration
on X does not change imported key position or depth; Y and Z scale change depth
in proportion to Y/Z. If that vertical extent collapses, depth scale falls
back to the horizontal extent. A camera-less key uses the canonical field of
view; an unrotated Empty is accepted and leveled with a warning.
Canonical export attaches a 4:3 perspective camera with a vertical field of
view of `2 * atan(240 / 350)` radians. Import accepts any finite positive
perspective aspect ratio; its vertical field of view, not aspect ratio, affects
baked depth and light position. Camera aspect is not stored in Cinematic.
The canonical 4:3 view is the center crop of an authored wider camera. At
playback, a wider non-letterboxed viewport reveals the sides again with the
same vertical framing; letterbox keeps the central 4:3 frame.

The key should face down in its illustration-local frame. Import levels an
off-axis key by the shortest rotation from its forward direction to the
illustration-local downward normal (`-Y`); for the opposite direction, it
turns around the key's right axis. The leveled right axis determines roll.
The same effective rotation is applied to
positional `LIGHT` children, and a warning identifies the corrected key.
Position remains the key's projected point on the image, not the tilted
camera's former look target. Thus a tilted camera's GLB preview need not match
game playback. Canonical export always faces down. Import still rejects
orthographic cameras and invalid transforms, including nonpositive scale.
Positive key scale does not change the key's camera position or framing, but it
does affect positional `LIGHT` children. Camera clipping distances do not
affect Cinematic values. glTF animation channels targeting the Cinematic
hierarchy warn and are ignored; `KEY` nodes define the timeline.

Base effects are:

```text
FADE_IN
FADE_OUT
BLUR
```

Omission means no base effect. `FADE_IN` and `FADE_OUT` require both `COLOR`
and `SECONDARY`; each component is a finite number from 0 through 1. Colors
are forbidden without a fade effect. Import quantizes these values to native
8-bit channels. `CROSSFADE` and `DREAM` are independent flags.

## Flash And Light

Omitting `FLASH` means no post effect. `HIDDEN` hides an ongoing flash and is
exclusive with active-flash fields. An active flash requires RGB components in
the 0 to 1 range and a finite `DECAY` value. RGB is quantized to 8-bit channels.

Omitting `LIGHT` leaves the key without a light cue. `LIGHT__OFF` explicitly
stores an off cue. An active light requires every name field shown in the
layout. Its position, including illustration and key scale, is projected onto
the camera's 640 by 480 screen; only that screen position is stored. Canonical
export places the helper 3.5 GLB units in front of the camera. `RGB` uses `1`
for a native channel value of `255`; values above `1` remain valid for brighter
lights. Falloff, intensity, and random intensity are finite floating-point
values.
The light's rotation and scale have no meaning. `FLASH`, `SOUND`, `PATH`, and
`LIGHT__OFF` transforms are presentation-only and do not affect import.

## Sounds And Sidecars

`SOUND__EFFECT` and `SOUND__SPEECH` select separate logical namespaces. Exactly
one valid direct `PATH` child is required; unrelated children warn and are
ignored. The `PATH` child stores a portable logical path without `sfx/` or
`speech/<language>/`. GLB export does not add a physical audio extension; a
suffix already present in the logical path remains part of that path. The final
label makes paths containing `__` unambiguous. Sound and path transforms have
no semantic effect.

Import normalizes and repairs logical paths. Equal paths of the same kind share
one `SoundHandle`; equal effect and speech paths remain distinct. Optional
source references preserve every distinct authored spelling for caller-side
lookup. GLB does not register languages or import audio sidecar bytes.

`exportGlb` writes only the GLB. `exportGlbBundle` also emits attached WAV, MP3,
or Ogg Vorbis encodings referenced by keys. Effect output paths are
`<logical-path>.<actual-extension>`. Speech output paths are
`<logical-path>[<registered-language-name>].<actual-extension>`. The registered
language spelling is preserved. Path-only sounds and unreferenced encodings
emit no file. Bundle export rejects encodings whose generated relative
sidecar paths collide, including collisions between the effect and speech
namespaces.
