cmake_minimum_required(VERSION 3.20)
# The release version lives in VERSION alone; builds add how far they are past it (BuildStamp).
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" peerchalk_version LIMIT_COUNT 1)
project(PeerChalk VERSION ${peerchalk_version} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# One shared Ninja pool for compiling and linking keeps builds from saturating the host, unless
# CMAKE_BUILD_PARALLEL_LEVEL asks for more.
set(peerchalk_heavy_jobs 1)
if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL})
    set(peerchalk_heavy_jobs "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
endif()
set_property(GLOBAL APPEND PROPERTY JOB_POOLS peerchalk_heavy=${peerchalk_heavy_jobs})
set(CMAKE_JOB_POOL_COMPILE peerchalk_heavy)
set(CMAKE_JOB_POOL_LINK peerchalk_heavy)

option(PEERCHALK_BUILD_DEPENDENCIES "Download and build only the dependencies; see deps/" OFF)
if(PEERCHALK_BUILD_DEPENDENCIES)
    add_subdirectory(deps)
    return()
endif()

include(CTest)

option(PEERCHALK_WERROR "Treat compiler warnings as errors in PeerChalk targets" ON)
set(peerchalk_warning_flags -Wall -Wextra -Wpedantic)
if(PEERCHALK_WERROR)
    list(APPEND peerchalk_warning_flags -Werror)
endif()

include(cmake/P2pdcSource.cmake)
# Build only the p2pdc library here; its own tests and tools belong to its repository.
set(peerchalk_build_testing ${BUILD_TESTING})
set(BUILD_TESTING OFF)
add_subdirectory("${p2pdc_SOURCE_DIR}" p2pdc EXCLUDE_FROM_ALL)
set(BUILD_TESTING ${peerchalk_build_testing})

# Yrs (the Rust port of Yjs) through its C interface holds the board document. The header and
# library must come from the same Yrs build for the target platform; see deps/.
find_path(PEERCHALK_YRS_INCLUDE_DIR libyrs.h)
find_library(PEERCHALK_YRS_LIBRARY NAMES yrs)
if(NOT PEERCHALK_YRS_INCLUDE_DIR OR NOT PEERCHALK_YRS_LIBRARY)
    message(FATAL_ERROR "Yrs not found. Build the dependencies as in docs/BUILDING.md, or set "
        "PEERCHALK_YRS_INCLUDE_DIR and PEERCHALK_YRS_LIBRARY.")
endif()
add_library(peerchalk_yrs UNKNOWN IMPORTED)
set_target_properties(peerchalk_yrs PROPERTIES
    IMPORTED_LOCATION "${PEERCHALK_YRS_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${PEERCHALK_YRS_INCLUDE_DIR}")

# PDFium shows, imports and exports PDFs, the same engine on every platform. Desktops link its
# library; browsers load its own module beside the app, reached through
# src/pdf/pdfium_web_bridge.cpp, so only its headers are needed here.
find_path(PEERCHALK_PDFIUM_INCLUDE_DIR fpdfview.h PATH_SUFFIXES pdfium)
if(EMSCRIPTEN)
    # The module sits in the dependency prefix's pdfium folder, beside include/pdfium.
    get_filename_component(peerchalk_pdfium_prefix "${PEERCHALK_PDFIUM_INCLUDE_DIR}/../.." ABSOLUTE)
    if(EXISTS "${peerchalk_pdfium_prefix}/pdfium/pdfium.wasm")
        set(peerchalk_pdfium_found TRUE)
    endif()
    add_library(peerchalk_pdfium INTERFACE)
else()
    find_library(PEERCHALK_PDFIUM_LIBRARY NAMES pdfium)
    set(peerchalk_pdfium_found ${PEERCHALK_PDFIUM_LIBRARY})
    add_library(peerchalk_pdfium UNKNOWN IMPORTED)
    set_target_properties(peerchalk_pdfium PROPERTIES IMPORTED_LOCATION "${PEERCHALK_PDFIUM_LIBRARY}")
endif()
if(NOT PEERCHALK_PDFIUM_INCLUDE_DIR OR NOT peerchalk_pdfium_found)
    message(FATAL_ERROR "PDFium not found. Build the dependencies as in docs/BUILDING.md.")
endif()
set_target_properties(peerchalk_pdfium PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PEERCHALK_PDFIUM_INCLUDE_DIR}")

# LaTeX layout through RaTeX. Browsers run it in a Web Worker instead (src/math/engine/web).
if(NOT EMSCRIPTEN)
    find_library(PEERCHALK_MATH_LIBRARY NAMES peerchalk_math)
    if(NOT PEERCHALK_MATH_LIBRARY)
        message(FATAL_ERROR "The math engine was not found. Build the dependencies as in "
            "docs/BUILDING.md, or set PEERCHALK_MATH_LIBRARY.")
    endif()
    add_library(peerchalk_math_engine UNKNOWN IMPORTED)
    set_target_properties(peerchalk_math_engine PROPERTIES
        IMPORTED_LOCATION "${PEERCHALK_MATH_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src/math/engine/include"
        INTERFACE_LINK_LIBRARIES "${CMAKE_DL_LIBS}")
    if(WIN32) # what Rust's standard library uses there, as rustc --print native-static-libs lists
        set_property(TARGET peerchalk_math_engine APPEND PROPERTY INTERFACE_LINK_LIBRARIES
            kernel32 advapi32 ntdll userenv ws2_32 dbghelp synchronization bcrypt)
    endif()
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Network Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)

add_subdirectory(src)
if(BUILD_TESTING AND NOT EMSCRIPTEN AND NOT ANDROID)
    add_subdirectory(tests)
endif()
