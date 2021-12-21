cmake_minimum_required(VERSION 3.11)

project(tdscript_lib VERSION 0.1)

add_subdirectory(libdns)

set(OPENSSL_CRYPTO_LIBRARY crypto)
set(OPENSSL_SSL_LIBRARY ssl)

add_subdirectory(td)

if (WITH_STATIC)
    set(BUILD_SHARED_LIBS OFF)
    set(LIBXML2_WITH_ZLIB OFF)
    set(LIBXML2_WITH_TESTS OFF)
endif()
add_subdirectory(libxml2)