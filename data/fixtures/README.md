# Test Fixtures

`catalog.json` is the committed fixture index. Tests and fuzz-seed preparation
use it instead of discovering committed files by directory shape.

`mount/` mirrors an Arx resource mount. It contains generated native resources
and their sidecars at canonical game paths. The current mount is a conversion
fixture, not a complete playable content package.

Loose authoring fixtures are self-contained project directories. The
catalog names the primary file; referenced MTL, texture, and sound sidecars
live relative to that file and are discovered through the format itself:

```text
level/glb/<fixture>/<fixture>.glb
model/glb/<fixture>/<fixture>.glb
model/obj/<fixture>/<fixture>.obj
ambiance/glb/<fixture>/<fixture>.glb
```

Sidecars may be absent when a fixture is not intended to exercise hydration.
Each supported intermediate/loose-format pair must nevertheless include at
least one fixture with every resource type used by that pair.
OBJ material libraries live beside their OBJ owner. Image and audio sidecars
follow the relative paths recorded by their primary format; `textures/` and
`sounds/` are conventions, not required directory names.

Native-carrier JSON uses the flat `json/` directory and keeps the represented
extension in each name, for example `sample.ftl.json` or `level43.dlf.json`.

Level GLBs are authored inputs. Their FTS, LLF, DLF, and texture outputs under
`mount/` are generated from those GLBs through `level:<N>` CLI output.
Model, Animation, and Ambiance native fixtures are generated from their
cataloged loose sources in the same way. Each GLB catalog entry records its
Arx-units-per-GLB-unit scale, and regeneration entries record fixture-specific
operations. The top-level native sidecar manifest lists every generated image
and audio file, including textures, icons, minimaps, and loading screens.

After building the development CLI, regenerate the native mount with:

```text
just regenerate-fixtures
```

The command runs every conversion with `--overwrite` in a clean temporary
mount and requires its complete file set to match the catalog before overlaying
the result onto `mount/`. Repeated output paths from separate conversions are
accepted only when their bytes match. Cataloged native aliases are copied after
conversion and must remain byte-identical to their declared source. The command
does not remove unrelated files such as future scripts or localization.

`../arx/` is an optional ignored mount for locally unpacked game resources.
Corpus tests discover compatible native files there recursively.
