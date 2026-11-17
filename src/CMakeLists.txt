# Board model: plain C++ with no Qt dependency, so it can be shared with sync code.
add_library(peerchalk_board STATIC board/stroke.cpp board/stroke_mesh.cpp board/viewport.cpp
    board/board.cpp board/stroke_codec.cpp board/stroke_builder.cpp)
target_include_directories(peerchalk_board PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(peerchalk_board PRIVATE peerchalk_yrs)
target_compile_options(peerchalk_board PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)

# Local projects, boards and identity: Qt Core plus p2pdc for keys and secrets.
add_library(peerchalk_library STATIC library/codes.cpp library/files.cpp
    library/identity_store.cpp library/signatures.cpp library/user_identity.cpp
    library/access_log.cpp library/access_rules.cpp library/workspace.cpp
    library/workspace_roles.cpp library/workspace_io.cpp library/workspace_migration.cpp)
target_include_directories(peerchalk_library PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(peerchalk_library PUBLIC Qt6::Core PRIVATE p2pdc::p2pdc)
target_compile_options(peerchalk_library PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)

add_subdirectory(ui)

qt_add_executable(peerchalk app/main.cpp)
target_link_libraries(peerchalk PRIVATE peerchalk_uiplugin peerchalk_ui p2pdc::p2pdc
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
target_compile_options(peerchalk PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
