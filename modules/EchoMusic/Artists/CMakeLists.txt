qt_add_qml_module(
    echo_artists
    STATIC
    URI
    EchoMusic.Artists
    DEPENDENCIES
    EchoMusic.Music
    EchoMusic.Playback
    EchoMusic.Shell
    SOURCES
    ArtistDetailContext.h
    ArtistDetailContext.cpp
    ArtistDetailService.h
    ArtistDetailService.cpp
    QML_FILES
    ArtistDetailPage.qml
    ArtistDetailHeader.qml
)
target_link_libraries(
    echo_artists
    PUBLIC echo_account echo_music echo_shell echo_playback_service
    PRIVATE echo_bridge
            echo_playback
            echo_playbackplugin
            echo_shellplugin
            echo_musicplugin
            echo_theme
            echo_themeplugin
            echo_controls
            echo_controlsplugin
            echo_build_options
            Qt6::Quick
            Qt6::QuickControls2
)
target_include_directories(echo_artists PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
