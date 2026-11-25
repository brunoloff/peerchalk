# Downloads p2p-discovery-connect at the pinned commit. To work on both projects together, point
# FETCHCONTENT_SOURCE_DIR_P2PDC at a local checkout. SOURCE_SUBDIR names no folder so that this
# only downloads: the callers decide how the source is used.
include(FetchContent)
FetchContent_Declare(p2pdc
    GIT_REPOSITORY https://github.com/brunoloff/p2pdc.git
    GIT_TAG 848dc13ae90353a9b190dafae958358a8f85e385
    SOURCE_SUBDIR download-only)
FetchContent_MakeAvailable(p2pdc)
