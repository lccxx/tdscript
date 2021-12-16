cmake_minimum_required(VERSION 3.11)

project(tdscript_lib VERSION 0.1)

add_subdirectory(openssl)

if (NOT EXISTS "${PROJECT_SOURCE_DIR}/install/td/lib/libtdcore.a")
    execute_process(
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/td"
            COMMAND "cmake" "-DCMAKE_BUILD_TYPE=Release"
                "-DOPENSSL_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/install/openssl/include"
                "-DOPENSSL_LIBRARIES=${PROJECT_SOURCE_DIR}/install/openssl/lib64"
                "-DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/install/td"
                "-S" "." "-B" "build"
            RESULT_VARIABLE ret
    )
    if(${ret})
        message(FATAL_ERROR "td cmake configure error")
    endif()

    execute_process(
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/td"
            COMMAND "cmake" "--build" "build"
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

set(Td_DIR "${PROJECT_SOURCE_DIR}/install/td/lib/cmake/Td" PARENT_SCOPE)

add_subdirectory(json)

if (WITH_STATIC)
    set(BUILD_SHARED_LIBS OFF)
endif()
add_subdirectory(libxml2)