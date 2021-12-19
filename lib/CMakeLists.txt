cmake_minimum_required(VERSION 3.11)

project(tdscript_lib VERSION 0.1)

add_subdirectory(openssl)

if (WITH_STATIC)
    set(ZLIB_LIBRARY "/usr/lib/libz.a")
    set(OPENSSL_CRYPTO_LIBRARY "${PROJECT_SOURCE_DIR}/install/openssl/lib/libcrypto.a")
    set(OPENSSL_SSL_LIBRARY "${PROJECT_SOURCE_DIR}/install/openssl/lib/libssl.a")
    set(TD_BUILD_DIR "build_static")
    set(TD_INSTALL_DIR "${PROJECT_SOURCE_DIR}/install/td_static")
else()
    set(ZLIB_LIBRARY "/usr/lib/libz.so")
    set(OPENSSL_CRYPTO_LIBRARY "${PROJECT_SOURCE_DIR}/install/openssl/lib/libcrypto.so")
    set(OPENSSL_SSL_LIBRARY "${PROJECT_SOURCE_DIR}/install/openssl/lib/libssl.so")
    set(TD_BUILD_DIR "build")
    set(TD_INSTALL_DIR "${PROJECT_SOURCE_DIR}/install/td")
endif()

if (NOT EXISTS "${TD_INSTALL_DIR}/lib/libtdcore.a")
    execute_process(
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/td"
            COMMAND "cmake" "-DCMAKE_BUILD_TYPE=Release"
                "-DWITH_STATIC=${WITH_STATIC}"
                "-DZLIB_LIBRARY=${ZLIB_LIBRARY}"
                "-DOPENSSL_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/install/openssl/include"
                "-DOPENSSL_CRYPTO_LIBRARY=${OPENSSL_CRYPTO_LIBRARY}"
                "-DOPENSSL_SSL_LIBRARY=${OPENSSL_SSL_LIBRARY}"
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
            COMMAND "cmake" "--install" "build"
            RESULT_VARIABLE ret
    )
    if(${ret})
        message(FATAL_ERROR "td install error")
    endif()
endif()

set(Td_DIR "${TD_INSTALL_DIR}/lib/cmake/Td" PARENT_SCOPE)

add_subdirectory(libdns)

if (WITH_STATIC)
    set(BUILD_SHARED_LIBS OFF)
    set(LIBXML2_WITH_ZLIB OFF)
endif()
add_subdirectory(libxml2)