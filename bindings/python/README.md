# Pistoris

Python 3.12+ bindings for editable Arx Fatalis resources and native carriers.

## Install

Building the native extension from source requires CMake, Ninja, Clang with
C++20 support, and the development headers matching the selected CPython 3.12+
interpreter. A virtual environment supplies Python packages but does not
supply those headers; on Linux, install the appropriate `python3-devel` or
versioned equivalent from the system package manager.

From the repository root:

```text
python -m pip install .
```

To build a wheel without installing it:

```text
python -m pip wheel .
```

Released wheels use CPython's 3.12 stable ABI. Each wheel is specific to its
operating system and architecture but can be installed by compatible CPython
3.12 and newer runtimes on that platform.

The distribution and import package are both named `pistoris`. The five main
editing classes are available at the package root and through resource
modules:

```python
pistoris.Model is pistoris.model.Model
pistoris.Animation is pistoris.animation.Animation
```

Resource-specific records, collections, enums, and conversion outputs live in
`pistoris.model`, `pistoris.animation`, `pistoris.ambiance`,
`pistoris.cinematic`, and `pistoris.level`. Native carriers remain under
`pistoris.native`.

## Resources

### Conversion Workflow

Semantic resources use distinct carrier and byte entry points:

```python
from pathlib import Path

import pistoris

source = Path("human_male.ftl").read_bytes()
imported = pistoris.Model.from_ftl_bytes(source)
model = imported.model
model.scale(1.25)

output = model.to_ftl_bytes()
Path("scaled.ftl").write_bytes(output.ftl)
for file in output.texture_files:
    path = Path(file.path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(file.encoded_image)
```

`from_ftl` accepts `pistoris.native.ftl.Data`; `from_ftl_bytes` parses binary
input. Binary and encoded-media arguments accept any contiguous, readable,
byte-sized buffer, including `bytes`, `bytearray`, `memoryview`, and NumPy
`uint8` arrays. Outputs remain `bytes`; NumPy is not a dependency.
The same split applies to TEA, AMB, CIN, and Level's FTS/LLF/DLF bundle.
Conversion outputs keep sidecar images and audio explicit instead of writing
to the filesystem.

Native carrier arrays and maps implement `collections.abc.MutableSequence`
and `MutableMapping`. Their concrete wrapper classes are private; use the
standard collection interfaces when annotating code.

### Source And Sidecar Resolution

Import methods return an owned bundle containing the resource and any requested
source references. Source collections are included by default. Set the matching
`include_texture_sources`, `include_sound_sources`, or
`include_illustration_sources` option to `False` when the paths are not needed;
the corresponding property remains an empty tuple.

Model GLB imports also include animations by default. Set
`include_animations=False` to import only the model. `animation_report` is a
conversion report when animations were requested and `None` otherwise.
`include_sound_sources` applies only when animations are included.

The paths are lookup inputs, not files opened by Pistoris. For Model and Level,
texture source paths correspond to textures in collection order. A caller can
resolve and attach them before exporting another format:

```python
source_path = Path("human_male.ftl")
imported = pistoris.Model.from_ftl_bytes(source_path.read_bytes())

for texture, lookup_path in zip(
    imported.model.mesh.textures,
    imported.texture_source_paths,
    strict=True,
):
    if image := resolve_texture(lookup_path):  # Your mount/filesystem resolver.
        texture.encoded_image = image

glb = imported.model.to_glb().glb
```

Animation, Ambiance, and Cinematic source records map a Sound in the imported
resource to its caller-facing lookup path; Model Animation records also identify
the owning Animation. The Sound's logical resource identity remains in the
imported resource. Output bundles work in the other direction: write their main
bytes and returned image or audio files to the desired storage layout.

### Types And Calling Conventions

`pistoris.FaceFlag`, `pistoris.level.LightFlag`, and
`pistoris.level.AnchorFlag` are integer flag enums for their corresponding
value fields. Cinematic sound-kind values live at
`pistoris.cinematic.SoundKind`. Mutually exclusive choices, including portal
shape, zone height mode, path node type, and Cinematic illustration format,
use ordinary enums. Source and sidecar records identify resources by their
semantic path. Dedicated `cinematic.sfx` and `cinematic.speech` references
carry their kind through the collection; mixed Cinematic source and sidecar
records expose `kind` explicitly. Speech encoding records also expose the
language name.

OBJ imports accept either one MTL string or an ordered sequence of
`pistoris.model.ObjMaterialLibrary` records. Use
`pistoris.model.obj_material_library_paths()` to discover the library paths
named by an OBJ before resolving their text.

Record constructors use keyword arguments. Payload arguments to conversions
remain positional; conversion options are keyword-only:

```python
vertex = pistoris.model.Vertex(position=pistoris.math.Vector3(1, 2, 3))
imported = pistoris.Model.from_glb(data, arx_units_per_glb_unit=43)
model = imported.model
```

### Live References And Collections

Semantic resources expose relationships as names, paths, or live references.
Optional relationships use `None`; low-level native carriers preserve their
format-specific indices and numeric sentinel values.
`pistoris.level.ZoneAmbiance` identifies a zone's Ambiance and its maximum
volume as a percentage, with `100` as the default.

Related state is grouped by domain. Models expose `mesh`, `skeleton`, and the
special `origin` point; levels expose `mesh` and `nav_surface`.
Collection-specific operations live
with the data they affect, such as `model.mesh.validate()`,
`model.mesh.weld_vertices()`, `level.mesh.weld_vertices()`,
`level.nav_surface.generate()`, and
`animation.sounds.compact()`. The same rule places portal repair on
`level.portals`, room-distance operations on `level.room_distances`, anchor
generation on `level.anchors`, and Ambiance timeline trimming on
`ambiance.tracks`. Atomic animation timeline replacement lives on
`animation.keyframes`.

Positional resource collections are live Python sequences. Use `append()`,
`extend()`, indexed assignment, `del`, and `clear()` where the resource can
preserve its invariants. As with built-in lists, `append()` and `extend()`
return `None`. Collections omit operations that cannot preserve coherent
references. An element reference keeps its resource alive and field assignment
writes through the resource's validated editing API:

```python
model.mesh.textures.append(pistoris.Texture(path="textures/armor"))
texture = model.mesh.textures[0]
texture.path = "textures/armor"
detached = texture.copy()
```

Removing an earlier element updates references to later elements. A reference
raises `ReferenceError` after its element is removed or an operation
structurally rebuilds its collection. Structural operations include collection
replacement, compaction, welding, and generation. `copy()` returns an
independent snapshot. Editable snapshots can be appended or assigned back to
the matching collection; inspection-only snapshots remain read-only values.
Positional collections implement `collections.abc.Sequence`, including
slicing, reverse iteration, membership, `index()`, and `count()`. Slices return
lists of live references.

Nested records on live references are live views as well:

```python
model.mesh.faces[0].corners[0].u = 0.5
animation.keyframes[0].group_transforms[0].scale = pistoris.math.Vector3(1, 1, 1)
ambiance.tracks[0].keys[0].volume.first = 0.8
cinematic.keyframes[0].light.intensity = 1.25
```

The same reference types bridge detached records with aggregate fields:

```python
face = pistoris.model.Face()
face.corners[0].u = 0.5
frame = pistoris.animation.Frame(group_transforms=[pistoris.animation.GroupTransform()])
frame.group_transforms[0].scale = pistoris.math.Vector3(1, 1, 1)
```

Replacing a detached aggregate property invalidates references obtained from
its previous value. Editing an existing element preserves its reference.

### Detached Records

Detached semantic records do not expose resource-local indices. A model
vertex names its bone; a face owns its detached corner vertices and names its
texture; level portals name their rooms; anchor connections name both
anchors. Adding or assigning a record resolves those relationships against
the destination resource and raises `KeyError` when a required name or path
is missing. Adding a detached face creates its three owned vertices; weld the
mesh afterwards when independently authored faces should share vertices.

Fixed nested collections support indexed replacement but not insertion or
deletion. Their `copy()` methods return detached records. Dynamic detached
aggregates also accept whole-property assignment when their size must change.

Mutable detached authoring records, sidecar records, and small inspection
records have structural equality, are unhashable, and use bounded
representations that summarize encoded media and aggregate fields. Immutable
identifier and value types such as `Selection`, vectors, colors, and bounds
have structural equality and are hashable. Conversion and import bundles
retain identity equality and hashing. Their bounded representations summarize
sidecars and reports without expanding resource objects or payloads.

### Identifier Collections

Identifier-addressed collections use string keys rather than exposing their
internal order. `add()` returns the new live reference. Lookup and deletion use
the same normalized name or path:

```python
selection = model.selections.add(pistoris.model.Selection(name="hands"))
same_selection = model.selections["hands"]
model.skeleton.bones[0].selections.add(selection)
del model.selections["hands"]

effect = cinematic.sfx.add("effects/door")
same_effect = cinematic.sfx["effects/door"]
del cinematic.sfx["effects/door"]
```

Selections and languages use unique names; Cinematic sound paths are unique
within their SFX or speech collection. Missing semantic identifiers raise
`KeyError`; lookup never creates an element. Membership accepts either a
semantic key or a compatible live reference. Iteration yields live references.
Internal indices may change when elements are removed, but live references
continue to identify the same logical element until that element itself is
removed or its collection is rebuilt.

### Paths And Math

Logical resource paths and selectors live under `pistoris.paths`. Model and
Animation families use `ModelType` and `AnimationType` string enums instead of
open-ended type strings; path records and inverse helpers preserve the typed
family value.

Mathematical values live under `pistoris.math`. `Vector2`, `Vector3`, `Angle`,
`Color3`, `Quat`, `Rect`, `Aabb`,
`pistoris.level.PlayerSpawn`, and `pistoris.level.ZoneAmbiance` are immutable
values with structural equality and useful representations. Vector-like values
are iterable and indexable. Replace a containing field to change one of these
values. `pistoris.model.Origin` is a mutable detached aggregate because its bone
and selection membership are authored together.

Direct semantic state uses writable properties. Optional media properties
return `None` when no image or audio is attached and accept `None` to clear it.
Empty byte payloads are rejected; use `None` for absence. Output sidecar records
always represent an actual file and keep their media as `bytes`.

### Conversion Outputs

Conversion sidecars use fixed read-only sequences of immutable output records.
Indexing and slicing a sidecar sequence do not copy its encoded media; reading
a `bytes` property performs the Python copy. Small source-path and
source-reference collections remain owned tuples. Option structs are
represented by keyword arguments or small owned records instead of exposing
C++-specific call shapes.

### Model

Selection membership belongs to the selected member. Use
`vertex.selections`, `bone.selections`, `action_point.selections`, or
`model.origin.selections` as mutable sets of live `SelectionRef`
objects. `SelectionRef.vertices`, `bones`, and `action_points` provide the
reverse read-only view. Detached `Selection` values contain only an immutable,
normalized name and are hashable. On detached model records, `selections` is a
live mutable set of those values; editing that set updates the record without
copying the record or its other fields. `SelectionRef.copy()` returns this
name-only value.

An attached selection's optional `leading_vertex` is a separate live child
reference. Assign `SelectionLeadingVertex(position=..., bone=...)` to create or
replace it, or `None` to remove it. Clearing the leading vertex invalidates its
existing reference without affecting the selection reference. Use
`SelectionLeadingVertexRef.copy()` when an independent leading-vertex value is
needed. Replacing or clearing a skeleton preserves positions and selection
memberships but unbinds vertices, the origin, action points, and leading
vertices from the old bones.
`model.skeleton.infer_selection_memberships()` replaces bone selection
memberships using geometry owned directly by each bone.

#### Inventory Icon

Inventory icons use immutable value records because they combine encoded media
with layout metadata. `model.inventory_icon` is an always-present operation
facet: use `copy()` to inspect its optional value, `set()` or `clear()` to
replace it, and `render()` for encoded output. Inventory-icon width and height
default to `None`: with neither value specified, the stored footprint comes
from the image; with one specified, the other follows the source aspect ratio.
Rendering uses the stored footprint when both dimensions are `None`, derives
one missing dimension when the other is supplied, and accepts
`ImageFormat.PNG`, `ImageFormat.BMP`, or `ImageFormat.TGA`. JPEG is a valid
source image but not a render target because rendered icons can contain
transparency.

### Level

Assign a detached `PlayerSpawn` or `None` through `level.player_spawn`; a
present spawn is exposed as a live reference with writable position and
rotation.

`level.minimap` and `level.loading_screen` are always-present operation facets;
their media properties or `copy()` results use `None` when no image is stored.
Use `level.minimap.set()`, `set_from_projection()`, or `clear()` to replace its
image and placement, and `render()` or `generate()` for derived output. Use the
loading-screen `encoded_image` property or `clear()` to replace it and
`render()` to select original, normal, or fullscreen layout. Both renderers
accept PNG, BMP, or TGA output. `level.minimap.render()` returns the effective
projection offset with the encoded image.

Level face corners always expose a `Color3`. Unauthored lighting reads as the
neutral default `Color3(0.5, 0.5, 0.5)`, and
`level.mesh.reset_corner_colors()` restores that value for every corner.
Use `level.mesh.generate_static_lighting()` to replace corner colors from the
current geometry and lights.

Level room distances expose one value for every unordered pair of distinct
rooms. Unavailable pairs read as `distance=-1` with both portal references set
to `None`. Access a pair with live room references as
`level.room_distances[a, b]`, assign a complete `RoomDistance` to update it
atomically, call `reset()` on one reference, or call
`level.room_distances.reset()` to reset all pairs. Pair references follow both
rooms across unrelated insertions and removals and become invalid only when
one of those rooms is removed.

### Animation

`Animation.groups` is the live sequence of skeletal group identities. Assign
`group.claimed` to change ownership, inspect `group.is_void`, and call
`group.make_void()` for the destructive reset that also clears the claimed
state. Changing keyframe group topology invalidates saved group references.

### Ambiance

`Ambiance.master_track` is a live track reference and reads as `None` only
when the ambiance has no tracks. Assign another live track from the same
ambiance to change it. Use `ambiance.tracks.clear()` to return to the empty
state; `None` is never assignable.

Create Ambiance tracks as detached `PannedTrack` or `PositionedTrack` values,
then append or assign them through `ambiance.tracks`. Live `TrackRef.kind`
assignment converts every key while preserving timing, volume, and pitch;
new spatial automation starts at zero. Existing key and common automation
references remain valid. Spatial automation references become invalid because
their channel no longer exists after conversion.

### Cinematic

Cinematic sound effects and speech have separate path collections at
`cinematic.sfx` and `cinematic.speech`. Each sound effect owns one optional
encoding. Each speech reference owns a mutable mapping from registered
language names to encodings. Assigning audio creates or replaces that
encoding without exposing the Cinematic's internal sound and language IDs:

```python
effect = cinematic.sfx.add("effects/door")
effect.encoded_audio = encoded_audio

language = cinematic.languages.add("english")
line = cinematic.speech.add("npcs/guard/greeting")
line.encodings[language.name] = encoded_speech
encoded_speech = line.encodings[language.name]
del line.encodings[language.name]
```

Cinematic keyframes refer to live illustrations and sounds. Add a detached
keyframe together with its illustration explicitly:

```python
cinematic.illustrations.append(pistoris.cinematic.Illustration(path="illustrations/intro"))
illustration = cinematic.illustrations[0]
keyframe = pistoris.cinematic.Keyframe(frame=0)
cinematic.keyframes.add(keyframe, illustration=illustration)
```

`keyframe.light` is `None` when the key has no light. Assign a detached
`pistoris.cinematic.Light` to create or replace it, and assign `None` to remove
it. A present light is exposed as a live `LightRef`.

## Native Carriers

Low-level carriers live under `pistoris.native`. Their collections are mutable
and native text fields use `bytes`:

```python
ftl = pistoris.native.ftl.read(source)
ftl.vertices.append(pistoris.native.ftl.Vertex())
encoded = pistoris.native.ftl.write(ftl)
```

Carrier collection elements are borrowed native records. Field edits write
through, but a structural collection edit may invalidate previously retrieved
elements; reacquire an element after appending, deleting, or replacing items.
This lower-level behavior is intentionally simpler than semantic resource
references.

Fixed native text fields omit their null terminator when read. Assignment
rejects embedded nulls and adds the terminator and remaining padding. Use
`classify_text_encoding`, `latin1_to_utf8`, and `utf8_to_latin1` when working
with native text whose encoding is not already known.

Use carriers for binary or arx-convert JSON compatibility. Use `Model`,
`Animation`, `Level`, `Ambiance`, and `Cinematic` for coherent editing.
FTS binary data has no semantic level identity, but the compatible JSON schema
requires `levelIdx`. Supply it explicitly when exporting; import returns it
beside the carrier:

```python
encoded = pistoris.native.fts.to_json(fts, level=7)
imported = pistoris.native.fts.from_json(encoded)
fts = imported.data
level = imported.level
```

## Errors And Concurrency

Failed operations raise `pistoris.PistorisError`. The exception exposes
`code`, `message`, `detail`, and an optional structured `location` describing
the affected format element, readable `element_name` when applicable, index,
field, resource path, or byte offset.

```python
try:
    model = pistoris.Model.from_glb(data).model
except pistoris.PistorisError as error:
    print(error.message)
    if error.location is not None:
        print(error.location.domain, error.location.element_name)
        if error.location.index is not None:
            print("index", error.location.index)
    raise
```

Long-running parsing, conversion, validation, generation, and rendering calls
release Python's global interpreter lock. Separate resources and native
carriers may be processed concurrently. Concurrent access to the same resource
or carrier requires synchronization by the caller.

The installed package includes `.pyi` type stubs. They are the concise
signature reference for properties, keyword arguments, overloads, enums, and
conversion bundle types; this guide explains the contracts behind those
signatures.
