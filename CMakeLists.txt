cmake_minimum_required(VERSION 3.25)
project(arx-pistoris VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

option(ARX_COVERAGE "Build with Clang source-based coverage instrumentation" OFF)

function(arx_target_defaults target)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  # non-Release only - keep consumer FetchContent tolerant of compiler drift
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(${target} PRIVATE -Werror=switch)
  endif()
  if(ARX_FUZZING)
    # fuzzer-no-link instruments all TUs for coverage without replacing main
    target_compile_options(${target} PRIVATE $<$<CONFIG:RelWithDebInfo>:-fsanitize=fuzzer-no-link,address,undefined>)
    target_link_options(${target}    PRIVATE $<$<CONFIG:RelWithDebInfo>:-fsanitize=address,undefined>)
  elseif(ARX_COVERAGE)
    # ASan interceptors corrupt profile dumps on abnormal exit
    target_compile_options(${target} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
    target_link_options(${target}    PRIVATE -fprofile-instr-generate)
  else()
    target_compile_options(${target} PRIVATE $<$<CONFIG:RelWithDebInfo>:-fsanitize=address,undefined>)
    target_link_options(${target}    PRIVATE $<$<CONFIG:RelWithDebInfo>:-fsanitize=address,undefined>)
  endif()
  if(WIN32)
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
endfunction()

configure_file(src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h)

# Hot-reload ABI tripwire: SHA256 of public headers, baked into the binary.
# Consumer computes the same hash; mismatch at reload = refuse.
function(arx_compute_layout_hash OUT_VAR)
    set(_hdrs
        "${CMAKE_CURRENT_SOURCE_DIR}/include/arx_pistoris/pistoris_types.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/arx_pistoris/arx_pistoris.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/arx_pistoris/arx_math.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/arx_pistoris/common_data.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/arx_pistoris/ftl_data.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/arx_pistoris/tea_data.hpp")
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
# (coverage, fuzz). Stub is written once at configure time.
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
       "namespace pistoris { const char* buildTime() { return \"${_arx_stamp} (reconfig)\"; } }\n")
endif()

# --- ASan runtime DLL ---
# clang_rt.asan_dynamic lives in clang's resource dir; copy to bin/ so doctest
# discovery and manual runs find it without PATH gymnastics
if(WIN32)
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

add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
include(third_party/doctest/doctest.cmake)

# --- Internal engine (no ARX_API functions defined here) ---
file(GLOB_RECURSE ARX_INTERNAL_SRCS CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/arx/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/external/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp")

add_library(arx_pistoris_internals OBJECT
  ${ARX_INTERNAL_SRCS}
  ${ARX_BUILD_INFO_CPP}
)
set_target_properties(arx_pistoris_internals PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)
if(ARX_BUILD_INFO_LIVE)
  add_dependencies(arx_pistoris_internals arx_build_info_gen)
endif()
target_include_directories(arx_pistoris_internals
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(arx_pistoris_internals PRIVATE nlohmann_json)
arx_target_defaults(arx_pistoris_internals)

# strerror sits in its own TU; -fno-coverage-mapping keeps boilerplate dispatch out of the report
set(ARX_API_COMMON_SRCS src/api/api_helpers.cpp)
if(ARX_COVERAGE)
  set_source_files_properties(src/api/strerror.cpp PROPERTIES COMPILE_OPTIONS "-fno-coverage-mapping")
endif()

# --- C ABI shared library ---
add_library(arx_pistoris_c SHARED src/api/c_api.cpp src/api/strerror.cpp ${ARX_API_COMMON_SRCS})
target_compile_definitions(arx_pistoris_c PRIVATE ARX_PISTORIS_EXPORTS)
target_include_directories(arx_pistoris_c PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(arx_pistoris_c PUBLIC arx_pistoris_internals)
set_target_properties(arx_pistoris_c PROPERTIES OUTPUT_NAME arx_pistoris)
arx_target_defaults(arx_pistoris_c)

# --- C++ API static library ---
add_library(arx_pistoris_cpp STATIC src/api/cpp_api.cpp src/api/strerror.cpp ${ARX_API_COMMON_SRCS})
target_compile_definitions(arx_pistoris_cpp PUBLIC ARX_PISTORIS_CPP_API)
target_include_directories(arx_pistoris_cpp PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
)
target_link_libraries(arx_pistoris_cpp PUBLIC arx_pistoris_internals)
arx_target_defaults(arx_pistoris_cpp)

# --- Tests ---
# Skip when fuzzing (doctest discovery can't find the ASan runtime) or BUILD_TESTING=OFF
if(BUILD_TESTING AND NOT ARX_FUZZING)

file(GLOB ARX_UNIT_TEST_SRCS   CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/*.cpp")
file(GLOB ARX_C_TEST_SRCS      CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/api/*.cpp")
file(GLOB ARX_CPP_TEST_SRCS    CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/cpp_api/*.cpp")
file(GLOB ARX_CORPUS_TEST_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/*.cpp")

# Unit tests: static wrapper + src/ on include path (internal headers)
add_executable(arx_pistoris_unit_tests ${ARX_UNIT_TEST_SRCS})
target_link_libraries(arx_pistoris_unit_tests PRIVATE arx_pistoris_cpp doctest nlohmann_json)
target_include_directories(arx_pistoris_unit_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
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

# C++ API tests: static compile-time API, no src/ - enforces public-headers-only
add_executable(arx_pistoris_cpp_tests ${ARX_CPP_TEST_SRCS})
target_link_libraries(arx_pistoris_cpp_tests PRIVATE arx_pistoris_cpp doctest)
target_include_directories(arx_pistoris_cpp_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
arx_target_defaults(arx_pistoris_cpp_tests)

# Corpus tests: real-file and limit tests, slow
add_executable(arx_pistoris_corpus_tests ${ARX_CORPUS_TEST_SRCS})
target_link_libraries(arx_pistoris_corpus_tests PRIVATE arx_pistoris_c doctest)
target_include_directories(arx_pistoris_corpus_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/tests
)
arx_target_defaults(arx_pistoris_corpus_tests)

doctest_discover_tests(arx_pistoris_unit_tests   ADD_LABELS 1 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
doctest_discover_tests(arx_pistoris_c_tests      ADD_LABELS 1 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
doctest_discover_tests(arx_pistoris_cpp_tests    ADD_LABELS 1 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
doctest_discover_tests(arx_pistoris_corpus_tests ADD_LABELS 1 WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
  add_test(NAME ReturnCodeCoverage
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/check_rc_coverage.py
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(ReturnCodeCoverage PROPERTIES LABELS "coverage")
endif()

endif()  # BUILD_TESTING AND NOT ARX_FUZZING

# --- CLI ---
file(GLOB_RECURSE ARX_CLI_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cli/*.cpp")
add_executable(arx_pistoris_cli ${ARX_CLI_SRCS})
target_link_libraries(arx_pistoris_cli PRIVATE arx_pistoris_cpp)
target_include_directories(arx_pistoris_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/cli)
arx_target_defaults(arx_pistoris_cli)
target_link_options(arx_pistoris_cli PRIVATE
  $<$<AND:$<CONFIG:Release>,$<PLATFORM_ID:Linux>>:-static-libstdc++>
  $<$<AND:$<CONFIG:Release>,$<PLATFORM_ID:Linux>>:-static-libgcc>
)

set_target_properties(arx_pistoris_cli PROPERTIES OUTPUT_NAME arx-pistor)

install(TARGETS arx_pistoris_cli
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES LICENSE ADDITIONAL_TERMS README.md
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

# --- Format target ---
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
  file(GLOB_RECURSE ALL_SOURCE_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp    ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/include/arx_pistoris/*.h
    ${CMAKE_SOURCE_DIR}/include/arx_pistoris/*.hpp
    ${CMAKE_SOURCE_DIR}/tests/*.cpp  ${CMAKE_SOURCE_DIR}/tests/*.h
    ${CMAKE_SOURCE_DIR}/cli/*.cpp    ${CMAKE_SOURCE_DIR}/cli/*.h
    ${CMAKE_SOURCE_DIR}/fuzz/*.cpp
  )
  add_custom_target(format
    COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
    COMMENT "clang-format"
  )
else()
  message(WARNING "clang-format not found; format target unavailable")
endif()

# --- Fuzz targets ---
option(ARX_FUZZING "Build libFuzzer fuzz targets (requires clang -fsanitize=fuzzer)" OFF)

function(arx_add_fuzz_target name source)
  add_executable(${name} ${source})
  target_compile_definitions(${name} PRIVATE ARX_PISTORIS_EXPORTS)
  target_link_libraries(${name} PRIVATE arx_pistoris_fuzz_api)
  target_compile_options(${name} PRIVATE -fsanitize=fuzzer,address -g)
  target_link_options(${name}    PRIVATE -fsanitize=fuzzer,address)
  arx_target_defaults(${name})
endfunction()

if(ARX_FUZZING)
  if(WIN32)
    # MSVC STL annotations clash with clang_rt.fuzzer; disable to avoid linker mismatch
    add_compile_definitions(_DISABLE_STRING_ANNOTATION _DISABLE_VECTOR_ANNOTATION)
  endif()

  add_library(arx_pistoris_fuzz_api STATIC src/api/c_api.cpp src/api/strerror.cpp ${ARX_API_COMMON_SRCS})
  target_compile_definitions(arx_pistoris_fuzz_api PRIVATE ARX_PISTORIS_EXPORTS)
  target_include_directories(arx_pistoris_fuzz_api PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
  )
  target_link_libraries(arx_pistoris_fuzz_api PUBLIC arx_pistoris_internals)
  arx_target_defaults(arx_pistoris_fuzz_api)

  arx_add_fuzz_target(arx_pistoris_ftl_fuzz           fuzz/ftl_fuzz.cpp)
  arx_add_fuzz_target(arx_pistoris_ftl_roundtrip_fuzz fuzz/ftl_roundtrip_fuzz.cpp)
  arx_add_fuzz_target(arx_pistoris_tea_fuzz           fuzz/tea_fuzz.cpp)
  arx_add_fuzz_target(arx_pistoris_tea_roundtrip_fuzz fuzz/tea_roundtrip_fuzz.cpp)
  arx_add_fuzz_target(arx_pistoris_ftl_json_fuzz      fuzz/ftl_json_fuzz.cpp)
  arx_add_fuzz_target(arx_pistoris_tea_json_fuzz      fuzz/tea_json_fuzz.cpp)
  arx_add_fuzz_target(arx_pistoris_obj_fuzz           fuzz/obj_fuzz.cpp)
  arx_add_fuzz_target(arx_pistoris_obj_with_mtl_fuzz  fuzz/obj_with_mtl_fuzz.cpp)
  arx_add_fuzz_target(arx_pistoris_glb_import_fuzz    fuzz/glb_import_fuzz.cpp)

  # --- Seed Generation ---
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_FOUND)
    set(GLB_SEED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/fixtures/model/glb")
    set(GLB_OUT_DIR  "${CMAKE_BINARY_DIR}/fuzz-seeds/glb")

    add_custom_target(arx_fuzz_seeds
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/destructure_glb.py
                ${GLB_SEED_DIR} ${GLB_OUT_DIR}
        COMMENT "Destructuring GLB assets into fuzzer seeds"
        VERBATIM
    )
    add_dependencies(arx_pistoris_glb_import_fuzz arx_fuzz_seeds)
  endif()
endif()
