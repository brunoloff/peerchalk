# Downloads p2p-discovery-connect at the pinned commit. To work on both projects together, point
# FETCHCONTENT_SOURCE_DIR_P2PDC at a local checkout. SOURCE_SUBDIR names no folder so that this
# only downloads: the callers decide how the source is used.
include(FetchContent)
FetchContent_Declare(p2pdc
    GIT_REPOSITORY https://github.com/brunoloff/p2pdc.git
    GIT_TAG ad2c1f2b7f85f9f3bf553326e8d2334f16ff7baa
    SOURCE_SUBDIR download-only)
FetchContent_MakeAvailable(p2pdc)
