add_library(echo_logging STATIC LogService.cpp LogService.h)
target_include_directories(echo_logging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(echo_logging PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(
    echo_logging
    PUBLIC Qt6::Core Qt6::Gui
    PRIVATE echo_build_options echo_ffi
)
qt_extract_metatypes(echo_logging)
