# Qt Quick items and QML exposed as the PeerChalk module. The module is static,
# so the executable imports its plugin with Q_IMPORT_QML_PLUGIN(PeerChalkPlugin).
set_source_files_properties(Theme.qml Icons.qml Format.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_library(peerchalk_ui STATIC)
qt_add_qml_module(peerchalk_ui URI PeerChalk VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml/PeerChalk"
    SOURCES
        board_canvas.cpp board_canvas.hpp board_canvas_input.cpp
        board_minimap.cpp board_minimap.hpp
        conversions.hpp scene_nodes.cpp scene_nodes.hpp stroke_layer.cpp stroke_layer.hpp
        board_preview.cpp board_preview.hpp library.cpp library.hpp library_people.cpp
        library_boards.cpp board_document.cpp board_document.hpp
        ../sync/messages.cpp ../sync/messages.hpp ../sync/network.cpp ../sync/network.hpp
        ../sync/network_config.cpp ../sync/network_config.hpp ../sync/network_peers.cpp
        ../sync/network_sync.cpp
        row_model.cpp row_model.hpp
        app_style.cpp app_style.hpp
    QML_FILES
        Main.qml Theme.qml Icons.qml Format.qml Icon.qml Panel.qml RailButton.qml ToolRail.qml
        Inspector.qml SectionLabel.qml ColorSwatch.qml WidthButton.qml NavigatorPanel.qml
        BoardPage.qml HomePage.qml ProjectSidebar.qml ProjectHeader.qml WelcomePanel.qml
        BoardCard.qml IdentityPage.qml PageHeader.qml ProfileCard.qml PeopleCard.qml
        PersonRow.qml VerifyPanel.qml Avatar.qml PaperButton.qml TextInputDialog.qml
        ConfirmDialog.qml CodeDialog.qml Toast.qml MembersDialog.qml RemoteCursor.qml
        NetworkStatus.qml)
target_include_directories(peerchalk_ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
# The generated QML type registrations include headers by file name.
target_include_directories(peerchalk_ui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../sync")
target_link_libraries(peerchalk_ui PUBLIC peerchalk_board peerchalk_library p2pdc::p2pdc Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick
    Qt6::QuickControls2)
target_compile_options(peerchalk_ui PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:${peerchalk_warning_flags}>)
