# Testing and Fuzzing

This guide is for contributors running the local quality gates, fixture
corpora, coverage, and fuzzers.

## Development Gate

Run the ordinary configure, build, and test workflow:

```text
just dev
```

Run individual steps or suites:

```text
just build tests
just test
just run-tests
just run-tests ftl
just run-tests level unit
just run-tests level api
just run-tests level corpus
```

`run-tests` takes an optional doctest suite followed by one of `unit`, `api`,
`corpus`, `c-api`, `cpp-api`, `c-corpus`, or `cpp-corpus`.

Before a release:

```text
just pre-release
```

The release gate checks formatting without modifying files, runs the ordinary
and sanitizer test suites, enforces clang-tidy, builds every fuzz target,
replays its fixture seeds, and installs and smoke-tests the release CLI package.

`sanitize` runs the test suite with ASan and UBSan. `tidy` applies available
clang-tidy fixes; `tidy-check` treats diagnostics as errors without modifying
files. `format` applies clang-format; `format-check` only reports differences.

GitHub pull requests and pushes to `main` run development, sanitizer, and
package checks on Windows and Linux. Formatting, clang-tidy, and fuzz builds
run on Linux. Branch protection should require the `CI / Gate` check.

## Coverage

Coverage requires `llvm-profdata` and `llvm-cov`:

```text
just coverage
just coverage src/level/level.cpp
```

The complete command prints a summary and writes an HTML report below
`build-coverage/html/`.

## Test Data

`data/fixtures/catalog.json` indexes committed fixtures and their related
representations. Native files and sidecars use canonical game paths below the
fixture mount; loose authoring files remain grouped by resource:

```text
data/fixtures/mount/
data/fixtures/level/glb/<fixture>/
data/fixtures/model/glb/<fixture>/
data/fixtures/model/obj/<fixture>/
data/fixtures/ambiance/glb/<fixture>/
data/fixtures/cinematic/<fixture>/
data/fixtures/json/
```

Their sources and derivative families are recorded in
[Test Data Attribution](../data/Attribution.md). Cataloged native assets under
`mount/` are regenerated from their loose sources.

Original Arx Fatalis resources cannot be committed. Optional corpus tests
recursively discover compatible native files in an unpacked resource mount:

```text
data/arx/
```

Readers and independent native roundtrips test every matching file that is
present, including CIN. Level bundle tests use only complete canonical
FTS/DLF/LLF triplets; an incomplete optional game triplet is ignored. Separate
readiness tests require at least one cataloged Level, Model, Animation,
Ambiance, and Cinematic fixture.
Loose sidecars are resolved relative to their primary format file. Missing
sidecars are allowed for fixtures that do not test sidecar loading, while
aggregate corpus checks require loading coverage for each resource type used
by a format pair. Every emitted image and audio sidecar is validated through
the public binary API before it is loaded for the roundtrip.
Fixture projects keep OBJ material libraries beside their OBJ. Image and audio
sidecars follow the paths recorded by their primary format; project-local
`textures/` and `sounds/` directories are only conventions.

GLB catalog entries include the scale used by their source project. Corpus
tests apply that scale on import and export, and compare authored Model bounds
with their generated native counterpart. Fixture-specific native generation
settings also live in the catalog. Its native sidecar manifest records every
generated image and audio file; corpus tests validate those files through the
public binary API.

To rebuild cataloged native fixtures with the development CLI:

```text
just regenerate-fixtures
```

Regeneration uses `--overwrite` for every conversion, requires the complete
temporary file set to match the catalog, and then overlays it onto the
committed mount without deleting unrelated files. Separate conversions may
share an output path only when the bytes match. Declared native aliases are
also checked against their source byte-for-byte.

## Fuzzing

Build all libFuzzer targets:

```text
just fuzz-build
```

The fuzz build prepares flat seed directories below `build-fuzz/fuzz-seeds/`
from the fixture catalog. Native parsers, conversion boundaries, GLB
containers, and encoded media therefore start from valid project data. Empty
fixture categories produce empty seed directories rather than failing seed
preparation.

Run a target until interrupted:

```text
just fuzz ftl
just fuzz ftl roundtrip
just fuzz model native
just fuzz model glb
just fuzz tea
just fuzz tea roundtrip
just fuzz animation native
just fuzz amb
just fuzz amb roundtrip
just fuzz ambiance native
just fuzz ambiance glb
just fuzz cin
just fuzz cin roundtrip
just fuzz cinematic native
just fuzz cinematic glb
just fuzz obj
just fuzz obj mtl
just fuzz glb
just fuzz image
just fuzz audio
just fuzz fts
just fuzz fts roundtrip
just fuzz llf
just fuzz llf roundtrip
just fuzz dlf
just fuzz dlf roundtrip
just fuzz level native-fts
just fuzz level native-llf
just fuzz level native-dlf
just fuzz level native-dlf-embedded
just fuzz level glb
```

Native parser targets accept arbitrary format bytes. Their `roundtrip`
variants require every successfully parsed value to write and parse again;
FTL, TEA, AMB, and CIN also require deterministic second writes. The `native`
Model, Animation, Ambiance, and Cinematic targets continue from a parsed native
format through its intermediate representation and validation.

`model glb`, `level glb`, `ambiance glb`, and `cinematic glb` preserve GLB
container framing while fuzzing its JSON and binary payloads so mutations
reach format-specific import logic. `glb` instead fuzzes the shared GLB
container parser with raw bytes. `image` and `audio` exercise encoded-data
validation and metadata inspection.

CI and `just pre-release` replay each generated seed corpus once under the fuzz
sanitizers. These bounded smoke tests complement, but do not replace, open-ended
local fuzzing.

Standard fuzz runs use a curated dictionary where stable parser tokens
materially improve reach. Native-format dictionaries focus on binary
identifiers and structural values. GLB projection dictionaries contain
semantic authoring tokens; fixture-derived seeds provide complete structures.
CMake configuration and standard `just fuzz` runs reject missing assigned
dictionaries. Smoke tests ask libFuzzer to parse each assigned dictionary
during seed replay.

Use `just fuzz-mine` with the same arguments to enable value profiling and
final statistics without the curated dictionary.

Persistent corpora are written below `fuzz-corpus/`. Crash, timeout, leak, and
OOM artifacts are written below `fuzz-corpus/artifacts/`. Committed fixtures
and generated seed files are read as starting inputs and are never modified.
