find_package(GTest REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Test GuiPrivate)
include(GoogleTest)

add_executable(peerchalk_board_tests stroke_test.cpp stroke_mesh_test.cpp viewport_test.cpp
    board_test.cpp board_text_test.cpp stroke_codec_test.cpp stroke_builder_test.cpp
    page_layout_test.cpp board_pages_test.cpp)
target_link_libraries(peerchalk_board_tests PRIVATE peerchalk_board GTest::gtest_main)
target_compile_options(peerchalk_board_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
gtest_discover_tests(peerchalk_board_tests)

add_executable(peerchalk_library_tests codes_test.cpp workspace_test.cpp
    identity_store_test.cpp signatures_test.cpp access_log_test.cpp files_test.cpp
    account_store_test.cpp backup_test.cpp instance_slot_test.cpp)
target_link_libraries(peerchalk_library_tests PRIVATE peerchalk_library GTest::gtest_main)
target_compile_options(peerchalk_library_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
gtest_discover_tests(peerchalk_library_tests)

add_executable(peerchalk_ui_tests qt_test_main.cpp board_canvas_test.cpp board_minimap_test.cpp
    main_window_test.cpp library_test.cpp account_manager_test.cpp math_engine_test.cpp rich_text_test.cpp board_pdf_test.cpp
    accounts_window_test.cpp desktop_launcher_test.cpp app_updater_test.cpp board_preview_test.cpp)
target_link_libraries(peerchalk_ui_tests PRIVATE peerchalk_uiplugin peerchalk_ui Qt6::QuickControls2 Qt6::Test Qt6::GuiPrivate
    GTest::gtest)
target_compile_options(peerchalk_ui_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
gtest_discover_tests(peerchalk_ui_tests
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

# End-to-end sync over p2pdc on a private OpenDHT network on the loopback interface.
find_package(PkgConfig REQUIRED)
pkg_check_modules(PEERCHALK_TEST_OPENDHT REQUIRED IMPORTED_TARGET opendht)
find_package(msgpack-cxx CONFIG REQUIRED)
add_executable(peerchalk_network_tests qt_test_main.cpp network_test.cpp messages_test.cpp
    network_settings_test.cpp live_strokes_test.cpp message_parts_test.cpp
    space_schedule_test.cpp instance_link_test.cpp connection_log_test.cpp)
target_link_libraries(peerchalk_network_tests PRIVATE peerchalk_ui Qt6::Test GTest::gtest
    PkgConfig::PEERCHALK_TEST_OPENDHT msgpack-cxx)
target_compile_options(peerchalk_network_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
gtest_discover_tests(peerchalk_network_tests
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300)
