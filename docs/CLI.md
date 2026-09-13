# CLI Guide

This guide is for users converting files with `arx-pistor`. Exact GLB and OBJ
authoring conventions are cataloged in the [Authoring Guide](AUTHORING_GUIDE.md)
and [Authoring Reference](AUTHORING_REFERENCE.md).

`arx-pistor` converts Arx Fatalis resources between native formats, OBJ, GLB,
and arx-convert-compatible JSON.

## Command Shape

```text
arx-pistor <inputs...> <output> [options]
```

Options may appear before, between, or after paths. The last positional is the
output; preceding positionals form the input bundle. Use `--` to stop option
parsing when an input or output path begins with `-`.

```text
arx-pistor level.fts level.llf level.dlf level.glb
arx-pistor model.ftl idle.tea walk.tea model.glb
arx-pistor anim.tea anim.json
```

The CLI normally detects formats from the relevant extension and, where
reliable, file contents. GLB can describe different asset kinds, so use an
explicit kind when its role is ambiguous:

```text
arx-pistor --kind level authored.glb baked.fts
arx-pistor --kind model model.glb model.ftl
```

The installed help is the exact reference for options, defaults, and
dependencies:

```text
arx-pistor --help
arx-pistor --help level
arx-pistor --help level debug
arx-pistor --help cli selectors
arx-pistor --help cli formats
```

Help displays one page at a time. Primary topics are `cli`, `level`, `model`,
`animation`, and `ambiance`. Topic names accept unambiguous prefixes for
interactive use; use full names in scripts. An unrecognized word leaves the
current general page selected, while an ambiguous prefix is an error.

## Supported Conversions

| Data | Native | OBJ | GLB | Compatible JSON |
| --- | :---: | :---: | :---: | :---: |
| FTL model | read/write | static, bidirectional | bidirectional | bidirectional |
| TEA animation | read/write | - | with an FTL skeleton | bidirectional |
| FTS/DLF/LLF Level | read/write | - | bidirectional | bidirectional |
| AMB ambiance | read/write | - | bidirectional | bidirectional |

GLB is the primary editable authoring format for Level, Model, and Ambiance
data. Model GLB can also carry Animation sidecars; OBJ supports limited static
Model authoring.

JSON follows arx-convert schemas. A compound suffix such as `.ftl.json`,
`.tea.json`, `.fts.json`, `.dlf.json`, `.llf.json`, or `.amb.json` explicitly
identifies the native payload. Generic `.json` input uses its root `$schema`
or, when absent, recognizable root fields. These hints select a converter;
that converter still validates the document. The CLI translates JSON to or
from native carriers before other work; it is not a separate editing model.

Conversions limited to native formats and compatible JSON can complete without
a Level, Model, Animation, or Ambiance rebuild. GLB or OBJ conversion normally
requires a resource rebuild; debug-output exceptions are identified below.
Options that edit, generate, transform, or rebase resource data also require a
rebuild. Images and audio may be processed independently. The sections below
state where either kind of processing changes paths, encoding, or normalization.

## Mounts and Resource Selectors

Mounts are roots of the Arx resource namespace:

```text
arx-pistor --mount "<user Arx directory>" --mount "<user Arx directory>/unpacked" level:1 level1.glb
```

Reads search mounts from left to right. Earlier mounts override later ones,
matching the game overlay model. `--auto-mount` appends the platform's standard
Arx Libertatis resource folder followed by its `unpacked/` folder. Explicit
mounts remain higher priority. Without an explicit `--mount`, reads begin with
`.`; `--auto-mount` retains it ahead of the standard folders. Supplying any
explicit mount replaces that implicit root, so add `--mount .` when the current
directory should remain in an explicit mount list. The standard root is
`Saved Games/Arx Libertatis` on Windows.
On Linux it is `$XDG_DATA_HOME/arx` when `XDG_DATA_HOME` is absolute, otherwise
`$HOME/.local/share/arx`; automatic mounts are unavailable if neither is
absolute.

Relative loose outputs use `.` independently of the read mounts. With
`--auto-mount`, a game-layout destination such as `level:40` or a relative DLF,
FTL, TEA, or AMB path instead uses the standard game resource folder. Set an
explicit output root with:

```text
--write-mount <folder>
```

`--write-mount` overrides both automatic choices regardless of option order. The
option does not add a read mount; the folder is searched only when it is already
included through `--mount` or `--auto-mount`. It may be a new directory; an
actual write creates it, while dry-run does not. Absolute outputs bypass the
write mount.

A valid resource selector aliases one registered full resource path. Either
spelling selects the same mounted resource; selectors do not cover every valid
logical resource path:

```text
level:<N>
model:<type>:<name>[:<tweak>]
anim:<npc|fix_inter>:<name>
cinematic:<name>
ambiance:<name>
```

Each selector identifies exactly one resource. `anim:npc:human_walk` does not
implicitly load `human_walk2`, `human_walk3`, or the rest of a runtime family.

Supported model types:

```text
npc
fix_inter
system
armor
jewelry
magic
movable
provisions
quest_item
special
weapons
ui-runes
ui-menus
editor
```

The item categories map below the native `items/` hierarchy. `ui-runes`,
`ui-menus`, and `editor` map to the corresponding interface and editor Model
directories. Only interactive and item Models accept the optional tweak
component.

List resources visible through active mounts:

```text
arx-pistor --mount data --list-resources all
arx-pistor --mount data --list-resources level
arx-pistor --mount data --list-resources model
```

The result is naturally sorted and contains one canonical selector per stdout line.
The resource kind accepts any unambiguous prefix, such as `mod` for `model`.
`--list-resources` can be appended to an existing command without removing its
conversion arguments. Conversion positionals are ignored and no conversion is
performed. Other supplied options must still parse correctly. Discovery
options such as `--mount` and `--log-level` still apply. `--help` and
`--version` take precedence over resource listing.

Relative raw paths are also resolved through mounts. Fully absolute raw paths
bypass mounts. Logical resource paths use portable `/` separators and reject
traversal, host-reserved characters, and ambiguous drive-relative or
root-relative forms.

## Level Inputs

For the contents of an authored Level GLB, see the
[Level authoring guide](authoring/LEVEL_GUIDE.md) or the exact
[Level naming reference](authoring/LEVEL_REFERENCE.md).

### Loose FTS Input

FTS is mandatory. Supply zero or one LLF and zero or one DLF explicitly in
either order:

```text
arx-pistor --kind level level.fts level.glb
arx-pistor --kind level level.fts level.llf level.glb
arx-pistor --kind level level.fts level.dlf level.llf level.glb
```

Loose input does not discover DLF or LLF siblings. It is suitable for files
outside a complete game layout.

### Game-Layout DLF Input

A DLF primary input selects the game-resource layout:

```text
arx-pistor --mount unpacked graph/levels/level1/level1.dlf level1.glb
```

The path must be mount-relative. The CLI reads the DLF scene path, discovers
the mandatory FTS as the engine does, and looks for an optional same-stem LLF
beside the DLF. Explicit companion positionals are not accepted in this form.

`level:1` is the canonical selector:

```text
arx-pistor --mount unpacked level:1 level1.glb
```

### Level JSON Input

The primary file must be named `level<N>.fts.json`; the number supplies the
Level identity. Optional DLF and LLF JSON files are supplied explicitly in
either order. Their payload schemas determine which native format they
contain.

```text
arx-pistor level40.fts.json scene-data.json lighting.json level40.glb
```

JSON input performs no sibling discovery.

### Level GLB Input

Use `--kind level` when a GLB is not otherwise unambiguous:

```text
arx-pistor --kind level authored.glb canonical.glb
arx-pistor --kind level authored.glb native.fts
```

GLB-to-GLB imports into Level and emits the canonical authoring structure. It
is not a byte-preserving rewrite.

## Level Outputs

### GLB

```text
arx-pistor level.fts level.glb
arx-pistor level.fts level.llf level.dlf level.glb
```

The result contains editable rooms, portals, navigation support, anchors,
lights, scene objects, texture images, the minimap where supplied, and baked
corner colors. The loading screen is written beside the GLB as
`<name>[loading].png`. See the
[Level authoring guide](authoring/LEVEL_GUIDE.md) and exact
[Level naming reference](authoring/LEVEL_REFERENCE.md).

Attach static previews for referenced entity classes:

```text
arx-pistor --mount unpacked level:1 level.glb --load-previews
```

The option loads each distinct entity class and its textures through active
mounts, then reuses one preview mesh for every matching entity instance.
Missing or invalid Models and textures warn without stopping Level export.
`--load-previews` does not apply to diagnostic GLB output.

### Loose Native Bundle

An FTS destination writes an explicitly placed bundle:

```text
arx-pistor --kind level authored.glb project/my_level.fts
```

Default output:

```text
project/my_level.fts
project/my_level.llf
project/my_level.dlf
project/<logical texture resource paths>
project/my_level[map][offset_<x>_<y>].<ext> when present; zero offset omits the suffix
project/my_level[loading].png             when present
```

The DLF scene path defaults to `graph/levels/my_level/`. That logical path is
intentionally independent of the loose physical FTS location, and the CLI
reports the distinction.

### Game-Layout Native Bundle

A mount-relative DLF destination selects game layout for the native bundle:

```text
arx-pistor --write-mount output --kind level authored.glb graph/levels/level40/level40.dlf
```

Default output:

```text
graph/levels/level40/level40.dlf
graph/levels/level40/level40.llf
game/graph/levels/level40/fast.fts
graph/obj3d/textures/<texture files>
graph/levels/level40/map.<image suffix>     when present
graph/levels/level40/loading.<image suffix> when present
```

`level:40` selects the same canonical layout:

```text
arx-pistor --write-mount output --kind level authored.glb level:40
```

Only a canonical numbered DLF path or `level:` selector can derive the game's
minimap and loading-screen destinations. A noncanonical relative DLF still uses
game layout for the native bundle, but writes any minimap or loading screen with
the loose sibling names described below.

Game-layout DLF destinations cannot be absolute because they define locations
inside the mounted resource namespace.

Use `--dlf-scene-directory <resource-directory>` to replace the DLF scene
directory. The value is a nonempty portable relative path; it need not start
with `graph/`. In a loose bundle it changes only the path stored in DLF. In a
game-layout bundle it also changes the physical `game/<directory>/fast.fts`
destination.

### Level JSON

Request Level JSON output with the central name `level<N>.fts.json`. The CLI
uses that number and writes explicit native-format suffixes:

```text
level40.fts.json
level40.dlf.json
level40.llf.json
```

Without a Level rebuild, native or JSON output contains only the supplied
native files. A Level rebuild normally produces the complete triplet.

### DLF-Only Output

`--dlf-only` applies requested Level operations but emits only player spawn,
entities, fogs, zones, and paths:

```text
arx-pistor --kind level authored.glb level:40 --dlf-only
```

For loose output it writes the sibling `.dlf`; for game-layout output it writes
the requested DLF resource. Quad reconstruction and texture sidecars do not
apply. Minimap and loading-screen images are omitted.
`--no-quad-reconstruction` and `--skip-texture-export` are accepted and ignored
so a full-bake command can be reused.

## Level Images

Mounted canonical DLF input discovers the game minimap and loading-screen
paths. Loose FTS, Level JSON, and other non-GLB Level inputs use sibling
`<name>[map][offset_<x>_<y>]` and `<name>[loading]` images. Omitting the offset
suffix means `0, 0`; offset values are Arx units. GLB stores the minimap
internally and may use a sibling `<name>[loading]` image.

Plain `[map]` names take priority over offset names. Within each group, minimap
discovery prefers PNG, JPG, JPEG, BMP, then TGA. Invalid minimap candidates warn
and fall through to the next candidate; multiple candidates warn which one was
selected. Invalid or ambiguous minimaps embedded in GLB warn and are omitted.

Mounted native output renders the minimap with the active
`graph/levels/mini_offsets.ini` values and the engine's built-in offset
overrides. Rendering or reprojection overwrites its one-pixel perimeter with
the configured border color. A native write that requires neither rendering
nor reprojection preserves the source image and its existing border.
`--minimap-border-color <R> <G> <B>`
sets that color and requests rendering. Generated minimaps default to their
halo color while the halo is enabled; other rendered minimaps default to white.
It renders a normal loading screen at `320 x 390`; level 10 uses the fullscreen
`640 x 480` layout. A minimap written for a level above 31 warns because the
game may not display it.

Native output preserves supported image encodings when no resizing or
reprojection is required. Images that require either operation are PNG.
Loose minimap filenames retain their Arx-unit projection offset, while loose
loading screens are PNG. Loose and GLB output neither adds nor removes a
minimap border. GLB embeds the minimap but keeps the loading screen beside the
GLB. `--dlf-only` omits both images.

## Level Textures

Texture loading has a format-reference pass followed by a game-resource pass.
For loose FTS and JSON input, the first pass resolves the original native
texture references relative to the input parent. For GLB input it resolves
external image URIs relative to the input parent. An explicit folder replaces
the parent only for this first pass:

```text
--input-texture-folder <path>
```

Mount-relative DLF input is already in the game layout and skips the first
pass. The second pass resolves every texture still missing image bytes from its
normalized logical resource path through active mounts. It is never affected
by `--input-texture-folder`.

Native references and logical resource paths use the game's extension search
order: `.png`, `.jpg`, `.jpeg`, `.bmp`, then `.tga`. The first existing file is
authoritative. GLB external URIs use their exact suffix. Missing images warn
only after both passes and remain valid path-only Level textures.

Rebase texture resource identities before output conversion:

```text
--rebase-textures graph/obj3d/textures
```

An explicit `--rebase-textures` applies before native, JSON, or GLB output.
During a Level rebuild, a `level:` input written to a loose output uses the
local `textures/` directory, while loose input written to a `level:` output uses
`graph/obj3d/textures`. Game-to-game and loose-to-loose conversions preserve
texture identities. Raw game-resource paths also preserve them; use a selector
to request the automatic layout or rebase explicitly. Selectors alone do not
require a Level rebuild, so native output remains unchanged. For a GLB texture
without embedded image bytes, its logical identity becomes the external URI.
Its preserved physical suffix is used when known; otherwise `.png` is assumed
with a warning.

Game-layout DLF output writes texture sidecars at their logical resource paths.
Loose FTS and all JSON output write those same paths relative to the output
file's parent. Without a Level rebuild, output copies files found from the
native FTS references. DLF-only output has no FTS texture references and writes
no texture sidecars.

Use `--skip-texture-export` to bake references without reading or writing image
sidecars.

GLB embeds PNG and JPEG and converts BMP/TGA to PNG. During a Level rebuild,
native baking preserves compatible encoded bytes and resizes non-power-of-two
images on each axis to the next power of two before encoding them as PNG for
engine compatibility.

If one room and texture would exceed the engine's 65535 render-corner limit,
the native bake creates collision-free `_N` texture aliases and emits one
warning listing the required identical sidecars.

## Level Operations

Requested editing and generation operations run in this order:

1. Vertex welding.
2. Navigation-surface generation.
3. Navigation-island pruning.
4. Anchor generation.
5. Anchor connection generation.
6. Anchor-island pruning.
7. Room-distance generation.
8. Static-lighting generation.
9. Minimap generation.

Common examples:

```text
arx-pistor --kind level authored.glb baked.fts --weld-vertices

arx-pistor --kind level authored.glb baked.fts --gen-nav-surface --gen-anchors --connect-anchors

arx-pistor --kind level authored.glb baked.fts --gen-room-distances --gen-static-lighting

arx-pistor --kind level authored.glb baked.fts --gen-minimap
```

Generation never occurs merely because data is absent. Use
`arx-pistor --help level` for every option and default.

`--gen-minimap` replaces any loaded minimap with an internal `640 x 640`
image sampled from the full Level domain. Mounted native output then projects
and crops that image from the minimum referenced X and maximum referenced Z,
using `mini_offsets`, so the written map can have different dimensions. The
default palette uses dark blue foreground, lighter blue background, light brown
water, cyan lava, and a five-pixel white halo. Related options set each color,
provide optional sampler images, or change the halo. Color channels use the
`[0, 1]` range. Sampler images are stretched to `640 x 640` and multiplied by
their active colors; use white to preserve an image's colors unchanged.
Downward-facing surfaces and slopes above 85 degrees are ignored. The highest
remaining surface determines whether a pixel is foreground, water, or lava.
Mounted native output uses the halo color for its minimap border unless
`--minimap-border-color` overrides it. Disabling the halo restores the white
border default.

Full native output reconstructs compatible FTS quads by default. Use
`--no-quad-reconstruction` to keep surviving clipped triangles separate.

Every DLF and LLF native or JSON write records the current modification time
and `arx-pistoris` signer. `--sign-level <text>` changes the signer to
`arx-pistoris/<text>`. If output contains neither DLF nor LLF, the option has
no effect.

Native writers compress by default. Use `--no-compression` for raw output.

## Level GLB Coordinates

Level stores native Arx coordinates with -Y up. GLB uses +Y up and defaults to
100 Arx units per GLB unit:

```text
--glb-arx-units-per-unit 100
--glb-offset 0 0 0
```

The explicit unit ratio must be inside inclusive `[1,1000]`. An explicit
offset is an Arx-space origin and suppresses automatic placement. Without it,
GLB import chooses a 100-unit-aligned X/Z offset that places geometry inside
the game map.

Use the same explicit offset and ratio for an exact inverse. These options also
apply to Level debug GLB output.

## Level Debug Output

Export native cell slicing:

```text
arx-pistor level.fts cells.glb --debug-cells
```

An FTS with no requested Level operation can be read as-is. Level operations or
GLB input rebuild the FTS before producing debug output.

Inspect navigation or room-distance state:

```text
arx-pistor --kind level authored.glb navigation.glb --gen-nav-surface --gen-anchors --connect-anchors --debug-navigation

arx-pistor --kind level authored.glb distances.glb --gen-room-distances --debug-room-distances
```

Debug GLB is diagnostic output, not an authoring interchange contract.

## Model and Animation Workflows

For Model GLB, Animation helpers, and static OBJ conventions, see the
[Model and Animation authoring guide](authoring/MODEL_GUIDE.md) or the exact
[Model and Animation naming reference](authoring/MODEL_REFERENCE.md).

Bundle an FTL model with TEA animations:

```text
arx-pistor model.ftl idle.tea walk.tea model.glb
```

Importing that GLB to FTL writes one model and sibling TEA files:

```text
arx-pistor --kind model model.glb model.ftl
```

Export a Model as a static entity ready for placement in a Level GLB:

```text
arx-pistor model:npc:human_base human.glb --as-level-preview
arx-pistor C:/models/human_base.ftl human.glb --as-level-preview --preview-class-path model:npc:human_base
```

The preview uses the Level default of 100 Arx units per GLB unit and ignores
Animation sidecars, bones, selections, and action points. A normal
mount-relative FTL input supplies its class path automatically; a tweak uses the
base Model class. Loose absolute FTL, GLB, OBJ, and JSON inputs require
`--preview-class-path` for a usable class helper; without it the preview contains
the visible `<class-path>` placeholder.
The entity label comes from the input filename after known extensions and an
exact final `_base` suffix are removed; an empty result becomes `asset`.

A mount-relative FTL path or Model selector uses the game resource layout. It
skips format-relative lookup and resolves logical Model texture paths through
mounts. An absolute FTL path is loose: its original native references are
resolved relative to the FTL parent, then unresolved textures fall back to
their logical game resource paths through mounts. OBJ and GLB use the same
two-pass process, with exact format paths in the first pass. For these loose
inputs, `--input-texture-folder` replaces the input parent for the first pass
only.

OBJ input reads the material libraries declared by `mtllib` through the same
IO service. `map_Kd` paths are resolved relative to the OBJ; the material name
is the texture-identity fallback when `map_Kd` is absent.

Model inventory icons are optional image sidecars. Mounted item FTL input looks
for the engine icon resource `<entity-class>[icon]`. Absolute FTL input and
JSON, OBJ, or GLB input look for `<input-stem>[icon]` beside the input. Both forms
use the game image priority `.png`, `.jpg`, `.jpeg`, `.bmp`, then `.tga`.
For a recognized mounted Model tweak, the entity class and icon belong to its
base Model.
Override discovery with an exact file path:

```text
--input-icon <path>
```

Missing automatic icons are ignored; a missing or invalid explicit icon fails
the conversion. GLB and OBJ outputs, and native or JSON output using Model or
icon editing options, write a retained icon as PNG at 32 pixels per slot.
Loose outputs place it beside the main file as
`<output-stem>[icon].png`. Mounted FTL output writes it at the engine item-icon
path and omits it with a warning when the destination is not an item class. BMP
black pixels use the engine's transparent color-key behavior. Without a Model
rebuild, output preserves the encoded image and uses its detected image
extension.

Control the stored footprint and rendered content with Model route options:

```text
--icon-slots <WIDTH> <HEIGHT>
--icon-layout <LAYOUT=CENTER>
```

Each slot value is one through three or `-` to derive that axis from the source
aspect ratio. Both `-` values retain a native footprint within 3x3 and otherwise
fit the longest axis to three slots. Layout is `center`, `top-left`, `top-right`,
`bottom-left`, `bottom-right`, or `stretch`; unambiguous prefixes are accepted.
Center is the default. Corner layouts preserve aspect ratio, while `stretch`
fills the footprint. These options apply to any Model with an icon and require
a Model rebuild.

An explicit `--rebase-textures` applies before native, JSON, OBJ, or GLB
output. During a Model rebuild, a `model:` input written to a loose output uses
the local `textures/` directory, while loose input written to a `model:` output
uses `graph/obj3d/textures`. Game-to-game, loose-to-loose, and raw game-resource
paths preserve texture identities. Selectors alone do not require a Model
rebuild, so native output remains unchanged. Game-layout FTL output writes
sidecars at logical resource paths through the write mount. Loose FTL and all
JSON output write those paths relative to the output parent. Without a Model
rebuild, output copies files found from the FTL references.
OBJ output writes its MTL beside the OBJ and writes available texture images at
their `map_Kd` paths relative to the OBJ parent. Resource-output collision
handling covers those image sidecars before any OBJ output is written.
`--skip-texture-export` keeps references without reading or writing image
sidecars.

Animation Sounds in Model conversion use the same two-pass lookup. GLB and
loose TEA references are first resolved relative to their owning file, or to
`--input-sound-folder`; unresolved logical paths then fall back to active
mounts. Game-resource TEA inputs skip the first pass. Missing files are
reported after both lookups.

During a Model rebuild, GLB output writes original encoded audio as external
files relative to the GLB. FTL and JSON output write Animation audio as PCM16
WAV under `sfx/`; a logical path already below `sfx/` is not prefixed again.
`--rebase-sounds` changes the logical directory before output, while
`--skip-sound-export` preserves sample references without reading or writing
audio files.

During a Model rebuild, Animations from `anim:` inputs written to loose output
use the local `sounds/` directory for their Sound paths. Loose Animation inputs
written through a `model:` output use `sfx`. Automatic rebasing applies only
when every sound-bearing Animation qualifies for the same transition. A batch
that mixes qualifying and nonqualifying sound-bearing sources preserves every
Sound path and reports one information message. A batch with no qualifying
source is preserved silently. Soundless Animations do not affect this decision.
Explicit `--rebase-sounds` overrides it. Without a Model rebuild, native output
copies available WAV sidecars without automatic rebasing.

For loose FTL output, TEA sidecars are written next to the FTL. An Animation
resource path supplies the filename when available; otherwise the
Animation name does. For game-layout FTL output, a canonical Animation path is
written unchanged. Missing Animation paths are inferred as `npc` for an NPC
Model destination and `fix_inter` otherwise, with one warning per inferred
path.

Model FTL and JSON outputs omit an Animation sidecar only when it has zero
groups and no root motion, footsteps, or referenced Sounds. Timeline length
and otherwise empty keyframes do not make an Animation nonempty. Use
`--allow-empty-animation` to write these sidecars. Standalone Animation output
is unaffected.

Animation output names are sanitized before writing. Collision resolution is
deterministic and preserves naturally authored numeric names.

Standalone TEA conversion:

```text
arx-pistor anim.tea anim.json
arx-pistor anim.json anim.tea
```

Standalone Animation conversion uses the same Sound lookup, rebasing, native
WAV conversion, and sidecar-output rules. During an Animation rebuild, an
`anim:` input written to a loose output uses `sounds/`, while loose input
written to an `anim:` output uses `sfx`. Same-layout and raw game-resource
conversions preserve Sound paths. Without an Animation rebuild, native output
also preserves them.
`--input-sound-folder`, `--rebase-sounds`, and `--skip-sound-export` have the
same meanings as in Model conversion.

OBJ is static model geometry only:

```text
arx-pistor model.ftl model.obj
arx-pistor model.obj model.ftl
```

Apply model or animation transforms:

```text
arx-pistor model.ftl transformed.ftl --scale 2 --rotate 0 90 0 --offset 100 0 0
arx-pistor anim.tea adjusted.tea --scale 2 --rotate 0 90 0
```

Scale is uniform. `--offset` translates Model data in Arx units. Model
transforms run in scale, rotation, then translation order. Attached Animations
receive Model scale and rotation but not Model translation. Standalone
Animation conversion accepts scale and rotation, but not offset.

Model GLB defaults to 10 Arx units per GLB unit. Override the ratio with
`--glb-arx-units-per-unit`. `--glb-offset` applies only to Level GLB
conversion.

### Reference Model Operations

Use a compatible base FTL to copy exact bone positions or selection
memberships after editing a replacement Model:

```text
arx-pistor --kind model edited.glb repaired.ftl --ftl-reference human_base.ftl --snap-bone-origins --copy-bone-selections --copy-action-selections
```

`--ftl-reference` requires at least one of the three reference operations. The
operations may be used independently or combined. Selection copying replaces
the requested membership category: target-only memberships clear and
reference-only selections are not created. Unmatched target action-point
memberships clear. Selection memberships on unmatched reference action points
are omitted with a warning.

Snapping and bone-origin selection copying require equal bone counts and
identical parent topology. Action-point selection copying does not require
matching skeletons. Bone names may differ; mismatches are reported without
preventing the operation. Selections correspond by name. Repeated action points
correspond by name and occurrence order.

Infer bone-origin memberships from directly owned geometry without a reference:

```text
arx-pistor edited.glb inferred.ftl --infer-bone-selections
```

Inference uses a 90% vertex-membership threshold and replaces existing
bone-origin memberships. It cannot be combined with
`--copy-bone-selections`.

## Ambiance Workflows

For the Ambiance GLB hierarchy and automation names, see the
[Ambiance authoring guide](authoring/AMBIANCE_GUIDE.md) or the exact
[Ambiance naming reference](authoring/AMBIANCE_REFERENCE.md).

Convert a native AMB to the Ambiance GLB authoring surface and back:

```text
arx-pistor ambiance.amb ambiance.glb
arx-pistor --kind ambiance ambiance.glb ambiance.amb
```

AMB also converts to and from arx-convert-compatible JSON:

```text
arx-pistor ambiance.amb ambiance.json
arx-pistor ambiance.json ambiance.amb
```

Ambiance GLB defaults to 10 Arx units per GLB unit. Override the ratio with
`--glb-arx-units-per-unit`. `--glb-offset` applies only to Level GLB
conversion.

Embed a static Model reference in GLB output:

```text
arx-pistor ambiance.amb ambiance.glb --reference-model human_base.ftl
```

The path accepts the same primary inputs as Model conversion: FTL or compatible
JSON, OBJ, and GLB. The reference is aligned to its first `view_attach` action
point when present, otherwise to the Model origin. Animation sidecars in a
reference GLB are ignored. The Model is only a visual reference and is not
stored in Ambiance or emitted to AMB.

Ambiance conversion resolves sound sidecars in two passes. Format paths are
first resolved relative to the input or `--input-sound-folder`; unresolved
logical Sound paths then fall back to active mounts. The override affects only
the format-relative pass. Native game-resource input skips that pass.

Use `--trim-to-master` to shorten non-master tracks whose longest nominal
duration exceeds the master's shortest nominal duration. The repair needs
readable audio whenever tracks must be compared. If comparison or trimming
fails, no output is published.

During an Ambiance rebuild, AMB output converts attached WAV, MP3, or Ogg
Vorbis data to PCM16 WAV. Spatial playback is mono; when one stereo source is
also used by a centered panned track, separate stereo and mono files are emitted
with `_N` suffixing. GLB output keeps original encoded bytes as external
sidecars. Loose output writes sidecars relative to the main output. An
`ambiance:` input written to a loose output uses `sounds/`, while loose input
written to an `ambiance:` output uses `sfx/ambiance`. Same-layout and raw
game-resource conversions preserve Sound paths.
Use `--rebase-sounds` to choose another logical directory, or
`--skip-sound-export` to preserve references without writing audio files.
`--trim-to-master` may still read audio for timing when both options are used.
Without an Ambiance rebuild, AMB or JSON output preserves native Sound paths
and copies available sidecars without transcoding them. `--trim-to-master`
requires an Ambiance rebuild even when both endpoints are AMB or JSON.

For reference inputs with format-specific texture lookup,
`--input-texture-folder` replaces the reference Model input parent. Missing
images still fall back to logical game resource paths through active mounts.

## Output Safety

`--overwrite` replaces existing outputs without asking.
`--no-overwrite` skips them without asking. Otherwise the CLI prompts when
interactive.

When independent assets request the same audio or image output, byte-identical
data is written once. If the data differs, the CLI lists every asset and byte
size and asks which candidate to keep. `--keep-first-resource` selects the
first candidate without prompting. `--dry-run` reports differing candidates
without selecting one. A resource sidecar cannot use a path reserved for a
primary asset output.

Each file is completed in a temporary sibling before replacing its
destination. Multi-file bundles apply overwrite policy per file.

Required parent directories are created for real writes. `--dry-run` performs
resolution, reading, conversion, validation, and output-path syntax checks. It
does not inspect existing output targets, apply overwrite policy, or create
files or directories.

Logs, progress, warnings, prompts, and diagnostics go to stderr. Resource
listing, explicit help, and version output go to stdout.

Interactive help, logs, and diagnostics use terminal colors where supported.
Redirected output remains plain; set `NO_COLOR` to disable colors explicitly.
