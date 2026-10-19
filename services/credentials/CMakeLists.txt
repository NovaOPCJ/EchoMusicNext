add_library(echo_credentials STATIC LocalCredentialStore.cpp LocalCredentialStore.h)
target_include_directories(echo_credentials PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(
    echo_credentials
    PUBLIC Qt6::Core
    PRIVATE Qt6::Sql echo_build_options
)
