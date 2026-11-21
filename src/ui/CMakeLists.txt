# Qt Quick items and QML exposed as the PeerChalk module. The module is static,
# so the executable imports its plugin with Q_IMPORT_QML_PLUGIN(PeerChalkPlugin).
set_source_files_properties(Theme.qml Icons.qml Format.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_library(peerchalk_ui STATIC)
# Browsers save and choose files through web_files.cpp instead of system dialogs.
set(peerchalk_native_qml)
set(peerchalk_instance_link ../sync/instance_link_web.cpp)
set(peerchalk_math_engine_source ../math/math_engine_web.cpp)
if(NOT EMSCRIPTEN)
    set(peerchalk_native_qml NativeBackupFiles.qml NativeSaveFile.qml NativePdfFile.qml)
    set(peerchalk_instance_link ../sync/instance_link_native.cpp)
    set(peerchalk_math_engine_source ../math/math_engine_native.cpp)
endif()
qt_add_qml_module(peerchalk_ui URI PeerChalk VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/PeerChalk"
    SOURCES
        board_canvas.cpp board_canvas.hpp board_canvas_input.cpp board_canvas_text.cpp
        board_canvas_pages.cpp board_canvas_page_edits.cpp board_canvas_outline.cpp board_canvas_current.cpp
        board_canvas_pdf.cpp page_overlay_model.cpp
        page_overlay_model.hpp
        board_minimap.cpp board_minimap.hpp
        conversions.hpp scene_nodes.cpp scene_nodes.hpp stroke_layer.cpp stroke_layer.hpp
        page_scene.cpp page_scene.hpp pdf_renderer.cpp pdf_renderer.hpp library_pdfs.cpp
        pdf_textures.cpp pdf_textures.hpp
        board_preview.cpp board_preview.hpp library.cpp library.hpp library_people.cpp
        board_pdf_pages.cpp board_pdf_drawing.hpp
        desktop_launcher.cpp desktop_launcher.hpp app_updater.cpp app_updater.hpp
        library_boards.cpp library_links.cpp board_document.cpp board_document.hpp
        ../sync/messages.cpp ../sync/messages.hpp ../sync/network.cpp ../sync/network.hpp ../sync/network_blobs.cpp
        ../sync/network_config.cpp ../sync/network_config.hpp ../sync/network_peers.cpp
        ../sync/network_settings.cpp ../sync/network_settings.hpp ../sync/network_settings_page.cpp
        ../sync/network_live.cpp ../sync/live_strokes.cpp ../sync/live_strokes.hpp
        ../sync/network_sync.cpp ../sync/network_spaces.cpp ../sync/network_boards.cpp
        ../sync/message_parts.cpp ../sync/message_parts.hpp
        ../sync/network_instances.cpp ../sync/instance_link.hpp ${peerchalk_instance_link}
        ../sync/space_schedule.cpp ../sync/space_schedule.hpp
        ../sync/connection_log.cpp ../sync/connection_log.hpp ../sync/p2pdc_names.cpp
        ../sync/p2pdc_names.hpp ../sync/network_discovery.cpp ../sync/network_log.cpp
        ../math/math_engine.hpp ../math/math_result.cpp ../math/math_smoothing.cpp
        ../math/math_smoothing.hpp ${peerchalk_math_engine_source}
        math_item.cpp math_item.hpp text_box_model.cpp text_box_model.hpp
        rich_text_document.cpp rich_text_document.hpp rich_text_binding.cpp rich_text_binding.hpp
        rich_text_binding_equations.cpp equation_syntax.cpp equation_syntax.hpp equation_geometry.hpp
        board_pdf.cpp board_pdf.hpp board_exporter.cpp board_exporter.hpp
        row_model.cpp row_model.hpp
        app_style.cpp app_style.hpp
        account_manager.cpp account_manager.hpp web_files.cpp web_files.hpp
    QML_FILES
        Main.qml Theme.qml Icons.qml Format.qml Icon.qml Panel.qml RailButton.qml ToolRail.qml
        Inspector.qml SectionLabel.qml ColorSwatch.qml WidthButton.qml NavigatorPanel.qml
        BoardPage.qml HomePage.qml ProjectSidebar.qml ProjectHeader.qml WelcomePanel.qml
        BoardCard.qml PeoplePane.qml PageHeader.qml ProfileCard.qml PeopleCard.qml
        PersonRow.qml VerifyPanel.qml Avatar.qml PaperButton.qml TextInputDialog.qml
        ConfirmDialog.qml CodeDialog.qml Toast.qml MembersDialog.qml RemoteCursor.qml
        NetworkStatus.qml PersonRoleRow.qml RequestsDialog.qml SettingsPage.qml
        NetworkPane.qml SettingField.qml PaperTab.qml ColorPalette.qml
        ChoiceChip.qml BoardKindCard.qml NewBoardDialog.qml PagePill.qml PageOverlay.qml
        PageGapControls.qml ColumnMenu.qml ColumnRuler.qml PageBoardChrome.qml
        PageOutline.qml
        SoftShadow.qml PaperMenu.qml PaperMenuItem.qml PaperMenuSeparator.qml
        PaperField.qml PaperSegments.qml PageSwatch.qml BackgroundTiles.qml DashedFrame.qml
        PdfExportFlow.qml PageInspector.qml PaperDialog.qml PaperButtonBox.qml
        PdfWaitingPill.qml
        AccountsWindow.qml AccountRow.qml AccountMenu.qml AccountChooser.qml ContinueButton.qml
        WelcomeLogo.qml WelcomeAbout.qml WelcomeFact.qml ReadyMark.qml UpdateStatus.qml UpdateBadge.qml
        AccountPane.qml BackupFlow.qml PassphraseDialog.qml
        BoardLinkSection.qml SiteLinks.qml EquationPanel.qml BoardShortcuts.qml TroubleshootingPane.qml
        TextBoxLayer.qml TextBoxView.qml TextBoxChrome.qml PdfExportButton.qml
        CreateAccountDialog.qml RemoveAccountDialog.qml ${peerchalk_native_qml})
# The app's icon, for the logo on the account screen.
set(peerchalk_logo "${CMAKE_CURRENT_SOURCE_DIR}/../../deploy/icons/peerchalk-512.png")
set_source_files_properties("${peerchalk_logo}" PROPERTIES QT_RESOURCE_ALIAS peerchalk-logo.png)
qt_add_resources(peerchalk_ui peerchalk_logo PREFIX /PeerChalk FILES "${peerchalk_logo}")
target_include_directories(peerchalk_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
# The generated QML type registrations include headers by file name.
target_include_directories(peerchalk_ui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../sync")
target_link_libraries(peerchalk_ui PUBLIC peerchalk_board peerchalk_library peerchalk_pdf p2pdc::p2pdc Qt6::Core Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick
    Qt6::QuickControls2)
if(NOT EMSCRIPTEN)
    target_link_libraries(peerchalk_ui PRIVATE peerchalk_math_engine)
endif()
target_compile_definitions(peerchalk_ui PRIVATE PEERCHALK_VERSION="${PROJECT_VERSION}")
target_compile_options(peerchalk_ui PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
