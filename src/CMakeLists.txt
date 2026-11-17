# Board model: plain C++ with no Qt dependency, so it can be shared with sync code.
add_library(peerchalk_board STATIC board/stroke.cpp board/stroke_mesh.cpp)
target_include_directories(peerchalk_board PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(peerchalk_board PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)

add_subdirectory(ui)

qt_add_executable(peerchalk app/main.cpp)
target_link_libraries(peerchalk PRIVATE peerchalk_uiplugin p2pdc::p2pdc
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
target_compile_options(peerchalk PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
