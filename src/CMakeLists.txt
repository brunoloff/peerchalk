# Board model: plain C++ with no Qt dependency, so it can be shared with sync code.
add_library(peerchalk_board STATIC board/stroke.cpp board/stroke_mesh.cpp board/viewport.cpp
    board/board.cpp board/board_text.cpp board/rich_text.cpp board/stroke_codec.cpp board/stroke_builder.cpp)
target_include_directories(peerchalk_board PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(peerchalk_board PRIVATE peerchalk_yrs)
target_compile_options(peerchalk_board PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)

# Local projects, boards and identity: Qt Core plus p2pdc for keys and secrets.
add_library(peerchalk_library STATIC library/codes.cpp library/files.cpp
    library/identity_store.cpp library/signatures.cpp library/user_identity.cpp
    library/access_log.cpp library/access_rules.cpp library/workspace.cpp
    library/workspace_roles.cpp library/workspace_io.cpp library/workspace_migration.cpp
    library/account_store.cpp library/account_import.cpp library/backup.cpp
    library/workspace_links.cpp
    $<IF:$<BOOL:${EMSCRIPTEN}>,library/instance_slot_web.cpp,library/instance_slot_native.cpp>)
target_include_directories(peerchalk_library PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(peerchalk_library SYSTEM PRIVATE "${SODIUM_INCLUDE_DIR}")
target_link_libraries(peerchalk_library PUBLIC Qt6::Core PRIVATE peerchalk_board p2pdc::p2pdc)
target_compile_options(peerchalk_library PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)

add_subdirectory(ui)

set(peerchalk_icons "${CMAKE_CURRENT_SOURCE_DIR}/../deploy/icons")
set(peerchalk_platform_sources)
if(WIN32)
    set(peerchalk_platform_sources app/peerchalk.rc)
elseif(APPLE)
    set(peerchalk_platform_sources "${peerchalk_icons}/peerchalk.icns")
    set_source_files_properties("${peerchalk_icons}/peerchalk.icns" PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
endif()
qt_add_executable(peerchalk WIN32 MACOSX_BUNDLE app/main.cpp app/app_controller.cpp
    app/app_controller.hpp app/smoke_test.cpp app/smoke_test.hpp ${peerchalk_platform_sources})
set_target_properties(peerchalk PROPERTIES
    OUTPUT_NAME $<IF:$<BOOL:${APPLE}>,PeerChalk,peerchalk>
    MACOSX_BUNDLE_GUI_IDENTIFIER io.github.brunoloff.PeerChalk
    MACOSX_BUNDLE_BUNDLE_NAME PeerChalk
    MACOSX_BUNDLE_ICON_FILE peerchalk.icns
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
target_link_libraries(peerchalk PRIVATE peerchalk_uiplugin peerchalk_ui p2pdc::p2pdc
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)
target_compile_options(peerchalk PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
if(EMSCRIPTEN)
    set(peerchalk_web_storage "${CMAKE_CURRENT_SOURCE_DIR}/app/web_storage.js")
    set(peerchalk_web_files "${CMAKE_CURRENT_SOURCE_DIR}/app/web_files.js")
    set(peerchalk_web_math "${CMAKE_CURRENT_SOURCE_DIR}/app/web_math.js")
    target_link_options(peerchalk PRIVATE -lidbfs.js "SHELL:--pre-js \"${peerchalk_web_storage}\""
        "SHELL:--pre-js \"${peerchalk_web_files}\"" "SHELL:--pre-js \"${peerchalk_web_math}\"")
    set_property(TARGET peerchalk APPEND PROPERTY LINK_DEPENDS "${peerchalk_web_storage}"
        "${peerchalk_web_files}" "${peerchalk_web_math}")
endif()

if(NOT EMSCRIPTEN)
    include(GNUInstallDirs)
    install(TARGETS peerchalk BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    if(UNIX AND NOT APPLE)
        set(peerchalk_linux "${CMAKE_CURRENT_SOURCE_DIR}/../deploy/linux")
        install(FILES "${peerchalk_linux}/io.github.brunoloff.PeerChalk.desktop"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
        install(FILES "${peerchalk_linux}/io.github.brunoloff.PeerChalk.metainfo.xml"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
        install(FILES "${peerchalk_icons}/peerchalk.svg"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
            RENAME io.github.brunoloff.PeerChalk.svg)
        install(FILES "${peerchalk_icons}/peerchalk-256.png"
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
            RENAME io.github.brunoloff.PeerChalk.png)
    endif()
endif()
