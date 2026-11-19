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

# Linux records a library by its SONAME, which Yrs leaves out; without one, programs would
# record the library's path in this build tree instead.
set(yrs_command build)
set(yrs_soname)
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
elseif(WIN32)
    # Windows keeps the library next to the program and links through an import library.
    set(yrs_target)
    set(yrs_library <BINARY_DIR>/release/libyrs.dll.a)
    set(yrs_patch)
    set(engine_mode)
    set(engine_install
        ${CMAKE_COMMAND} -E make_directory ${prefix}/lib
        COMMAND ${CMAKE_COMMAND} -E copy <BINARY_DIR>/release/libpeerchalk_math.a ${prefix}/lib)
else()
    set(yrs_target)
    if(NOT APPLE)
        set(yrs_command rustc)
        set(yrs_soname --crate-type cdylib -- -C link-arg=-Wl,-soname,libyrs.so)
    endif()
    set(yrs_library <BINARY_DIR>/release/${CMAKE_SHARED_LIBRARY_PREFIX}yrs${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(yrs_patch)
    set(engine_mode)
    set(engine_install
        ${CMAKE_COMMAND} -E make_directory ${prefix}/lib
        COMMAND ${CMAKE_COMMAND} -E copy <BINARY_DIR>/release/libpeerchalk_math.a ${prefix}/lib)
endif()

# After installing: macOS finds a library by the name recorded inside it, so it names its
# final place; Windows keeps the library itself next to the program.
set(yrs_finish)
if(APPLE)
    set(yrs_finish COMMAND install_name_tool -id ${prefix}/lib/libyrs.dylib ${prefix}/lib/libyrs.dylib)
elseif(WIN32)
    set(yrs_finish COMMAND ${CMAKE_COMMAND} -E make_directory ${prefix}/bin
        COMMAND ${CMAKE_COMMAND} -E copy <BINARY_DIR>/release/yrs.dll ${prefix}/bin)
endif()

ExternalProject_Add(yrs
    GIT_REPOSITORY https://github.com/y-crdt/y-crdt.git
    GIT_TAG 23b7f5693bbf9e7d26340c521ee8647f79bdfba2 # v0.28.0
    UPDATE_DISCONNECTED TRUE
    ${yrs_patch}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${rust_environment} CARGO_TARGET_DIR=<BINARY_DIR>
        cargo ${yrs_command} --manifest-path <SOURCE_DIR>/Cargo.toml --package yffi --locked --release
        ${yrs_target} ${yrs_soname}
    INSTALL_COMMAND ${CMAKE_COMMAND} -E make_directory ${prefix}/include ${prefix}/lib
        COMMAND ${CMAKE_COMMAND} -E copy <SOURCE_DIR>/tests-ffi/include/libyrs.h ${prefix}/include
        COMMAND ${CMAKE_COMMAND} -E copy ${yrs_library} ${prefix}/lib
        ${yrs_finish})

ExternalProject_Add(math_engine
    SOURCE_DIR "${engine}"
    BUILD_ALWAYS TRUE
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${rust_environment} CARGO_TARGET_DIR=<BINARY_DIR>
        bash "${engine}/build.sh" ${engine_mode}
    INSTALL_COMMAND ${engine_install})

# The macOS build ignores Homebrew, whose libraries need newer macOS, so it builds GoogleTest
# for the tests.
if(APPLE)
    ExternalProject_Add(googletest
        URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
        URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        CMAKE_ARGS -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${prefix}
            -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0 -DBUILD_GMOCK=OFF -DINSTALL_GTEST=ON)
endif()
