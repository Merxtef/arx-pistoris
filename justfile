set shell := ["bash", "-uc"]
set windows-shell := ["powershell.exe", "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command"]

# List available recipes
default:
    just --list

# Configure a CMake preset
configure preset="dev":
    cmake --preset {{ preset }}

# Build a CMake preset
build preset="dev":
    cmake --build --preset {{ preset }}

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

# Build fuzz targets
fuzz-build:
    cmake --preset fuzz
    cmake --build --preset fuzz

# Run doctest binaries directly: just run-tests [suite] [unit|api|corpus]
[unix]
run-tests suite="" target="":
    #!/usr/bin/env bash
    suite="{{ suite }}"
    target="{{ target }}"
    bin_dir="build/bin"
    unit="$bin_dir/arx_pistoris_unit_tests"
    api="$bin_dir/arx_pistoris_c_tests"
    corpus="$bin_dir/arx_pistoris_corpus_tests"

    for exe in "$unit" "$api" "$corpus"; do
      if [[ ! -f "$exe" ]]; then
        echo "error: binary not found: $exe" >&2
        echo "Run: just configure dev && just build dev" >&2
        exit 1
      fi
    done

    case "$target" in
      ""|unit|api|corpus) ;;
      *) echo "error: unknown target '$target'. Valid: unit, api, corpus" >&2; exit 1 ;;
    esac

    filter=()
    [[ -n "$suite" ]] && filter=("--ts=$suite")

    failed=0
    first=1
    run() {
      local name="$1" exe="$2"
      if [[ "$target" == "" || "$target" == "$name" ]]; then
        [[ $first -eq 0 ]] && echo
        first=0
        "$exe" "${filter[@]}" || failed=1
      fi
    }

    run unit "$unit"
    run api "$api"
    run corpus "$corpus"
    exit "$failed"

# Run doctest binaries directly: just run-tests [suite] [unit|api|corpus]
[windows]
run-tests suite="" target="":
    #!powershell.exe
    $Suite = '{{ suite }}'
    $Target = '{{ target }}'
    $BinDir = 'build/bin'
    $Unit = "$BinDir/arx_pistoris_unit_tests.exe"
    $Api = "$BinDir/arx_pistoris_c_tests.exe"
    $Corpus = "$BinDir/arx_pistoris_corpus_tests.exe"

    foreach ($Exe in @($Unit, $Api, $Corpus)) {
      if (-not (Test-Path $Exe)) {
        Write-Host "error: binary not found: $Exe" -ForegroundColor Red
        Write-Host "Run: just configure dev; just build dev"
        exit 1
      }
    }

    if ($Target -notin @('', 'unit', 'api', 'corpus')) {
      Write-Host "error: unknown target '$Target'. Valid: unit, api, corpus" -ForegroundColor Red
      exit 1
    }

    $Filter = if ($Suite) { @("--ts=$Suite") } else { @() }
    $Failed = $false
    $First = $true

    foreach ($Entry in @(
      @{ Name = 'unit'; Exe = $Unit },
      @{ Name = 'api'; Exe = $Api },
      @{ Name = 'corpus'; Exe = $Corpus }
    )) {
      if ($Target -eq '' -or $Target -eq $Entry.Name) {
        if (-not $First) { Write-Host }
        $First = $false
        & $Entry.Exe @Filter
        if ($LASTEXITCODE -ne 0) { $Failed = $true }
      }
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
    api="$build/bin/arx_pistoris_c_tests"
    corpus="$build/bin/arx_pistoris_corpus_tests"

    for tool in llvm-profdata llvm-cov; do
      command -v "$tool" >/dev/null 2>&1 || { echo "error: $tool not found in PATH" >&2; exit 1; }
    done

    cmake --preset coverage >/dev/null
    cmake --build --preset coverage

    for exe in "$unit" "$api" "$corpus"; do
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
      -object "$api"
      -object "$corpus"
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
[windows]
coverage path="":
    #!powershell.exe
    $ErrorActionPreference = 'Stop'
    $Build = 'build-coverage'
    $Profraw = "$Build/profraw"
    $Profdata = "$Build/coverage.profdata"
    $ProfdataRsp = "$Build/coverage-profraws.rsp"
    $Html = "$Build/html"
    $Unit = "$Build/bin/arx_pistoris_unit_tests.exe"
    $Api = "$Build/bin/arx_pistoris_c_tests.exe"
    $Corpus = "$Build/bin/arx_pistoris_corpus_tests.exe"
    $SourcePath = '{{ path }}'

    foreach ($Tool in @('llvm-profdata', 'llvm-cov')) {
      if (-not (Get-Command $Tool -ErrorAction SilentlyContinue)) {
        Write-Host "error: $Tool not found in PATH" -ForegroundColor Red
        exit 1
      }
    }

    & cmake --preset coverage | Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & cmake --build --preset coverage
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    foreach ($Exe in @($Unit, $Api, $Corpus)) {
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
      '-object', $Api,
      '-object', $Corpus
    )

    if ($SourcePath) {
      & llvm-cov show @CovArgs $SourcePath
    } else {
      & llvm-cov report @CovArgs
      & llvm-cov show -format=html "-output-dir=$Html" -show-regions '-show-branches=count' @CovArgs | Out-Null
      Write-Host ''
      Write-Host "HTML report: $Html/index.html"
    }

# Run a fuzzer: just fuzz <ftl|tea|obj|glb> [roundtrip|json|mtl]
[unix]
fuzz format variant="":
    @just fuzz-run "{{ format }}" 0 "{{ variant }}"

# Run a fuzzer with value profiling and final stats: just fuzz-mine <ftl|tea|obj|glb> [roundtrip|json|mtl]
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
      tea/)          exe=arx_pistoris_tea_fuzz;           corpus=fuzz-corpus/tea;           seeds=data/arx/tea;    dict=fuzz/tea.dict ;;
      tea/roundtrip) exe=arx_pistoris_tea_roundtrip_fuzz; corpus=fuzz-corpus/tea-roundtrip; seeds=data/arx/tea;    dict=fuzz/tea.dict ;;
      tea/json)      exe=arx_pistoris_tea_json_fuzz;      corpus=fuzz-corpus/tea-json;      seeds=fuzz/seeds/tea-json; dict= ;;
      obj/)          exe=arx_pistoris_obj_fuzz;           corpus=fuzz-corpus/obj;           seeds=data/fixtures/model/obj; dict=fuzz/obj.dict ;;
      obj/mtl)       exe=arx_pistoris_obj_with_mtl_fuzz;  corpus=fuzz-corpus/obj-with-mtl;  seeds=data/fixtures/model/obj; dict=fuzz/obj.dict ;;
      glb/)          exe=arx_pistoris_glb_import_fuzz;    corpus=fuzz-corpus/glb;           seeds=build-fuzz/fuzz-seeds/glb; dict= ;;
      *) echo "error: unknown target '$format $variant'. Valid: ftl, ftl roundtrip, ftl json, tea, tea roundtrip, tea json, obj, obj mtl, glb" >&2; exit 1 ;;
    esac

    exe_path="build-fuzz/bin/$exe"
    [[ -f "$exe_path" ]] || { echo "error: fuzz binary not found: $exe_path" >&2; echo "Run: just fuzz-build" >&2; exit 1; }
    mkdir -p "$corpus"

    args=("$corpus")
    [[ -n "$seeds" && -d "$seeds" ]] && args+=("$seeds")
    if [[ "$mine" == "1" ]]; then
      args+=("-print_final_stats=1" "-use_value_profile=1")
      echo "Mining mode: value profiling enabled; curated dictionaries disabled."
    elif [[ -n "$dict" && -f "$dict" ]]; then
      args+=("-dict=$(pwd)/$dict")
    fi

    echo "Running fuzzer: $exe"
    "$exe_path" "${args[@]}"

# Run a fuzzer: just fuzz <ftl|tea|obj|glb> [roundtrip|json|mtl]
[windows]
fuzz format variant="":
    @just fuzz-run "{{ format }}" 0 "{{ variant }}"

# Run a fuzzer with value profiling and final stats: just fuzz-mine <ftl|tea|obj|glb> [roundtrip|json|mtl]
[windows]
fuzz-mine format variant="":
    @just fuzz-run "{{ format }}" 1 "{{ variant }}"

[windows]
fuzz-run format mine="0" variant="":
    #!powershell.exe
    $Format = '{{ format }}'
    $Variant = '{{ variant }}'
    $Mine = '{{ mine }}'

    switch ("$Format/$Variant") {
      'ftl/'          { $Exe = 'arx_pistoris_ftl_fuzz';           $Corpus = 'fuzz-corpus/ftl';           $Seeds = 'data/fixtures/model/native'; $Dict = 'fuzz/ftl.dict' }
      'ftl/roundtrip' { $Exe = 'arx_pistoris_ftl_roundtrip_fuzz'; $Corpus = 'fuzz-corpus/ftl-roundtrip'; $Seeds = 'data/fixtures/model/native'; $Dict = 'fuzz/ftl.dict' }
      'ftl/json'      { $Exe = 'arx_pistoris_ftl_json_fuzz';      $Corpus = 'fuzz-corpus/ftl-json';      $Seeds = 'fuzz/seeds/ftl-json'; $Dict = $null }
      'tea/'          { $Exe = 'arx_pistoris_tea_fuzz';           $Corpus = 'fuzz-corpus/tea';           $Seeds = 'data/arx/tea';    $Dict = 'fuzz/tea.dict' }
      'tea/roundtrip' { $Exe = 'arx_pistoris_tea_roundtrip_fuzz'; $Corpus = 'fuzz-corpus/tea-roundtrip'; $Seeds = 'data/arx/tea';    $Dict = 'fuzz/tea.dict' }
      'tea/json'      { $Exe = 'arx_pistoris_tea_json_fuzz';      $Corpus = 'fuzz-corpus/tea-json';      $Seeds = 'fuzz/seeds/tea-json'; $Dict = $null }
      'obj/'          { $Exe = 'arx_pistoris_obj_fuzz';           $Corpus = 'fuzz-corpus/obj';           $Seeds = 'data/fixtures/model/obj'; $Dict = 'fuzz/obj.dict' }
      'obj/mtl'       { $Exe = 'arx_pistoris_obj_with_mtl_fuzz';  $Corpus = 'fuzz-corpus/obj-with-mtl';  $Seeds = 'data/fixtures/model/obj'; $Dict = 'fuzz/obj.dict' }
      'glb/'          { $Exe = 'arx_pistoris_glb_import_fuzz';    $Corpus = 'fuzz-corpus/glb';           $Seeds = 'build-fuzz/fuzz-seeds/glb'; $Dict = $null }
      default         { Write-Host "error: unknown target '$Format $Variant'. Valid: ftl, ftl roundtrip, ftl json, tea, tea roundtrip, tea json, obj, obj mtl, glb" -ForegroundColor Red; exit 1 }
    }

    $ExePath = "build-fuzz/bin/$Exe.exe"
    if (-not (Test-Path $ExePath)) {
      Write-Host "error: fuzz binary not found: $ExePath" -ForegroundColor Red
      Write-Host "Run: just fuzz-build"
      exit 1
    }

    if (-not (Test-Path $Corpus)) {
      New-Item -ItemType Directory -Path $Corpus | Out-Null
    }

    $FuzzerArgs = @($Corpus)
    if ($Seeds -and (Test-Path $Seeds)) { $FuzzerArgs += $Seeds }
    if ($Mine -eq '1') {
      $FuzzerArgs += @("-print_final_stats=1", "-use_value_profile=1")
      Write-Host "Mining mode: value profiling enabled; curated dictionaries disabled." -ForegroundColor Cyan
    } elseif ($Dict -and (Test-Path $Dict)) {
      $FuzzerArgs += "-dict=$((Resolve-Path $Dict).Path)"
    }

    Write-Host "Running fuzzer: $Exe" -ForegroundColor Cyan
    & $ExePath $FuzzerArgs
