find_package(GTest REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Test GuiPrivate)
include(GoogleTest)

add_executable(peerchalk_board_tests stroke_test.cpp stroke_mesh_test.cpp viewport_test.cpp
    board_test.cpp board_text_test.cpp stroke_codec_test.cpp stroke_builder_test.cpp
    page_layout_test.cpp page_map_test.cpp board_pages_test.cpp)
target_link_libraries(peerchalk_board_tests PRIVATE peerchalk_board GTest::gtest_main)
target_compile_options(peerchalk_board_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
gtest_discover_tests(peerchalk_board_tests)

add_executable(peerchalk_pdf_tests pdf_document_test.cpp)
target_link_libraries(peerchalk_pdf_tests PRIVATE peerchalk_pdf GTest::gtest_main)
target_compile_options(peerchalk_pdf_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
gtest_discover_tests(peerchalk_pdf_tests)

add_executable(peerchalk_library_tests codes_test.cpp workspace_test.cpp
    identity_store_test.cpp signatures_test.cpp access_log_test.cpp files_test.cpp
    account_store_test.cpp backup_test.cpp instance_slot_test.cpp blob_store_test.cpp)
target_link_libraries(peerchalk_library_tests PRIVATE peerchalk_library p2pdc::p2pdc GTest::gtest_main)
target_compile_options(peerchalk_library_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
gtest_discover_tests(peerchalk_library_tests)

add_executable(peerchalk_repository_tests repository_files_test.cpp repository_loader_test.cpp
    repository_writer_test.cpp board_tool_test.cpp)
target_link_libraries(peerchalk_repository_tests PRIVATE peerchalk_board_tool GTest::gtest_main)
target_compile_options(peerchalk_repository_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
gtest_discover_tests(peerchalk_repository_tests)

add_executable(peerchalk_ui_tests qt_test_main.cpp board_canvas_test.cpp board_canvas_pages_test.cpp board_minimap_test.cpp
    main_window_test.cpp library_test.cpp account_manager_test.cpp math_engine_test.cpp rich_text_test.cpp board_pdf_test.cpp
    accounts_window_test.cpp desktop_launcher_test.cpp app_updater_test.cpp board_preview_test.cpp
    pdf_renderer_test.cpp hosted_session_test.cpp embedded_window_test.cpp)
target_link_libraries(peerchalk_ui_tests PRIVATE peerchalk_uiplugin peerchalk_ui peerchalk_hosted Qt6::QuickControls2 Qt6::Test Qt6::GuiPrivate
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
    network_settings_test.cpp server_list_test.cpp stun_message_test.cpp server_checks_test.cpp live_strokes_test.cpp message_parts_test.cpp
    space_schedule_test.cpp instance_link_test.cpp connection_log_test.cpp scribe_test.cpp
    board_diagnosis_test.cpp)
target_link_libraries(peerchalk_network_tests PRIVATE peerchalk_ui Qt6::Test GTest::gtest
    PkgConfig::PEERCHALK_TEST_OPENDHT msgpack-cxx)
target_compile_options(peerchalk_network_tests PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
gtest_discover_tests(peerchalk_network_tests
    PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 300)

# Timings for a large page board (docs/page-boards.md); built, never run by ctest.
add_executable(peerchalk_page_bench page_board_bench.cpp)
target_link_libraries(peerchalk_page_bench PRIVATE peerchalk_ui peerchalk_pdf)
target_compile_options(peerchalk_page_bench PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)

# The version a build shows, from VERSION and git (cmake/BuildStamp.cmake).
add_test(NAME BuildStamp COMMAND ${CMAKE_COMMAND} -DSCRIPT=${PROJECT_SOURCE_DIR}/cmake/BuildStamp.cmake
    -DWORK=${CMAKE_CURRENT_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/build_stamp_test.cmake)
