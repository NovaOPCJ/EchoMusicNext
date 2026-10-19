include(FetchContent)
# Crashpad's maintained CMake integration. Every archive (including upstream
# submodules) is content-pinned; no branch, SDK telemetry or service is required.
FetchContent_Declare(
    echo_crashpad
    URL https://codeload.github.com/getsentry/crashpad/tar.gz/176efcdecd4275a672d9b45d92ae98f26ad288d8
    URL_HASH SHA256=ba9c296399d4b949666d47f23a6e9c6efb63a6f2ad041393116531d482d12768
    SOURCE_SUBDIR __populate_only DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(echo_crashpad)
FetchContent_Declare(
    echo_mini_chromium
    URL https://codeload.github.com/getsentry/mini_chromium/tar.gz/2f5c168fa462cf3836c4a7801b4f4955496d975d
    URL_HASH SHA256=00f60d55f810dd66a64cf882cb8f4d241809e9e14c24baaaa96031b66ca79ac4
    SOURCE_DIR "${echo_crashpad_SOURCE_DIR}/third_party/mini_chromium/mini_chromium" SOURCE_SUBDIR
    __populate_only DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_Declare(
    echo_crashpad_zlib
    URL https://codeload.github.com/getsentry/chromium-zlib/tar.gz/fef58692c1d7bec94c4ed3d030a45a1832a9615d
    URL_HASH SHA256=d49b88d645b17ccbad19406f95cd495527784b104b8fba1347a2475b08346224
    SOURCE_DIR "${echo_crashpad_SOURCE_DIR}/third_party/zlib/zlib" SOURCE_SUBDIR __populate_only
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_Declare(
    echo_crashpad_lss
    URL https://codeload.github.com/getsentry/chromium-linux-syscall-support/tar.gz/9719c1e1e676814c456b55f5f070eabad6709d31
    URL_HASH SHA256=a858367f0de0fd22a22172c562d750c29edb43f586cb6ff94be1941b77656bf3
    SOURCE_DIR "${echo_crashpad_SOURCE_DIR}/third_party/lss/lss" SOURCE_SUBDIR __populate_only
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(echo_mini_chromium echo_crashpad_zlib echo_crashpad_lss)
function(echo_add_crashpad)
    set(CMAKE_AUTOMOC OFF)
    set(CMAKE_AUTORCC OFF)
    set(CMAKE_AUTOUIC OFF)
    set(CRASHPAD_ENABLE_INSTALL
        OFF
        CACHE BOOL "" FORCE
    )
    set(CRASHPAD_ENABLE_INSTALL_DEV
        OFF
        CACHE BOOL "" FORCE
    )
    set(CRASHPAD_BUILD_TOOLS
        OFF
        CACHE BOOL "" FORCE
    )
    set(CRASHPAD_ENABLE_STACKTRACE
        OFF
        CACHE BOOL "" FORCE
    )
    # Follow upstream's platform defaults: OS zlib on Unix, bundled zlib on
    # Windows, where no system zlib runtime is part of the supported baseline.
    if(WIN32)
        set(CRASHPAD_ZLIB_SYSTEM
            OFF
            CACHE BOOL "" FORCE
        )
    else()
        set(CRASHPAD_ZLIB_SYSTEM
            ON
            CACHE BOOL "" FORCE
        )
    endif()
    add_subdirectory(
        "${echo_crashpad_SOURCE_DIR}" "${echo_crashpad_BINARY_DIR}" EXCLUDE_FROM_ALL SYSTEM
    )
endfunction()
echo_add_crashpad()
function(echo_copy_crash_runtime target)
    add_dependencies(${target} crashpad_handler)
    if(APPLE AND target STREQUAL "echomusicnext")
        set(destination "$<TARGET_BUNDLE_DIR:${target}>/Contents/Helpers")
    else()
        set(destination "$<TARGET_FILE_DIR:${target}>")
    endif()
    add_custom_command(
        TARGET ${target}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${destination}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:crashpad_handler>"
                "${destination}"
        VERBATIM
    )
    if(WIN32)
        add_dependencies(${target} crashpad_wer)
        add_custom_command(
            TARGET ${target}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:crashpad_wer>"
                    "${destination}"
            VERBATIM
        )
    endif()
endfunction()
