cmake_minimum_required(VERSION 3.11)

project(tdscript_lib VERSION 0.1)

add_subdirectory(libdns)

set(OPENSSL_CRYPTO_LIBRARY crypto)
set(OPENSSL_SSL_LIBRARY ssl)

if (WITH_STATIC)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  set(BUILD_SHARED_LIBS OFF)
  set(LIBXML2_WITH_ZLIB OFF)
  set(LIBXML2_WITH_TESTS OFF)
endif()

add_subdirectory(td)

add_subdirectory(libxml2)