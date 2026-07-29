# Testing and Fuzzing

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
and sanitizer test suites, enforces clang-tidy, builds every fuzz target, and
installs and smoke-tests the release CLI package.

`sanitize` runs the test suite with ASan and UBSan. `tidy` treats clang-tidy
diagnostics as errors. `format` applies clang-format; `format-check` only
reports differences.

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

Committed CC0 fixtures live below `data/fixtures/`:

```text
data/fixtures/model/
data/fixtures/animation/
data/fixtures/level/
```

Their sources and derivative families are recorded in
[Test Data Attribution](../data/Attribution.md).

Original Arx Fatalis resources cannot be committed. Optional corpus tests
discover them by extension below:

```text
data/arx/ftl/
data/arx/tea/
data/arx/fts/
data/arx/dlf/
data/arx/llf/
```

Readers and independent native roundtrips test every matching file that is
present. Level bundle tests use only complete same-level FTS/DLF/LLF triplets;
an incomplete optional game triplet is ignored rather than treated as a test
failure. Committed fixture triplets remain mandatory.

## Fuzzing

Build all libFuzzer targets:

```text
just fuzz-build
```

Run a parser or roundtrip target until interrupted:

```text
just fuzz ftl
just fuzz ftl roundtrip
just fuzz ftl json
just fuzz tea
just fuzz tea roundtrip
just fuzz tea json
just fuzz obj
just fuzz obj mtl
just fuzz glb
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

Use `just fuzz-mine` with the same arguments to enable value profiling and
final statistics without a curated dictionary.

Persistent corpora are written below `fuzz-corpus/`. Crash, timeout, leak, and
OOM artifacts are written below `fuzz-corpus/artifacts/`. Committed fixtures
and generated seed files are read as starting inputs and are never modified.
