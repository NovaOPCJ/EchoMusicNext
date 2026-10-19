qt_add_qml_module(
    echo_albums
    STATIC
    URI
    EchoMusic.Albums
    DEPENDENCIES
    EchoMusic.Details
    EchoMusic.Music
    EchoMusic.Playback
    EchoMusic.Shell
    SOURCES
    AlbumDetailContext.h
    AlbumDetailContext.cpp
    AlbumDetailService.h
    AlbumDetailService.cpp
    QML_FILES
    AlbumDetailPage.qml
    AlbumDetailHeader.qml
)
target_link_libraries(
    echo_albums
    PUBLIC echo_library echo_details echo_music echo_shell echo_playback_service
    PRIVATE echo_detailsplugin
            echo_bridge
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
target_include_directories(echo_albums PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
