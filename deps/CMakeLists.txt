# Downloads and builds what PeerChalk links, at pinned versions, into CMAKE_INSTALL_PREFIX:
# p2p-discovery-connect's dependencies, Yrs and the math engine. Qt, Emscripten and Rust come from
# tools/setup.sh. Use the "deps" and "deps-web" presets.
cmake_minimum_required(VERSION 3.25)
project(peerchalk_dependencies LANGUAGES C CXX)

include(ExternalProject)
include("${CMAKE_CURRENT_LIST_DIR}/../cmake/P2pdcSource.cmake")

set(prefix "${CMAKE_INSTALL_PREFIX}")
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    message(FATAL_ERROR "Set CMAKE_INSTALL_PREFIX to the folder that receives the dependencies")
endif()
set(rust_environment ${CMAKE_COMMAND} -E env RUSTUP_TOOLCHAIN=1.96.1)
if(APPLE)
    # Rust reads the oldest macOS to support from the environment.
    list(APPEND rust_environment MACOSX_DEPLOYMENT_TARGET=13.0)
endif()
set(engine "${CMAKE_CURRENT_LIST_DIR}/../src/math/engine")

set(p2pdc_arguments -DP2PDC_BUILD_DEPENDENCIES=ON -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX=${prefix})
if(CMAKE_TOOLCHAIN_FILE)
    list(APPEND p2pdc_arguments -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE})
endif()
ExternalProject_Add(p2pdc_dependencies
    SOURCE_DIR "${p2pdc_SOURCE_DIR}"
    CMAKE_ARGS ${p2pdc_arguments}
    INSTALL_COMMAND "")

if(EMSCRIPTEN)
    set(yrs_target --target wasm32-unknown-emscripten)
    set(yrs_library <BINARY_DIR>/wasm32-unknown-emscripten/release/libyrs.a)
    set(yrs_patch PATCH_COMMAND git apply
        "${p2pdc_SOURCE_DIR}/examples/shared-text/yrs-0.28-emscripten.patch")
    set(engine_mode web)
    set(engine_install
        ${CMAKE_COMMAND} -E make_directory ${prefix}/math-worker
        COMMAND ${CMAKE_COMMAND} -E copy
            <BINARY_DIR>/wasm32-unknown-emscripten/release/peerchalk_math_worker.js
            <BINARY_DIR>/wasm32-unknown-emscripten/release/peerchalk_math_worker.wasm
            ${prefix}/math-worker)
else()
    set(yrs_target)
    set(yrs_library <BINARY_DIR>/release/${CMAKE_SHARED_LIBRARY_PREFIX}yrs${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(yrs_patch)
    set(engine_mode)
    set(engine_install
        ${CMAKE_COMMAND} -E make_directory ${prefix}/lib
        COMMAND ${CMAKE_COMMAND} -E copy <BINARY_DIR>/release/libpeerchalk_math.a ${prefix}/lib)
endif()

# Apps find a macOS library by the name recorded inside it, so it names its final place.
set(yrs_fix_name)
if(APPLE)
    set(yrs_fix_name COMMAND install_name_tool -id ${prefix}/lib/libyrs.dylib ${prefix}/lib/libyrs.dylib)
endif()

ExternalProject_Add(yrs
    GIT_REPOSITORY https://github.com/y-crdt/y-crdt.git
    GIT_TAG 23b7f5693bbf9e7d26340c521ee8647f79bdfba2 # v0.28.0
    UPDATE_DISCONNECTED TRUE
    ${yrs_patch}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${rust_environment} CARGO_TARGET_DIR=<BINARY_DIR>
        cargo build --manifest-path <SOURCE_DIR>/Cargo.toml --package yffi --locked --release
        ${yrs_target}
    INSTALL_COMMAND ${CMAKE_COMMAND} -E make_directory ${prefix}/include ${prefix}/lib
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/tests-ffi/include/libyrs.h ${prefix}/include
        COMMAND ${CMAKE_COMMAND} -E copy ${yrs_library} ${prefix}/lib
        ${yrs_fix_name})

ExternalProject_Add(math_engine
    SOURCE_DIR "${engine}"
    BUILD_ALWAYS TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${rust_environment} CARGO_TARGET_DIR=<BINARY_DIR>
        bash "${engine}/build.sh" ${engine_mode}
    INSTALL_COMMAND ${engine_install})
