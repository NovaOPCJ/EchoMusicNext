# Qt's QNAM can consult macOS Internet Passwords before its public proxy-auth
# signal. Use libcurl for explicit-credential HTTP proxy transactions instead.
# TLS and route selection stay with Qt; no curl executable/sidecar is shipped.
include(FetchContent)
if(WIN32)
    enable_language(C)
    function(echo_setup_curl)
        set(BUILD_TESTING OFF)
        set(BUILD_SHARED_LIBS OFF)
        set(BUILD_CURL_EXE OFF)
        set(BUILD_STATIC_LIBS ON)
        set(BUILD_LIBCURL_DOCS OFF)
        set(BUILD_MISC_DOCS OFF)
        set(CURL_USE_SCHANNEL ON)
        set(CURL_USE_OPENSSL OFF)
        set(CURL_WINDOWS_SSPI ON)
        set(CURL_ENABLE_NTLM ON)
        set(CURL_USE_LIBPSL OFF)
        set(CURL_USE_LIBSSH2 OFF)
        set(CURL_USE_LIBSSH OFF)
        set(USE_NGHTTP2 OFF)
        set(USE_NGTCP2 OFF)
        set(USE_LIBIDN2 OFF)
        set(CURL_BROTLI OFF)
        set(CURL_ZSTD OFF)
        set(CURL_ZLIB OFF)
        set(CURL_DISABLE_INSTALL ON)
        # curl 8.23 uses per-protocol switches; legacy HTTP_ONLY is ignored.
        foreach(
            protocol
            DICT
            FILE
            FTP
            GOPHER
            IMAP
            LDAP
            LDAPS
            MQTT
            POP3
            RTSP
            SMTP
            TELNET
            TFTP
        )
            set(CURL_DISABLE_${protocol} ON)
        endforeach()
        FetchContent_Declare(
            echo_curl
            URL https://curl.se/download/curl-8.23.0.tar.gz
            URL_HASH SHA256=e3de77cfa6367083c776a26647fc9f633b538c06c3baa73d931a252ac8f72b49
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(echo_curl)
        # Check on every configure, including FETCHCONTENT_SOURCE_DIR overrides;
        # a download patch stamp must not silently leave this macro ineffective.
        set(curl_patch
            "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/patches/curl-8.23.0-native-http-digest.patch"
        )
        set(curl_patch_script "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ApplyCurlPatch.cmake")
        set_property(
            DIRECTORY
            APPEND
            PROPERTY CMAKE_CONFIGURE_DEPENDS "${curl_patch}" "${curl_patch_script}"
        )
        execute_process(
            COMMAND
                "${CMAKE_COMMAND}" "-DSOURCE_DIR=${echo_curl_SOURCE_DIR}"
                "-DPATCH_FILE=${curl_patch}" -P "${curl_patch_script}" COMMAND_ERROR_IS_FATAL ANY
        )
        # Preserve explicit usernames and the challenge realm for RFC 7616,
        # including DOMAIN\user and DOMAIN/user. Schannel and NTLM keep SSPI;
        # SASL DIGEST-MD5 message generation retains SSPI. Cover every translation
        # unit because it also selects the private digestdata layout.
        target_compile_definitions(libcurl_static PRIVATE ECHO_CURL_NATIVE_HTTP_DIGEST)
    endfunction()
    echo_setup_curl()
else()
    # macOS supplies libcurl in the OS; Linux packages receive security fixes
    # from their distribution. Require the public API baseline we actually use.
    find_package(CURL 7.81 REQUIRED)
endif()
