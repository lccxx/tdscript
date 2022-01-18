cmake_minimum_required(VERSION 3.11)

project(tdscript_lib VERSION 0.1)

add_subdirectory(libdns)

if (NOT LIBRESSL_ROOT_DIR)
  set(LIBRESSL_ROOT_DIR ${PROJECT_BINARY_DIR}/libdns/lib/ssl/lib)
endif()

if (NOT ZLIB_LIBRARY)
  set(ZLIB_LIBRARY /usr/lib/libz.a)
endif()

set(TD_BUILD_DIR "${PROJECT_BINARY_DIR}/td-build")

if (NOT EXISTS "${TD_INSTALL_DIR}/lib/libtdcore.a")
  execute_process(
          WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/td"
          COMMAND "cmake" "-DCMAKE_BUILD_TYPE=Release"
          "-DLIBRESSL_ROOT_DIR=${LIBRESSL_ROOT_DIR}"
          "-DZLIB_LIBRARY=${ZLIB_LIBRARY}"
          "-DCMAKE_INSTALL_PREFIX=${TD_INSTALL_DIR}"
          "-S" "." "-B" "${TD_BUILD_DIR}"
          RESULT_VARIABLE ret
  )
  if(${ret})
    message(FATAL_ERROR "td cmake configure error")
  endif()

  execute_process(
          WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/td"
          COMMAND "cmake" "--build" "${TD_BUILD_DIR}"
          RESULT_VARIABLE ret
  )
  if(${ret})
    message(FATAL_ERROR "td make error")
  endif()

  execute_process(
          WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/td"
          COMMAND "cmake" "--install" "${TD_BUILD_DIR}"
          RESULT_VARIABLE ret
  )
  if(${ret})
    message(FATAL_ERROR "td install error")
  endif()
endif()


if (WITH_STATIC)
  set(BUILD_SHARED_LIBS OFF)
endif()
set(LIBXML2_WITH_ZLIB OFF)
set(LIBXML2_WITH_LZMA OFF)
set(LIBXML2_WITH_PYTHON OFF)
set(LIBXML2_WITH_FTP OFF)
set(LIBXML2_WITH_HTTP OFF)
set(LIBXML2_WITH_TESTS OFF)
add_subdirectory(libxml2)
