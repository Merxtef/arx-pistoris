# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

set shell := ["bash", "-uc"]
set windows-shell := ["powershell.exe", "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command"]

# List available recipes
default:
    just --list

# Configure a CMake preset
configure preset="dev":
    cmake --preset {{ preset }}

# Build a logical target for a CMake preset
[unix]
build target="all" preset="dev":
    #!/usr/bin/env bash
    set -euo pipefail
    target="{{ target }}"
    preset="{{ preset }}"

    case "$target" in
      all) targets=() ;;
      lib) targets=(arx_pistoris_c arx_pistoris_cpp) ;;
      cli) targets=(arx_pistoris_cli) ;;
      unit) targets=(arx_pistoris_unit_tests) ;;
      api) targets=(arx_pistoris_c_tests arx_pistoris_c_smoke arx_pistoris_cpp_tests) ;;
      corpus) targets=(arx_pistoris_corpus_tests arx_pistoris_cpp_corpus_tests) ;;
      tests)
        targets=(
          arx_pistoris_unit_tests
          arx_pistoris_c_tests
          arx_pistoris_c_smoke
          arx_pistoris_cpp_tests
          arx_pistoris_corpus_tests
          arx_pistoris_cpp_corpus_tests
        )
        ;;
      *)
        echo "error: unknown build target '$target'. Valid: all, lib, cli, unit, api, corpus, tests" >&2
        exit 1
        ;;
    esac

    if [[ ${#targets[@]} -eq 0 ]]; then
      cmake --build --preset "$preset"
    else
      cmake --build --preset "$preset" --target "${targets[@]}"
    fi

[windows, script("powershell.exe", "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File")]
build target="all" preset="dev":
    $Target = '{{ target }}'
    $Preset = '{{ preset }}'
    $Targets = @(switch ($Target) {
      'all' { @() }
      'lib' { @('arx_pistoris_c', 'arx_pistoris_cpp') }
      'cli' { @('arx_pistoris_cli') }
      'unit' { @('arx_pistoris_unit_tests') }
      'api' { @('arx_pistoris_c_tests', 'arx_pistoris_c_smoke', 'arx_pistoris_cpp_tests') }
      'corpus' { @('arx_pistoris_corpus_tests', 'arx_pistoris_cpp_corpus_tests') }
      'tests' {
        @(
          'arx_pistoris_unit_tests',
          'arx_pistoris_c_tests',
          'arx_pistoris_c_smoke',
          'arx_pistoris_cpp_tests',
          'arx_pistoris_corpus_tests',
          'arx_pistoris_cpp_corpus_tests'
        )
      }
      default {
        Write-Host "error: unknown build target '$Target'. Valid: all, lib, cli, unit, api, corpus, tests" -ForegroundColor Red
        exit 1
      }
    })

    $CMakeArgs = @('--build', '--preset', $Preset)
    if ($Targets.Count -eq 0) {
      & cmake @CMakeArgs
    } else {
      $CMakeArgs += '--target'
      $CMakeArgs += $Targets
      & cmake @CMakeArgs
    }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Run CTest for a preset
test preset="dev":
    ctest --preset {{ preset }}

# Configure, build, and test the dev preset
dev:
    cmake --workflow --preset dev

# Configure, build, and test with sanitizers
sanitize:
    cmake --workflow --preset sanitize

# Build with clang-tidy diagnostics enforced as errors
tidy:
    cmake --preset tidy
    cmake --build --preset tidy

# Build the release preset
release:
    cmake --preset release
    cmake --build --preset release

# Format project C++ sources
format:
    cmake --preset dev
    cmake --build --preset dev --target format

# Check project C++ source formatting
format-check:
    cmake --preset dev
    cmake --build --preset dev --target format-check

# Build fuzz targets
fuzz-build:
    cmake --preset fuzz
    cmake --build --preset fuzz

# Build, install, and verify the release CLI package
[unix]
package-smoke:
    cmake --preset release -DARX_WARNINGS_AS_ERRORS=ON
    cmake --build --preset release --target arx_pistoris_cli
    cmake -E remove_directory build-release/package
    cmake --install build-release --prefix build-release/package/arx-pistoris
    bash scripts/ci/package-smoke.sh build-release/package/arx-pistoris

[windows]
package-smoke:
    cmake --preset release -DARX_WARNINGS_AS_ERRORS=ON
    cmake --build --preset release --target arx_pistoris_cli
    cmake -E remove_directory build-release/package
    cmake --install build-release --prefix build-release/package/arx-pistoris
    powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File scripts/ci/package-smoke.ps1 build-release/package/arx-pistoris

# Run the local release gate
pre-release:
    just format-check
    cmake --preset dev -DARX_WARNINGS_AS_ERRORS=ON
    cmake --build --preset dev
    ctest --preset dev
    cmake --preset sanitize -DARX_WARNINGS_AS_ERRORS=ON
    cmake --build --preset sanitize
    ctest --preset sanitize
    cmake --preset tidy -DARX_WARNINGS_AS_ERRORS=ON
    cmake --build --preset tidy
    cmake --preset fuzz -DARX_WARNINGS_AS_ERRORS=ON
    cmake --build --preset fuzz
    just package-smoke

# Run test binaries directly: just run-tests [suite] [target]
[unix]
run-tests suite="" target="":
    #!/usr/bin/env bash
    suite="{{ suite }}"
    target="{{ target }}"
    bin_dir="build/bin"
    unit="$bin_dir/arx_pistoris_unit_tests"
    c_api="$bin_dir/arx_pistoris_c_tests"
    c_smoke="$bin_dir/arx_pistoris_c_smoke"
    cpp_api="$bin_dir/arx_pistoris_cpp_tests"
    c_corpus="$bin_dir/arx_pistoris_corpus_tests"
    cpp_corpus="$bin_dir/arx_pistoris_cpp_corpus_tests"

    case "$target" in
      ""|all|tests|unit|api|corpus|c-api|cpp-api|c-corpus|cpp-corpus) ;;
      *)
        echo "error: unknown test target '$target'. Valid: all, tests, unit, api, corpus, c-api, cpp-api, c-corpus, cpp-corpus" >&2
        exit 1
        ;;
    esac

    filter=()
    [[ -n "$suite" ]] && filter=("--test-suite=$suite")

    failed=0
    first=1
    selected() {
      local name="$1"
      case "$target" in
        ""|all|tests) return 0 ;;
        unit) [[ "$name" == unit ]] ;;
        api) [[ "$name" == c-api || "$name" == c-smoke || "$name" == cpp-api ]] ;;
        c-api) [[ "$name" == c-api || "$name" == c-smoke ]] ;;
        corpus) [[ "$name" == c-corpus || "$name" == cpp-corpus ]] ;;
        *) [[ "$name" == "$target" ]] ;;
      esac
    }

    run() {
      local name="$1" exe="$2" filterable="$3"
      selected "$name" || return
      if [[ ! -f "$exe" ]]; then
        echo "error: binary not found: $exe" >&2
        echo "Run: just configure dev && just build tests" >&2
        exit 1
      fi
      [[ $first -eq 0 ]] && echo
      first=0
      if [[ "$filterable" == 1 ]]; then
        "$exe" "${filter[@]}" || failed=1
      else
        "$exe" || failed=1
      fi
    }

    run unit "$unit" 1
    run c-api "$c_api" 1
    run c-smoke "$c_smoke" 0
    run cpp-api "$cpp_api" 1
    run c-corpus "$c_corpus" 1
    run cpp-corpus "$cpp_corpus" 1
    exit "$failed"

# Run test binaries directly: just run-tests [suite] [target]
[windows, script("powershell.exe", "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File")]
run-tests suite="" target="":
    $Suite = '{{ suite }}'
    $Target = '{{ target }}'
    $BinDir = 'build/bin'
    $Unit = "$BinDir/arx_pistoris_unit_tests.exe"
    $CApi = "$BinDir/arx_pistoris_c_tests.exe"
    $CSmoke = "$BinDir/arx_pistoris_c_smoke.exe"
    $CppApi = "$BinDir/arx_pistoris_cpp_tests.exe"
    $CCorpus = "$BinDir/arx_pistoris_corpus_tests.exe"
    $CppCorpus = "$BinDir/arx_pistoris_cpp_corpus_tests.exe"

    if ($Target -notin @('', 'all', 'tests', 'unit', 'api', 'corpus', 'c-api', 'cpp-api', 'c-corpus', 'cpp-corpus')) {
      Write-Host "error: unknown test target '$Target'. Valid: all, tests, unit, api, corpus, c-api, cpp-api, c-corpus, cpp-corpus" -ForegroundColor Red
      exit 1
    }

    $Failed = $false
    $First = $true

    function Test-Selected([string]$Name) {
      switch ($Target) {
        { $_ -in @('', 'all', 'tests') } { return $true }
        'unit' { return $Name -eq 'unit' }
        'api' { return $Name -in @('c-api', 'c-smoke', 'cpp-api') }
        'c-api' { return $Name -in @('c-api', 'c-smoke') }
        'corpus' { return $Name -in @('c-corpus', 'cpp-corpus') }
        default { return $Name -eq $Target }
      }
    }

    foreach ($Entry in @(
      @{ Name = 'unit'; Exe = $Unit; Filterable = $true },
      @{ Name = 'c-api'; Exe = $CApi; Filterable = $true },
      @{ Name = 'c-smoke'; Exe = $CSmoke; Filterable = $false },
      @{ Name = 'cpp-api'; Exe = $CppApi; Filterable = $true },
      @{ Name = 'c-corpus'; Exe = $CCorpus; Filterable = $true },
      @{ Name = 'cpp-corpus'; Exe = $CppCorpus; Filterable = $true }
    )) {
      if (-not (Test-Selected $Entry.Name)) { continue }
      if (-not (Test-Path $Entry.Exe)) {
        Write-Host "error: binary not found: $($Entry.Exe)" -ForegroundColor Red
        Write-Host "Run: just configure dev; just build tests"
        exit 1
      }
      if (-not $First) { Write-Host }
      $First = $false
      if ($Entry.Filterable -and $Suite) {
        & $Entry.Exe "--test-suite=$Suite"
      } else {
        & $Entry.Exe
      }
      if ($LASTEXITCODE -ne 0) { $Failed = $true }
    }

    if ($Failed) { exit 1 }

# Generate llvm-cov report: just coverage [source-file]
[unix]
coverage path="":
    #!/usr/bin/env bash
    set -euo pipefail
    build="build-coverage"
    profraw="$build/profraw"
    profdata="$build/coverage.profdata"
    profdata_rsp="$build/coverage-profraws.rsp"
    html="$build/html"
    unit="$build/bin/arx_pistoris_unit_tests"
    library="$build/bin/libarx_pistoris.so"
    cli="$build/bin/arx-pistor"

    for tool in llvm-profdata llvm-cov; do
      command -v "$tool" >/dev/null 2>&1 || { echo "error: $tool not found in PATH" >&2; exit 1; }
    done

    rm -f default.profraw "$build/default.profraw"
    cmake --preset coverage >/dev/null
    mkdir -p "$build"
    rm -f "$build"/discovery_*.profraw
    LLVM_PROFILE_FILE="$build/discovery_%m_%p.profraw" cmake --build --preset coverage
    rm -f "$build"/discovery_*.profraw
    rm -f default.profraw "$build/default.profraw"

    for exe in "$unit" "$library" "$cli"; do
      [[ -f "$exe" ]] || { echo "error: missing binary after build: $exe" >&2; exit 1; }
    done

    rm -rf "$profraw"
    mkdir -p "$profraw"
    ctest --preset coverage

    shopt -s nullglob
    profraws=("$profraw"/*.profraw)
    [[ ${#profraws[@]} -gt 0 ]] || { echo "error: no .profraw files in $profraw" >&2; exit 1; }
    printf '%s\n' "${profraws[@]}" > "$profdata_rsp"
    llvm-profdata merge -sparse @"$profdata_rsp" -o "$profdata"

    cov_args=(
      "-instr-profile=$profdata"
      "-ignore-filename-regex=third_party|tests|build|fuzz"
      "$unit"
      -object "$library"
      -object "$cli"
    )

    source_path="{{ path }}"
    if [[ -n "$source_path" ]]; then
      llvm-cov show "${cov_args[@]}" "$source_path"
    else
      llvm-cov report "${cov_args[@]}"
      llvm-cov show -format=html "-output-dir=$html" -show-regions -show-branches=count "${cov_args[@]}" >/dev/null
      echo
      echo "HTML report: $html/index.html"
    fi

# Generate llvm-cov report: just coverage [source-file]
[windows, script("powershell.exe", "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File")]
coverage path="":
    $ErrorActionPreference = 'Stop'
    $Build = 'build-coverage'
    $Profraw = "$Build/profraw"
    $Profdata = "$Build/coverage.profdata"
    $ProfdataRsp = "$Build/coverage-profraws.rsp"
    $Html = "$Build/html"
    $Unit = "$Build/bin/arx_pistoris_unit_tests.exe"
    $Library = "$Build/bin/arx_pistoris.dll"
    $Cli = "$Build/bin/arx-pistor.exe"
    $SourcePath = '{{ path }}'

    foreach ($Tool in @('llvm-profdata', 'llvm-cov')) {
      if (-not (Get-Command $Tool -ErrorAction SilentlyContinue)) {
        Write-Host "error: $Tool not found in PATH" -ForegroundColor Red
        exit 1
      }
    }

    Remove-Item -LiteralPath 'default.profraw', "$Build/default.profraw" -Force -ErrorAction SilentlyContinue
    & cmake --preset coverage | Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Get-ChildItem -Path $Build -Filter 'discovery_*.profraw' -File -ErrorAction SilentlyContinue |
      Remove-Item -Force
    $env:LLVM_PROFILE_FILE = "$Build/discovery_%m_%p.profraw"
    & cmake --build --preset coverage
    Remove-Item Env:LLVM_PROFILE_FILE -ErrorAction SilentlyContinue
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Get-ChildItem -Path $Build -Filter 'discovery_*.profraw' -File -ErrorAction SilentlyContinue |
      Remove-Item -Force
    Remove-Item -LiteralPath 'default.profraw', "$Build/default.profraw" -Force -ErrorAction SilentlyContinue

    foreach ($Exe in @($Unit, $Library, $Cli)) {
      if (-not (Test-Path $Exe)) {
        Write-Host "error: missing binary after build: $Exe" -ForegroundColor Red
        exit 1
      }
    }

    if (Test-Path $Profraw) { Remove-Item -Recurse -Force $Profraw }
    New-Item -ItemType Directory -Force -Path $Profraw | Out-Null

    & ctest --preset coverage
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $Profraws = Get-ChildItem -Path $Profraw -Filter '*.profraw' -File
    if ($Profraws.Count -eq 0) {
      Write-Host "error: no .profraw files in $Profraw" -ForegroundColor Red
      exit 1
    }

    $Profraws.FullName | Set-Content -Path $ProfdataRsp -Encoding ascii
    & llvm-profdata merge -sparse "@$ProfdataRsp" -o $Profdata
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $CovArgs = @(
      "-instr-profile=$Profdata",
      '-ignore-filename-regex=third_party|tests|build|fuzz',
      $Unit,
      '-object', $Library,
      '-object', $Cli
    )

    if ($SourcePath) {
      & llvm-cov show @CovArgs $SourcePath
    } else {
      & llvm-cov report @CovArgs
      & llvm-cov show -format=html "-output-dir=$Html" -show-regions '-show-branches=count' @CovArgs | Out-Null
      Write-Host ''
      Write-Host "HTML report: $Html/index.html"
    }

# Run a fuzzer: just fuzz <ftl|tea|obj|glb|fts|llf|dlf|level> [roundtrip|json|mtl|native-fts|native-llf|native-dlf|native-dlf-embedded]
[unix]
fuzz format variant="":
    @just fuzz-run "{{ format }}" 0 "{{ variant }}"

# Run a fuzzer with value profiling and final stats: just fuzz-mine <ftl|tea|obj|glb|fts|llf|dlf|level> [roundtrip|json|mtl|native-fts|native-llf|native-dlf|native-dlf-embedded]
[unix]
fuzz-mine format variant="":
    @just fuzz-run "{{ format }}" 1 "{{ variant }}"

[unix]
fuzz-run format mine="0" variant="":
    #!/usr/bin/env bash
    set -euo pipefail
    format="{{ format }}"
    variant="{{ variant }}"
    mine="{{ mine }}"

    case "$format/$variant" in
      ftl/)          exe=arx_pistoris_ftl_fuzz;           corpus=fuzz-corpus/ftl;           seeds=data/fixtures/model/native; dict=fuzz/ftl.dict ;;
      ftl/roundtrip) exe=arx_pistoris_ftl_roundtrip_fuzz; corpus=fuzz-corpus/ftl-roundtrip; seeds=data/fixtures/model/native; dict=fuzz/ftl.dict ;;
      ftl/json)      exe=arx_pistoris_ftl_json_fuzz;      corpus=fuzz-corpus/ftl-json;      seeds=fuzz/seeds/ftl-json; dict= ;;
      tea/)          exe=arx_pistoris_tea_fuzz;           corpus=fuzz-corpus/tea;           seeds=data/fixtures/animation/native; dict=fuzz/tea.dict ;;
      tea/roundtrip) exe=arx_pistoris_tea_roundtrip_fuzz; corpus=fuzz-corpus/tea-roundtrip; seeds=data/fixtures/animation/native; dict=fuzz/tea.dict ;;
      tea/json)      exe=arx_pistoris_tea_json_fuzz;      corpus=fuzz-corpus/tea-json;      seeds=fuzz/seeds/tea-json; dict= ;;
      obj/)          exe=arx_pistoris_obj_fuzz;           corpus=fuzz-corpus/obj;           seeds=data/fixtures/model/obj; dict=fuzz/obj.dict ;;
      obj/mtl)       exe=arx_pistoris_obj_with_mtl_fuzz;  corpus=fuzz-corpus/obj-with-mtl;  seeds=data/fixtures/model/obj; dict=fuzz/obj.dict ;;
      glb/)          exe=arx_pistoris_glb_import_fuzz;    corpus=fuzz-corpus/glb;           seeds=build-fuzz/fuzz-seeds/glb; dict= ;;
      fts/)          exe=arx_pistoris_fts_fuzz;           corpus=fuzz-corpus/fts;           seeds=data/fixtures/level/fts/native; dict=fuzz/fts.dict ;;
      fts/roundtrip) exe=arx_pistoris_fts_roundtrip_fuzz; corpus=fuzz-corpus/fts-roundtrip; seeds=data/fixtures/level/fts/native; dict=fuzz/fts.dict ;;
      llf/)          exe=arx_pistoris_llf_fuzz;           corpus=fuzz-corpus/llf;           seeds=data/fixtures/level/llf/native; dict=fuzz/llf.dict ;;
      llf/roundtrip) exe=arx_pistoris_llf_roundtrip_fuzz; corpus=fuzz-corpus/llf-roundtrip; seeds=data/fixtures/level/llf/native; dict=fuzz/llf.dict ;;
      dlf/)          exe=arx_pistoris_dlf_fuzz;           corpus=fuzz-corpus/dlf;           seeds=data/fixtures/level/dlf/native; dict=fuzz/dlf.dict ;;
      dlf/roundtrip) exe=arx_pistoris_dlf_roundtrip_fuzz; corpus=fuzz-corpus/dlf-roundtrip; seeds=data/fixtures/level/dlf/native; dict=fuzz/dlf.dict ;;
      level/native-fts)          exe=arx_pistoris_level_native_fts_fuzz;          corpus=fuzz-corpus/level-native-fts;          seeds=data/fixtures/level/fts/native; dict=fuzz/fts.dict ;;
      level/native-llf)          exe=arx_pistoris_level_native_llf_fuzz;          corpus=fuzz-corpus/level-native-llf;          seeds=data/fixtures/level/llf/native; dict=fuzz/llf.dict ;;
      level/native-dlf)          exe=arx_pistoris_level_native_dlf_fuzz;          corpus=fuzz-corpus/level-native-dlf;          seeds=data/fixtures/level/dlf/native; dict=fuzz/dlf.dict ;;
      level/native-dlf-embedded) exe=arx_pistoris_level_native_dlf_embedded_fuzz; corpus=fuzz-corpus/level-native-dlf-embedded; seeds=data/fixtures/level/dlf/native; dict=fuzz/dlf.dict ;;
      level/glb)                 exe=arx_pistoris_level_glb_import_fuzz;          corpus=fuzz-corpus/level-glb;                 seeds=build-fuzz/fuzz-seeds/level-glb; dict=fuzz/level-glb.dict ;;
      *) echo "error: unknown target '$format $variant'. Valid: ftl, ftl roundtrip, ftl json, tea, tea roundtrip, tea json, obj, obj mtl, glb, fts, fts roundtrip, llf, llf roundtrip, dlf, dlf roundtrip, level native-fts, level native-llf, level native-dlf, level native-dlf-embedded, level glb" >&2; exit 1 ;;
    esac

    exe_path="build-fuzz/bin/$exe"
    [[ -f "$exe_path" ]] || { echo "error: fuzz binary not found: $exe_path" >&2; echo "Run: just fuzz-build" >&2; exit 1; }
    artifacts="fuzz-corpus/artifacts/$exe"
    mkdir -p "$corpus" "$artifacts"

    args=("$corpus" "-artifact_prefix=$(pwd)/$artifacts/")
    [[ -n "$seeds" && -d "$seeds" ]] && args+=("$seeds")
    if [[ "$mine" == "1" ]]; then
      args+=("-print_final_stats=1" "-use_value_profile=1")
      echo "Mining mode: value profiling enabled; curated dictionaries disabled."
    elif [[ -n "$dict" && -f "$dict" ]]; then
      args+=("-dict=$(pwd)/$dict")
    fi

    echo "Running fuzzer: $exe"
    "$exe_path" "${args[@]}"

# Run a fuzzer: just fuzz <ftl|tea|obj|glb|fts|llf|dlf|level> [roundtrip|json|mtl|native-fts|native-llf|native-dlf|native-dlf-embedded]
[windows]
fuzz format variant="":
    @just fuzz-run "{{ format }}" 0 "{{ variant }}"

# Run a fuzzer with value profiling and final stats: just fuzz-mine <ftl|tea|obj|glb|fts|llf|dlf|level> [roundtrip|json|mtl|native-fts|native-llf|native-dlf|native-dlf-embedded]
[windows]
fuzz-mine format variant="":
    @just fuzz-run "{{ format }}" 1 "{{ variant }}"

[windows, script("powershell.exe", "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File")]
fuzz-run format mine="0" variant="":
    $Format = '{{ format }}'
    $Variant = '{{ variant }}'
    $Mine = '{{ mine }}'

    switch ("$Format/$Variant") {
      'ftl/'          { $Exe = 'arx_pistoris_ftl_fuzz';           $Corpus = 'fuzz-corpus/ftl';           $Seeds = 'data/fixtures/model/native'; $Dict = 'fuzz/ftl.dict' }
      'ftl/roundtrip' { $Exe = 'arx_pistoris_ftl_roundtrip_fuzz'; $Corpus = 'fuzz-corpus/ftl-roundtrip'; $Seeds = 'data/fixtures/model/native'; $Dict = 'fuzz/ftl.dict' }
      'ftl/json'      { $Exe = 'arx_pistoris_ftl_json_fuzz';      $Corpus = 'fuzz-corpus/ftl-json';      $Seeds = 'fuzz/seeds/ftl-json'; $Dict = $null }
      'tea/'          { $Exe = 'arx_pistoris_tea_fuzz';           $Corpus = 'fuzz-corpus/tea';           $Seeds = 'data/fixtures/animation/native'; $Dict = 'fuzz/tea.dict' }
      'tea/roundtrip' { $Exe = 'arx_pistoris_tea_roundtrip_fuzz'; $Corpus = 'fuzz-corpus/tea-roundtrip'; $Seeds = 'data/fixtures/animation/native'; $Dict = 'fuzz/tea.dict' }
      'tea/json'      { $Exe = 'arx_pistoris_tea_json_fuzz';      $Corpus = 'fuzz-corpus/tea-json';      $Seeds = 'fuzz/seeds/tea-json'; $Dict = $null }
      'obj/'          { $Exe = 'arx_pistoris_obj_fuzz';           $Corpus = 'fuzz-corpus/obj';           $Seeds = 'data/fixtures/model/obj'; $Dict = 'fuzz/obj.dict' }
      'obj/mtl'       { $Exe = 'arx_pistoris_obj_with_mtl_fuzz';  $Corpus = 'fuzz-corpus/obj-with-mtl';  $Seeds = 'data/fixtures/model/obj'; $Dict = 'fuzz/obj.dict' }
      'glb/'          { $Exe = 'arx_pistoris_glb_import_fuzz';    $Corpus = 'fuzz-corpus/glb';           $Seeds = 'build-fuzz/fuzz-seeds/glb'; $Dict = $null }
      'fts/'          { $Exe = 'arx_pistoris_fts_fuzz';           $Corpus = 'fuzz-corpus/fts';           $Seeds = 'data/fixtures/level/fts/native'; $Dict = 'fuzz/fts.dict' }
      'fts/roundtrip' { $Exe = 'arx_pistoris_fts_roundtrip_fuzz'; $Corpus = 'fuzz-corpus/fts-roundtrip'; $Seeds = 'data/fixtures/level/fts/native'; $Dict = 'fuzz/fts.dict' }
      'llf/'          { $Exe = 'arx_pistoris_llf_fuzz';           $Corpus = 'fuzz-corpus/llf';           $Seeds = 'data/fixtures/level/llf/native'; $Dict = 'fuzz/llf.dict' }
      'llf/roundtrip' { $Exe = 'arx_pistoris_llf_roundtrip_fuzz'; $Corpus = 'fuzz-corpus/llf-roundtrip'; $Seeds = 'data/fixtures/level/llf/native'; $Dict = 'fuzz/llf.dict' }
      'dlf/'          { $Exe = 'arx_pistoris_dlf_fuzz';           $Corpus = 'fuzz-corpus/dlf';           $Seeds = 'data/fixtures/level/dlf/native'; $Dict = 'fuzz/dlf.dict' }
      'dlf/roundtrip' { $Exe = 'arx_pistoris_dlf_roundtrip_fuzz'; $Corpus = 'fuzz-corpus/dlf-roundtrip'; $Seeds = 'data/fixtures/level/dlf/native'; $Dict = 'fuzz/dlf.dict' }
      'level/native-fts'          { $Exe = 'arx_pistoris_level_native_fts_fuzz';          $Corpus = 'fuzz-corpus/level-native-fts';          $Seeds = 'data/fixtures/level/fts/native'; $Dict = 'fuzz/fts.dict' }
      'level/native-llf'          { $Exe = 'arx_pistoris_level_native_llf_fuzz';          $Corpus = 'fuzz-corpus/level-native-llf';          $Seeds = 'data/fixtures/level/llf/native'; $Dict = 'fuzz/llf.dict' }
      'level/native-dlf'          { $Exe = 'arx_pistoris_level_native_dlf_fuzz';          $Corpus = 'fuzz-corpus/level-native-dlf';          $Seeds = 'data/fixtures/level/dlf/native'; $Dict = 'fuzz/dlf.dict' }
      'level/native-dlf-embedded' { $Exe = 'arx_pistoris_level_native_dlf_embedded_fuzz'; $Corpus = 'fuzz-corpus/level-native-dlf-embedded'; $Seeds = 'data/fixtures/level/dlf/native'; $Dict = 'fuzz/dlf.dict' }
      'level/glb'                 { $Exe = 'arx_pistoris_level_glb_import_fuzz';          $Corpus = 'fuzz-corpus/level-glb';                 $Seeds = 'build-fuzz/fuzz-seeds/level-glb'; $Dict = 'fuzz/level-glb.dict' }
      default         { Write-Host "error: unknown target '$Format $Variant'. Valid: ftl, ftl roundtrip, ftl json, tea, tea roundtrip, tea json, obj, obj mtl, glb, fts, fts roundtrip, llf, llf roundtrip, dlf, dlf roundtrip, level native-fts, level native-llf, level native-dlf, level native-dlf-embedded, level glb" -ForegroundColor Red; exit 1 }
    }

    $ExePath = "build-fuzz/bin/$Exe.exe"
    if (-not (Test-Path $ExePath)) {
      Write-Host "error: fuzz binary not found: $ExePath" -ForegroundColor Red
      Write-Host "Run: just fuzz-build"
      exit 1
    }

    if (-not (Test-Path $Corpus)) {
      New-Item -ItemType Directory -Force -Path $Corpus | Out-Null
    }
    $Artifacts = "fuzz-corpus/artifacts/$Exe"
    if (-not (Test-Path $Artifacts)) {
      New-Item -ItemType Directory -Force -Path $Artifacts | Out-Null
    }
    $ArtifactPrefix = (Resolve-Path $Artifacts).Path
    if (-not $ArtifactPrefix.EndsWith([System.IO.Path]::DirectorySeparatorChar)) {
      $ArtifactPrefix += [System.IO.Path]::DirectorySeparatorChar
    }

    $FuzzerArgs = @($Corpus, "-artifact_prefix=$ArtifactPrefix")
    if ($Seeds -and (Test-Path $Seeds)) { $FuzzerArgs += $Seeds }
    if ($Mine -eq '1') {
      $FuzzerArgs += @("-print_final_stats=1", "-use_value_profile=1")
      Write-Host "Mining mode: value profiling enabled; curated dictionaries disabled." -ForegroundColor Cyan
    } elseif ($Dict -and (Test-Path $Dict)) {
      $FuzzerArgs += "-dict=$((Resolve-Path $Dict).Path)"
    }

    Write-Host "Running fuzzer: $Exe" -ForegroundColor Cyan
    & $ExePath $FuzzerArgs
