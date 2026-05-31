# Testing

## Regular tests

```bash
# Configure and build first
just configure dev
just build dev
```

Run tests with doctest output:

```bash
just run-tests               # all tests
just run-tests ftl           # all ftl tests (unit + api + corpus)
just run-tests ftl unit      # ftl unit tests only
just run-tests ftl api       # ftl api tests only
just run-tests ftl corpus    # ftl corpus tests only
```

To run with ASan + UBSan (recommended before committing):

```bash
just sanitize
```

To run clang-tidy as an enforced gate:

```bash
just tidy
```

To generate coverage:

```bash
just coverage
just coverage src/arx/ftl.cpp
```

## Fuzzing

Requires Clang with libFuzzer support.

```bash
just fuzz-build
```

Then run a fuzzer:

```bash
just fuzz ftl             # FTL fuzzer - Ctrl+C to stop
just fuzz ftl roundtrip   # FTL roundtrip fuzzer
just fuzz ftl json        # FTL JSON import fuzzer
just fuzz tea             # TEA fuzzer
just fuzz tea roundtrip   # TEA roundtrip fuzzer
just fuzz obj             # OBJ fuzzer
just fuzz obj mtl         # OBJ+MTL fuzzer
just fuzz glb             # GLB import fuzzer
```

Corpus dirs (`fuzz-corpus/*/`) are gitignored and created automatically on first run.
Seed files in `data/fixtures/*/` are used as starting inputs and never modified.

## Test assets

`data/fixtures/` — CC0-licensed examples committed to the repo, organized by asset role:
`model/`, `animation/`, and `level/`. Model fixtures use `native/`, `obj/`, `glb/`, and
`json/`; level fixtures keep document identity in the path, such as `level/dlf/native/`
and `level/dlf/json/`.

`data/arx/<format>/` — original Arx Fatalis game assets extracted from the game. These cannot
be committed (copyright). Place files here to run tests that require real game data; the
directory is gitignored and native-only.
