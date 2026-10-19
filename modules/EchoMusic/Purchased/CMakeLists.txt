qt_add_qml_module(
    echo_purchased
    STATIC
    URI
    EchoMusic.Purchased
    DEPENDENCIES
    EchoMusic.Controls
    EchoMusic.Music
    EchoMusic.Shell
    EchoMusic.Theme
    SOURCES
    PurchasedService.h
    PurchasedService.cpp
    QML_FILES
    PurchasedPage.qml
)
target_link_libraries(
    echo_purchased
    PUBLIC echo_music echo_shell echo_playback_service
    PRIVATE echo_bridge
            echo_build_options
            echo_theme
            echo_themeplugin
            echo_controls
            echo_controlsplugin
            echo_musicplugin
            echo_shellplugin
            echo_playback
            echo_playbackplugin
            Qt6::Quick
            Qt6::QuickControls2
)
target_include_directories(echo_purchased PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
