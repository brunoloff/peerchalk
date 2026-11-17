find_package(GTest REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Test)
include(GoogleTest)

add_executable(peerchalk_board_tests stroke_test.cpp stroke_mesh_test.cpp viewport_test.cpp
    board_test.cpp)
target_link_libraries(peerchalk_board_tests PRIVATE peerchalk_board GTest::gtest_main)
target_compile_options(peerchalk_board_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
gtest_discover_tests(peerchalk_board_tests)

add_executable(peerchalk_ui_tests qt_test_main.cpp board_canvas_test.cpp board_minimap_test.cpp
    main_window_test.cpp)
target_link_libraries(peerchalk_ui_tests PRIVATE peerchalk_uiplugin peerchalk_ui Qt6::QuickControls2 Qt6::Test
    GTest::gtest)
target_compile_options(peerchalk_ui_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
gtest_discover_tests(peerchalk_ui_tests
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
