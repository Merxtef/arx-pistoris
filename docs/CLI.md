# CLI Guide

`arx-pistor` converts Arx Fatalis resources between native formats, OBJ, GLB,
and arx-convert-compatible JSON.

## Command Shape

```text
arx-pistor <inputs...> <output> [options]
```

Options may appear before, between, or after paths. The last positional is the
output; preceding positionals form the input bundle.

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
arx-pistor --help level conventions
```

## Supported Conversions

| Data | Native | OBJ | GLB | Compatible JSON |
| --- | :---: | :---: | :---: | :---: |
| FTL model | read/write | static, bidirectional | bidirectional | bidirectional |
| TEA animation | read/write | - | with an FTL skeleton | bidirectional |
| FTS/DLF/LLF Level | read/write | - | bidirectional | bidirectional |

Level GLB is the coherent current authoring surface. FTL and TEA use legacy
direct conversion paths until Model and Animations editing classes are added.

JSON follows arx-convert schemas. The CLI translates JSON to or from native
carriers before other work; it is not a separate editing model.

## Mounts and Resource Selectors

Mounts are roots of the Arx resource namespace:

```text
arx-pistor --mount "<user Arx directory>" --mount "<user Arx directory>/unpacked" level:1 level1.glb
```

Reads search mounts from left to right. Earlier mounts override later ones,
matching the game overlay model. Native resource output is written through the
first mount. With no explicit `--mount`, `.` is the only mount. Supplying any
`--mount` removes that implicit root; add `--mount .` explicitly when needed.

A valid resource selector and its canonical full resource path are completely
interchangeable identities:

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
```

The item categories map below the native `items/` hierarchy. A model tweak is
an optional final selector component.

List resources visible through active mounts:

```text
arx-pistor --mount data --list-resources all
arx-pistor --mount data --list-resources level
arx-pistor --mount data --list-resources model
```

The result is sorted and contains one canonical selector per stdout line.
`--list-resources` performs only listing; positional conversions are ignored.

Relative raw paths are also resolved through mounts. Fully absolute raw paths
bypass mounts. Logical resource paths use portable `/` separators and reject
traversal, host-reserved characters, and ambiguous drive-relative or
root-relative forms.

## Level Inputs

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

`level:1` is the canonical shorthand:

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
lights, scene objects, texture images, and baked corner colors where supplied.
See the [Authoring Guide](AUTHORING_GUIDE.md) and exact
[Authoring Reference](AUTHORING_REFERENCE.md).

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
project/textures/<texture files>
```

The DLF scene path defaults to `graph/levels/my_level/`. That logical path is
intentionally independent of the loose physical FTS location, and the CLI
reports the distinction.

### Game-Layout Native Bundle

A mount-relative DLF destination writes resources where the game expects them:

```text
arx-pistor --mount output --kind level authored.glb graph/levels/level40/level40.dlf
```

Default output:

```text
graph/levels/level40/level40.dlf
graph/levels/level40/level40.llf
game/graph/levels/level40/fast.fts
graph/obj3d/textures/<texture files>
```

`level:40` selects the same canonical layout:

```text
arx-pistor --mount output --kind level authored.glb level:40
```

Game-layout DLF destinations cannot be absolute because they define locations
inside the mounted resource namespace.

Use `--fts-scene-directory <resource-directory>` to replace the DLF scene
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

A direct native-to-JSON conversion with no Level editing emits only the
supplied carriers. Once a Level operation is requested, output is baked from
Level and normally emits the complete triplet.

### DLF-Only Output

`--dlf-only` applies requested Level operations but emits only player spawn,
entities, fogs, zones, and paths:

```text
arx-pistor --kind level authored.glb level:40 --dlf-only
```

For loose output it writes the sibling `.dlf`; for game-layout output it writes
the requested DLF resource. Quad reconstruction and texture sidecars do not
apply. `--no-quad-reconstruction` and `--skip-texture-export` are accepted and
ignored so a full-bake command can be reused.

## Level Textures

Game-layout input resolves exact texture resource paths through mounts. Loose
FTS input searches a sibling `textures` directory by extensionless filename.
Override either with a flat physical input folder:

```text
--input-texture-folder <path>
```

The search order is `.png`, `.jpg`, `.jpeg`, `.bmp`, then `.tga`.
Matching is case-insensitive. Ambiguous flat filename matches are skipped with
a warning rather than chosen arbitrarily. Missing images warn but retain the
logical Level texture.

Rebase native output references and sidecars together:

```text
--output-texture-folder graph/obj3d/textures
```

Use `--skip-texture-export` to bake references without reading or writing image
sidecars.

GLB embeds PNG and JPEG and converts BMP/TGA to PNG. Native output preserves
compatible encoded bytes. Non-power-of-two images are resized on each axis to
the next power of two and encoded as PNG for engine compatibility.

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

Common examples:

```text
arx-pistor --kind level authored.glb baked.fts --weld-vertices

arx-pistor --kind level authored.glb baked.fts --generate-nav-surface --generate-anchors --connect-anchors

arx-pistor --kind level authored.glb baked.fts --generate-room-distances --generate-static-lighting
```

Generation never occurs merely because data is absent. Use
`arx-pistor --help level` for every option and default.

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

An unchanged FTS can be used directly. If Level operations or GLB input are
present, the CLI bakes an in-memory FTS first.

Inspect navigation or room-distance state:

```text
arx-pistor --kind level authored.glb navigation.glb --generate-nav-surface --generate-anchors --connect-anchors --debug-navigation

arx-pistor --kind level authored.glb distances.glb --generate-room-distances --debug-room-distances
```

Debug GLB is diagnostic output, not an authoring interchange contract.

## Model and Animation Workflows

Bundle an FTL model with TEA animations:

```text
arx-pistor model.ftl idle.tea walk.tea model.glb
```

Importing that GLB to FTL writes one model and sibling TEA files:

```text
arx-pistor --kind model model.glb model.ftl
```

Animation output names are sanitized before writing. Collision resolution is
deterministic and preserves naturally authored numeric names.

Standalone TEA conversion:

```text
arx-pistor anim.tea anim.json
arx-pistor anim.json anim.tea
```

OBJ is static model geometry only:

```text
arx-pistor model.ftl model.obj
arx-pistor model.obj model.ftl
```

Apply model or animation transforms:

```text
arx-pistor model.ftl scaled.ftl --scale 2 --rotate 0 90 0
arx-pistor anim.tea rotated.tea --rotate 0 90 0
```

Model conversions accept the forward-compatible `--glb-*` coordinate options
but do not apply them yet.

### Selection Repair

Some DCC tools rewrite custom selection attributes. Rename imported FTL
selections by position:

```text
arx-pistor --kind model edited.glb repaired.ftl --rename-selections "chest,,leggings,head"
```

Empty CSV fields leave that selection unchanged. The list may be shorter than
the imported selection count but not longer.

### Reference FTL Repair

Merged bodies and equipment may require exact base-model metadata:

```text
arx-pistor --kind model edited.glb repaired.ftl --ftl-reference human_base.ftl --snap-bone-origins-to-reference snap-origins --snap-action-points-to-reference --copy-synthetic-selection-affiliations
```

Available helpers:

- `--autosize-to-reference`
- `--snap-bone-origins-to-reference snap-origins`
- `--snap-bone-origins-to-reference delta-deform`
- `--snap-bone-origins-to-reference hierarchy-deform [N]`
- `--snap-action-points-to-reference`
- `--copy-synthetic-selection-affiliations`

Deformation modes are repair heuristics and can distort models whose topology
or proportions differ from the reference.

## Output Safety

`--overwrite` replaces existing outputs without asking.
`--no-overwrite` skips them without asking. Otherwise the CLI prompts when
interactive.

Each file is completed in a temporary sibling before replacing its
destination. Multi-file bundles apply overwrite policy per file.

Required parent directories are created for real writes. `--dry-run` performs
resolution, reading, conversion, validation, and output-path checks but creates
no files or directories.

Logs, progress, warnings, prompts, and diagnostics go to stderr. Resource
listing, explicit help, and version output go to stdout.
