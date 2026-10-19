# Canonical reverse-DNS application identity for bundle and desktop/portal
# declarations. Plugin manifest IDs are independent functional identifiers.
set(ECHO_APPLICATION_ID "io.github.hoowhoami.echomusicnext")

# Project warnings are private build requirements, not requirements imposed on
# consumers of a module. Targets opt in without global compiler flags.
add_library(echo_build_options INTERFACE)
if(MSVC)
    target_compile_options(echo_build_options INTERFACE /W4 /permissive-)
else()
    target_compile_options(echo_build_options INTERFACE -Wall -Wextra -Wpedantic)
endif()
