add_library(echo_sharing STATIC ShareTarget.h ShareTarget.cpp SharingCommands.h SharingCommands.cpp)
target_include_directories(echo_sharing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    echo_sharing
    PUBLIC Qt6::Core Qt6::Gui
    PRIVATE echo_account echo_build_options
)
qt_extract_metatypes(echo_sharing)
