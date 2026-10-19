add_library(echo_startup STATIC StartupController.cpp StartupController.h)
target_include_directories(echo_startup PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    echo_startup
    PUBLIC Qt6::Core
    PRIVATE echo_account echo_storage echo_network echo_playback_service echo_build_options
)
qt_extract_metatypes(echo_startup)
