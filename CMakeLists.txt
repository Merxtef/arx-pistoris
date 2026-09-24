# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Merxtef

cmake_minimum_required(VERSION 3.25)
project(arx-pistoris VERSION 0.5.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

option(ARX_COVERAGE "Build with Clang source-based coverage instrumentation" OFF)
option(ARX_SANITIZERS "Build RelWithDebInfo targets with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(ARX_FUZZING "Build libFuzzer fuzz targets (requires clang -fsanitize=fuzzer)" OFF)
option(ARX_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# MSVC STL annotations clash with clang_rt.fuzzer
if(WIN32 AND ARX_FUZZING)
  add_compile_definitions(_DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION)
endif()

function(arx_target_warnings target)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  if(ARX_WARNINGS_AS_ERRORS)
    target_compile_options(${target} PRIVATE -Werror)
  elseif(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    # Keep consumer FetchContent tolerant of compiler drift
    target_compile_options(${target} PRIVATE -Werror=switch)
  endif()
  if(WIN32)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
endfunction()

function(arx_target_fuzz_instrumentation target)
  # fuzzer-no-link instruments library TUs without replacing the executable entry point
  target_compile_options(${target} PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-fsanitize=fuzzer-no-link,address,undefined>
    $<$<CONFIG:RelWithDebInfo>:-fno-sanitize-recover=undefined>)
  target_link_options(${target} PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-fsanitize=address,undefined>
    $<$<CONFIG:RelWithDebInfo>:-fno-sanitize-recover=undefined>)
endfunction()

function(arx_target_sanitizers target)
  if(ARX_FUZZING)
    arx_target_fuzz_instrumentation(${target})
  elseif(ARX_COVERAGE)
    # ASan interceptors corrupt profile dumps on abnormal exit
    target_compile_options(${target} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(${target}    PRIVATE -fprofile-instr-generate)
  elseif(ARX_SANITIZERS)
    target_compile_options(${target} PRIVATE $<$<CONFIG:RelWithDebInfo>:-fsanitize=address,undefined>)
    target_link_options(${target}    PRIVATE $<$<CONFIG:RelWithDebInfo>:-fsanitize=address,undefined>)
  endif()
endfunction()

function(arx_target_defaults target)
  arx_target_warnings(${target})
  arx_target_sanitizers(${target})
endfunction()

function(arx_target_vendored target)
  set_target_properties(${target} PROPERTIES C_CLANG_TIDY "" CXX_CLANG_TIDY "")
  if(MSVC)
    target_compile_options(${target} PRIVATE /w)
  else()
    target_compile_options(${target} PRIVATE -w)
  endif()
endfunction()

function(arx_fuzz_target_defaults target)
  arx_target_warnings(${target})
  target_compile_options(${target} PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-fsanitize=fuzzer,address,undefined>
    $<$<CONFIG:RelWithDebInfo>:-fno-sanitize-recover=undefined>)
  target_link_options(${target} PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-fsanitize=fuzzer,address,undefined>
    $<$<CONFIG:RelWithDebInfo>:-fno-sanitize-recover=undefined>)
endfunction()

configure_file(src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)

# Hot-reload ABI tripwire: SHA256 of public headers, baked into the binary
# Consumer computes the same hash; mismatch at reload = refuse
function(arx_compute_layout_hash OUT_VAR)
    file(GLOB_RECURSE _hdrs CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/arx_pistoris/*.h")
    list(SORT _hdrs)
    set(_concat "")
    foreach(_h IN LISTS _hdrs)
        # standalone only: FetchContent parent already tracks these, double-registering collides
        if(PROJECT_IS_TOP_LEVEL)
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_h}")
        endif()
        file(READ "${_h}" _c)
        string(APPEND _concat "${_c}")
    endforeach()
    string(SHA256 _hash "${_concat}")
    set(${OUT_VAR} "${_hash}" PARENT_SCOPE)
endfunction()

arx_compute_layout_hash(ARX_PISTORIS_LAYOUT_HASH)
message(STATUS "arx_pistoris layout hash: ${ARX_PISTORIS_LAYOUT_HASH}")
configure_file(src/layout_hash_gen.h.in ${CMAKE_CURRENT_BINARY_DIR}/layout_hash_gen.h @ONLY)

# --- Build time info ---
# OFF skips the per-build regeneration to avoid relink thrash on non-deliverable presets
# (coverage, fuzz). Stub is written once at configure time
option(ARX_BUILD_INFO_LIVE "Regenerate build_info.cpp on every build" ON)
set(ARX_BUILD_INFO_CPP ${CMAKE_CURRENT_BINARY_DIR}/build_info.cpp)
if(ARX_BUILD_INFO_LIVE)
  add_custom_target(arx_build_info_gen
      BYPRODUCTS ${ARX_BUILD_INFO_CPP}
      COMMAND ${CMAKE_COMMAND}
          -DOUT=${ARX_BUILD_INFO_CPP}
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/write_build_info.cmake
      COMMENT "Regenerating build_info.cpp"
      VERBATIM
  )
  set_source_files_properties(${ARX_BUILD_INFO_CPP} PROPERTIES GENERATED TRUE)
else()
  string(TIMESTAMP _arx_stamp "%Y-%m-%dT%H:%M:%SZ" UTC)
  file(WRITE ${ARX_BUILD_INFO_CPP}
       "namespace pistoris { const char* generatedBuildTime() { return \"${_arx_stamp} (reconfig)\"; } }\n")
endif()

# --- ASan runtime DLL ---
# clang_rt.asan_dynamic lives in clang's resource dir; copy to bin/ so doctest
# discovery and manual runs find it without PATH gymnastics
if(WIN32 AND (ARX_SANITIZERS OR ARX_FUZZING))
  execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} --print-resource-dir
    OUTPUT_VARIABLE ARX_CLANG_RESOURCE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  find_file(ARX_ASAN_DLL
    NAMES clang_rt.asan_dynamic-x86_64.dll
    PATHS "${ARX_CLANG_RESOURCE_DIR}/lib/windows"
    NO_DEFAULT_PATH
  )
  if(ARX_ASAN_DLL)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    file(COPY "${ARX_ASAN_DLL}" DESTINATION "${CMAKE_BINARY_DIR}/bin")
  endif()
endif()

# --- Vendored dependencies ---
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

add_library(mapbox_earcut INTERFACE)
target_include_directories(mapbox_earcut SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

add_library(arx_pistoris_pkware STATIC
  third_party/blast/blast.c
  third_party/pklib/explode.c
  third_party/pklib/implode.c
)
set_target_properties(arx_pistoris_pkware PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  C_VISIBILITY_PRESET hidden
)
arx_target_vendored(arx_pistoris_pkware)
target_include_directories(arx_pistoris_pkware SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
if(ARX_FUZZING)
  arx_target_fuzz_instrumentation(arx_pistoris_pkware)
elseif(ARX_COVERAGE)
  target_compile_options(arx_pistoris_pkware PRIVATE -fno-coverage-mapping)
elseif(ARX_SANITIZERS)
  target_compile_options(arx_pistoris_pkware PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-fsanitize=address,undefined>)
  target_link_options(arx_pistoris_pkware PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-fsanitize=address,undefined>)
endif()
if(WIN32)
  target_compile_definitions(arx_pistoris_pkware PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_library(arx_pistoris_cgltf STATIC src/external/glb/cgltf_impl.cpp)
set_target_properties(arx_pistoris_cgltf PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
)
arx_target_vendored(arx_pistoris_cgltf)
target_include_directories(arx_pistoris_cgltf SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
if(ARX_FUZZING)
  arx_target_fuzz_instrumentation(arx_pistoris_cgltf)
elseif(ARX_COVERAGE)
  target_compile_options(arx_pistoris_cgltf PRIVATE -fno-coverage-mapping)
elseif(ARX_SANITIZERS)
  target_compile_options(arx_pistoris_cgltf PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-fsanitize=address,undefined>)
  target_link_options(arx_pistoris_cgltf PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-fsanitize=address,undefined>)
endif()
if(WIN32)
  target_compile_definitions(arx_pistoris_cgltf PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_library(arx_pistoris_stb STATIC src/utils/stb_impl.cpp)
set_target_properties(arx_pistoris_stb PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  CXX_VISIBILITY_PRESET hidden
)
arx_target_vendored(arx_pistoris_stb)
target_include_directories(arx_pistoris_stb SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
if(ARX_FUZZING)
  arx_target_fuzz_instrumentation(arx_pistoris_stb)
elseif(ARX_COVERAGE)
  target_compile_options(arx_pistoris_stb PRIVATE -fno-coverage-mapping)
elseif(ARX_SANITIZERS)
  target_compile_options(arx_pistoris_stb PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-fsanitize=address,undefined>)
  target_link_options(arx_pistoris_stb PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-fsanitize=address,undefined>)
endif()

add_library(arx_pistoris_audio_codecs STATIC
  src/utils/audio_codecs_impl.cpp
  third_party/stb/stb_vorbis.c
)
set_target_properties(arx_pistoris_audio_codecs PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  C_VISIBILITY_PRESET hidden
  CXX_VISIBILITY_PRESET hidden
)
arx_target_vendored(arx_pistoris_audio_codecs)
target_compile_definitions(arx_pistoris_audio_codecs PRIVATE STB_VORBIS_NO_STDIO)
target_include_directories(arx_pistoris_audio_codecs SYSTEM PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
if(ARX_FUZZING)
  arx_target_fuzz_instrumentation(arx_pistoris_audio_codecs)
elseif(ARX_COVERAGE)
  target_compile_options(arx_pistoris_audio_codecs PRIVATE -fno-coverage-mapping)
elseif(ARX_SANITIZERS)
  target_compile_options(arx_pistoris_audio_codecs PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-fsanitize=address,undefined>)
  target_link_options(arx_pistoris_audio_codecs PRIVATE
    $<$<CONFIG:RelWithDebInfo>:-fsanitize=address,undefined>)
endif()
if(WIN32)
  target_compile_definitions(arx_pistoris_audio_codecs PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
if(WIN32)
  target_compile_definitions(arx_pistoris_stb PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
include(third_party/doctest/doctest.cmake)

add_subdirectory(src)
add_subdirectory(cli)

# --- Tests ---
# Doctest discovery cannot find the ASan runtime in fuzz builds
if(BUILD_TESTING AND NOT ARX_FUZZING)

file(GLOB ARX_UNIT_TEST_SRCS   CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/*.cpp")
file(GLOB ARX_UNIT_MODULE_TEST_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/modules/*.cpp")
file(GLOB ARX_C_TEST_SRCS      CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/api/*.cpp")
file(GLOB ARX_CPP_TEST_SRCS    CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp_api/*.cpp")
file(GLOB ARX_CORPUS_TEST_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/*.cpp")
file(GLOB ARX_CPP_CORPUS_TEST_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp_corpus/*.cpp")
file(GLOB ARX_CLI_CORPUS_TEST_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/cli_corpus/*.cpp")

file(GLOB_RECURSE ARX_PUBLIC_C_HEADERS CONFIGURE_DEPENDS
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/arx_pistoris/*.h"
)
file(GLOB_RECURSE ARX_PUBLIC_CPP_HEADERS CONFIGURE_DEPENDS
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/arx_pistoris/*.hpp"
)
set(ARX_HEADER_CHECK_DIR "${CMAKE_CURRENT_BINARY_DIR}/header-self-containment")
set(ARX_HEADER_CHECK_SOURCES)
foreach(header IN LISTS ARX_PUBLIC_C_HEADERS)
  string(MAKE_C_IDENTIFIER "${header}" source_name)
  set(source "${ARX_HEADER_CHECK_DIR}/${source_name}.c")
  file(GENERATE OUTPUT "${source}" CONTENT "#include \"${header}\"\ntypedef int arx_header_self_containment;\n")
  list(APPEND ARX_HEADER_CHECK_SOURCES "${source}")
endforeach()
foreach(header IN LISTS ARX_PUBLIC_CPP_HEADERS)
  string(MAKE_C_IDENTIFIER "${header}" source_name)
  set(source "${ARX_HEADER_CHECK_DIR}/${source_name}.cpp")
  file(GENERATE OUTPUT "${source}" CONTENT "#include \"${header}\"\ntypedef int arx_header_self_containment;\n")
  list(APPEND ARX_HEADER_CHECK_SOURCES "${source}")
endforeach()
add_library(arx_pistoris_header_self_containment OBJECT ${ARX_HEADER_CHECK_SOURCES})
target_include_directories(arx_pistoris_header_self_containment PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(arx_pistoris_header_self_containment PRIVATE c_std_11 cxx_std_20)
target_compile_definitions(arx_pistoris_header_self_containment PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:ARX_PISTORIS_CPP_API>
  $<$<COMPILE_LANGUAGE:CXX>:ARX_PISTORIS_ENABLE_LEVEL_DEBUG_API>
)
arx_target_defaults(arx_pistoris_header_self_containment)
set_target_properties(arx_pistoris_header_self_containment PROPERTIES C_CLANG_TIDY "" CXX_CLANG_TIDY "")

# Unit tests: static wrapper + src/ on include path (internal headers)
add_executable(arx_pistoris_unit_tests ${ARX_UNIT_TEST_SRCS} ${ARX_UNIT_MODULE_TEST_SRCS})
target_link_libraries(arx_pistoris_unit_tests PRIVATE arx_pistoris_cli_support arx_pistoris_cpp doctest nlohmann_json)
target_compile_definitions(arx_pistoris_unit_tests PRIVATE ARX_PISTORIS_ENABLE_LEVEL_DEBUG_API)
target_include_directories(arx_pistoris_unit_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/cli
  ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
arx_target_defaults(arx_pistoris_unit_tests)

# C API tests: shared lib only, no src/ - enforces public-headers-only
add_executable(arx_pistoris_c_tests ${ARX_C_TEST_SRCS})
target_link_libraries(arx_pistoris_c_tests PRIVATE arx_pistoris_c doctest)
target_include_directories(arx_pistoris_c_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
arx_target_defaults(arx_pistoris_c_tests)

add_executable(arx_pistoris_c_smoke tests/api/c_smoke.c)
target_link_libraries(arx_pistoris_c_smoke PRIVATE arx_pistoris_c)
arx_target_defaults(arx_pistoris_c_smoke)

# C++ API tests: static compile-time API, no src/ - enforces public-headers-only
add_executable(arx_pistoris_cpp_tests ${ARX_CPP_TEST_SRCS})
target_link_libraries(arx_pistoris_cpp_tests PRIVATE arx_pistoris_cpp doctest)
target_compile_definitions(arx_pistoris_cpp_tests PRIVATE ARX_PISTORIS_ENABLE_LEVEL_DEBUG_API)
target_include_directories(arx_pistoris_cpp_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
arx_target_defaults(arx_pistoris_cpp_tests)

# Corpus tests: real-file and limit tests, slow
add_executable(arx_pistoris_corpus_tests ${ARX_CORPUS_TEST_SRCS})
target_link_libraries(arx_pistoris_corpus_tests PRIVATE arx_pistoris_c doctest nlohmann_json)
target_include_directories(arx_pistoris_corpus_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
arx_target_defaults(arx_pistoris_corpus_tests)

add_executable(arx_pistoris_cpp_corpus_tests ${ARX_CPP_CORPUS_TEST_SRCS})
target_link_libraries(arx_pistoris_cpp_corpus_tests PRIVATE arx_pistoris_cpp doctest nlohmann_json)
target_include_directories(arx_pistoris_cpp_corpus_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
arx_target_defaults(arx_pistoris_cpp_corpus_tests)

add_executable(arx_pistoris_cli_corpus_tests ${ARX_CLI_CORPUS_TEST_SRCS})
target_link_libraries(arx_pistoris_cli_corpus_tests PRIVATE
  arx_pistoris_cli_support
  arx_pistoris_cpp
  doctest
  nlohmann_json
)
target_include_directories(arx_pistoris_cli_corpus_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/cli
  ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
arx_target_defaults(arx_pistoris_cli_corpus_tests)

doctest_discover_tests(arx_pistoris_unit_tests   ADD_LABELS 1 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
doctest_discover_tests(arx_pistoris_cpp_tests    ADD_LABELS 1 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
doctest_discover_tests(arx_pistoris_cpp_corpus_tests ADD_LABELS 1 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
doctest_discover_tests(arx_pistoris_cli_corpus_tests ADD_LABELS 1 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
add_test(NAME arx_pistoris_c_tests
  COMMAND arx_pistoris_c_tests
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_tests_properties(arx_pistoris_c_tests PROPERTIES LABELS "arx_pistoris_c_tests")
add_test(NAME arx_pistoris_c_smoke
  COMMAND arx_pistoris_c_smoke
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_tests_properties(arx_pistoris_c_smoke PROPERTIES LABELS "arx_pistoris_c_tests")
add_test(NAME arx_pistoris_corpus_tests
  COMMAND arx_pistoris_corpus_tests
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_tests_properties(arx_pistoris_corpus_tests PROPERTIES LABELS "arx_pistoris_corpus_tests")
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME ReturnCodeCoverage
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/check_rc_coverage.py
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(ReturnCodeCoverage PROPERTIES LABELS "coverage")
add_test(NAME HeaderBoundaries
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/check_header_boundaries.py
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(HeaderBoundaries PROPERTIES LABELS "architecture")
add_test(NAME CliContract
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/cli_contract_tests.py $<TARGET_FILE:arx_pistoris_cli>
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(CliContract PROPERTIES LABELS "cli")
add_custom_target(regenerate-fixtures
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/regenerate_fixture_mount.py
    --cli $<TARGET_FILE:arx_pistoris_cli>
  DEPENDS arx_pistoris_cli
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  USES_TERMINAL
  VERBATIM)

endif()  # BUILD_TESTING AND NOT ARX_FUZZING

install(TARGETS arx_pistoris_cli
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES LICENSE ADDITIONAL_TERMS AUTHORS ARX_LIBERTATIS_AUTHORS README.md
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
)
install(DIRECTORY docs/
  DESTINATION ${CMAKE_INSTALL_DOCDIR}/docs
  FILES_MATCHING
  PATTERN "*.md"
  PATTERN "TESTING.md" EXCLUDE
)
install(FILES third_party/cgltf/LICENSE
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  RENAME cgltf-LICENSE
)
install(FILES third_party/stb/LICENSE
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  RENAME stb-LICENSE
)
install(FILES third_party/dr_libs/LICENSE
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  RENAME dr_libs-LICENSE
)
install(FILES third_party/blast/LICENSE
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  RENAME blast-LICENSE
)
install(FILES third_party/pklib/LICENSE
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  RENAME pklib-LICENSE
)
install(FILES third_party/nlohmann/LICENSE.MIT
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  RENAME nlohmann-json-LICENSE
)
install(FILES third_party/mapbox/LICENSE
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  RENAME mapbox-earcut-LICENSE
)

# --- Format target ---
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
  file(GLOB_RECURSE ALL_SOURCE_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp    ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/include/arx_pistoris/*.h
    ${CMAKE_SOURCE_DIR}/include/arx_pistoris/*.hpp
    ${CMAKE_SOURCE_DIR}/tests/*.c    ${CMAKE_SOURCE_DIR}/tests/*.cpp  ${CMAKE_SOURCE_DIR}/tests/*.h
    ${CMAKE_SOURCE_DIR}/cli/*.cpp    ${CMAKE_SOURCE_DIR}/cli/*.h
    ${CMAKE_SOURCE_DIR}/fuzz/*.cpp     ${CMAKE_SOURCE_DIR}/fuzz/*.h
  )
  string(REPLACE ";" "\n" ARX_FORMAT_FILE_LIST_CONTENT "${ALL_SOURCE_FILES}")
  set(ARX_FORMAT_FILE_LIST "${CMAKE_CURRENT_BINARY_DIR}/format-files.txt")
  file(WRITE "${ARX_FORMAT_FILE_LIST}" "${ARX_FORMAT_FILE_LIST_CONTENT}\n")
  add_custom_target(format
    COMMAND ${CLANG_FORMAT} -i --files=${ARX_FORMAT_FILE_LIST}
    COMMENT "clang-format"
  )
  add_custom_target(format-check
    COMMAND ${CLANG_FORMAT} --dry-run --Werror --files=${ARX_FORMAT_FILE_LIST}
    COMMENT "clang-format check"
  )
else()
  message(WARNING "clang-format not found; format and format-check targets unavailable")
endif()

# --- Fuzz targets ---
function(arx_add_fuzz_target name source seed_directory)
  cmake_parse_arguments(PARSE_ARGV 3 ARX_FUZZ "" "DICTIONARY" "")
  if(ARX_FUZZ_KEYWORDS_MISSING_VALUES)
    message(FATAL_ERROR "Missing values for fuzz target ${name}: ${ARX_FUZZ_KEYWORDS_MISSING_VALUES}")
  endif()
  if(ARX_FUZZ_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "Unknown arguments for fuzz target ${name}: ${ARX_FUZZ_UNPARSED_ARGUMENTS}")
  endif()

  add_executable(${name} ${source})
  target_compile_definitions(${name} PRIVATE ARX_PISTORIS_EXPORTS)
  target_link_libraries(${name} PRIVATE arx_pistoris_fuzz_api)
  arx_fuzz_target_defaults(${name})
  add_dependencies(${name} arx_fuzz_seeds)

  if(ARX_FUZZ_DICTIONARY)
    set(dictionary_path "${CMAKE_CURRENT_SOURCE_DIR}/${ARX_FUZZ_DICTIONARY}")
    if(NOT EXISTS "${dictionary_path}" OR IS_DIRECTORY "${dictionary_path}")
      message(FATAL_ERROR "Dictionary for fuzz target ${name} is not a file: ${dictionary_path}")
    endif()
  endif()

  if(BUILD_TESTING)
    set(smoke_arguments -runs=1 -verbosity=0)
    if(ARX_FUZZ_DICTIONARY)
      list(APPEND smoke_arguments "-dict=${dictionary_path}")
    endif()
    list(APPEND smoke_arguments "${FUZZ_SEED_DIR}/${seed_directory}")
    add_test(NAME ${name}_smoke
      COMMAND $<TARGET_FILE:${name}> ${smoke_arguments})
    set_tests_properties(${name}_smoke PROPERTIES
      LABELS fuzz-smoke
      TIMEOUT 120
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
  endif()
endfunction()

if(ARX_FUZZING)
  add_library(arx_pistoris_fuzz_api STATIC src/api/binary.cpp src/api/strerror.cpp ${ARX_PISTORIS_C_API_SOURCES})
  target_compile_definitions(arx_pistoris_fuzz_api PRIVATE ARX_PISTORIS_EXPORTS)
  target_include_directories(arx_pistoris_fuzz_api
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_BINARY_DIR}
  )
  target_link_libraries(arx_pistoris_fuzz_api PRIVATE ${ARX_PISTORIS_INTERNAL_TARGETS}
                                                       ${ARX_PISTORIS_INTERNAL_DEPENDENCIES})
  arx_target_defaults(arx_pistoris_fuzz_api)

  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(FUZZ_SEED_DIR "${CMAKE_BINARY_DIR}/fuzz-seeds")
  add_custom_target(arx_fuzz_seeds
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/prepare_fixture_seeds.py
              ${CMAKE_CURRENT_SOURCE_DIR}/data/fixtures/catalog.json ${FUZZ_SEED_DIR}
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/destructure_glb.py
              ${FUZZ_SEED_DIR}/model-glb-source ${FUZZ_SEED_DIR}/model-glb
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/destructure_glb.py
              ${FUZZ_SEED_DIR}/level-glb-source ${FUZZ_SEED_DIR}/level-glb
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/destructure_glb.py
              ${FUZZ_SEED_DIR}/ambiance-glb-source ${FUZZ_SEED_DIR}/ambiance-glb
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/destructure_glb.py
              ${FUZZ_SEED_DIR}/cinematic-glb-source ${FUZZ_SEED_DIR}/cinematic-glb
      COMMENT "Preparing fixture-derived fuzzer seeds"
      VERBATIM
  )

  arx_add_fuzz_target(arx_pistoris_ftl_fuzz                     fuzz/ftl_fuzz.cpp                     ftl
                      DICTIONARY fuzz/ftl.dict)
  arx_add_fuzz_target(arx_pistoris_ftl_roundtrip_fuzz           fuzz/ftl_roundtrip_fuzz.cpp           ftl
                      DICTIONARY fuzz/ftl.dict)
  arx_add_fuzz_target(arx_pistoris_model_native_import_fuzz     fuzz/model_native_import_fuzz.cpp     ftl
                      DICTIONARY fuzz/ftl.dict)
  arx_add_fuzz_target(arx_pistoris_tea_fuzz                     fuzz/tea_fuzz.cpp                     tea
                      DICTIONARY fuzz/tea.dict)
  arx_add_fuzz_target(arx_pistoris_tea_roundtrip_fuzz           fuzz/tea_roundtrip_fuzz.cpp           tea
                      DICTIONARY fuzz/tea.dict)
  arx_add_fuzz_target(arx_pistoris_animation_native_import_fuzz fuzz/animation_native_import_fuzz.cpp tea
                      DICTIONARY fuzz/tea.dict)
  arx_add_fuzz_target(arx_pistoris_amb_fuzz                     fuzz/amb_fuzz.cpp                     amb
                      DICTIONARY fuzz/amb.dict)
  arx_add_fuzz_target(arx_pistoris_amb_roundtrip_fuzz           fuzz/amb_roundtrip_fuzz.cpp           amb
                      DICTIONARY fuzz/amb.dict)
  arx_add_fuzz_target(arx_pistoris_ambiance_native_import_fuzz  fuzz/ambiance_native_import_fuzz.cpp  amb
                      DICTIONARY fuzz/amb.dict)
  arx_add_fuzz_target(arx_pistoris_cin_fuzz                     fuzz/cin_fuzz.cpp                     cin
                      DICTIONARY fuzz/cin.dict)
  arx_add_fuzz_target(arx_pistoris_cin_roundtrip_fuzz           fuzz/cin_roundtrip_fuzz.cpp           cin
                      DICTIONARY fuzz/cin.dict)
  arx_add_fuzz_target(arx_pistoris_cinematic_native_import_fuzz fuzz/cinematic_native_import_fuzz.cpp cin
                      DICTIONARY fuzz/cin.dict)
  arx_add_fuzz_target(arx_pistoris_obj_fuzz                     fuzz/obj_fuzz.cpp                     obj
                      DICTIONARY fuzz/obj.dict)
  arx_add_fuzz_target(arx_pistoris_obj_with_mtl_fuzz            fuzz/obj_with_mtl_fuzz.cpp            obj-mtl
                      DICTIONARY fuzz/obj.dict)
  arx_add_fuzz_target(arx_pistoris_glb_container_fuzz           fuzz/glb_container_fuzz.cpp           glb-container)
  target_include_directories(arx_pistoris_glb_container_fuzz PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(arx_pistoris_glb_container_fuzz PRIVATE arx_pistoris_cgltf)
  arx_add_fuzz_target(arx_pistoris_model_glb_import_fuzz        fuzz/model_glb_import_fuzz.cpp        model-glb
                      DICTIONARY fuzz/model-glb.dict)
  arx_add_fuzz_target(arx_pistoris_ambiance_glb_import_fuzz     fuzz/ambiance_glb_import_fuzz.cpp     ambiance-glb
                      DICTIONARY fuzz/ambiance-glb.dict)
  arx_add_fuzz_target(arx_pistoris_cinematic_glb_import_fuzz    fuzz/cinematic_glb_import_fuzz.cpp    cinematic-glb
                      DICTIONARY fuzz/cinematic-glb.dict)
  arx_add_fuzz_target(arx_pistoris_image_fuzz                   fuzz/image_fuzz.cpp                   image)
  arx_add_fuzz_target(arx_pistoris_audio_fuzz                   fuzz/audio_fuzz.cpp                   audio)
  arx_add_fuzz_target(arx_pistoris_fts_fuzz                     fuzz/fts_fuzz.cpp                     fts
                      DICTIONARY fuzz/fts.dict)
  arx_add_fuzz_target(arx_pistoris_fts_roundtrip_fuzz           fuzz/fts_roundtrip_fuzz.cpp           fts
                      DICTIONARY fuzz/fts.dict)
  arx_add_fuzz_target(arx_pistoris_llf_fuzz                     fuzz/llf_fuzz.cpp                     llf
                      DICTIONARY fuzz/llf.dict)
  arx_add_fuzz_target(arx_pistoris_llf_roundtrip_fuzz           fuzz/llf_roundtrip_fuzz.cpp           llf
                      DICTIONARY fuzz/llf.dict)
  arx_add_fuzz_target(arx_pistoris_dlf_fuzz                     fuzz/dlf_fuzz.cpp                     dlf
                      DICTIONARY fuzz/dlf.dict)
  arx_add_fuzz_target(arx_pistoris_dlf_roundtrip_fuzz           fuzz/dlf_roundtrip_fuzz.cpp           dlf
                      DICTIONARY fuzz/dlf.dict)
  arx_add_fuzz_target(arx_pistoris_level_native_fts_fuzz        fuzz/level_native_fts_fuzz.cpp        fts
                      DICTIONARY fuzz/fts.dict)
  arx_add_fuzz_target(arx_pistoris_level_native_llf_fuzz        fuzz/level_native_llf_fuzz.cpp        llf
                      DICTIONARY fuzz/llf.dict)
  arx_add_fuzz_target(arx_pistoris_level_native_dlf_fuzz        fuzz/level_native_dlf_fuzz.cpp        dlf
                      DICTIONARY fuzz/dlf.dict)
  arx_add_fuzz_target(arx_pistoris_level_native_dlf_embedded_fuzz
                                                               fuzz/level_native_dlf_embedded_fuzz.cpp dlf
                      DICTIONARY fuzz/dlf.dict)
  arx_add_fuzz_target(arx_pistoris_level_glb_import_fuzz        fuzz/level_glb_import_fuzz.cpp        level-glb
                      DICTIONARY fuzz/level-glb.dict)
endif()
