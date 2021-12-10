cmake_minimum_required(VERSION 3.10)

project(tdscript VERSION 0.1)

message("* ${PROJECT_NAME} v${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

option(WITH_STATIC "link statically" OFF)
option(WITH_TESTS "make with tests" ON)

find_package(OpenSSL)
add_subdirectory(lib/td)

set(JSON_BuildTests OFF CACHE INTERNAL "")
add_subdirectory(lib/json)

configure_file(version.h.in include/tdscript/version.h)

SET(LIBRARIES Td::TdStatic ${OPENSSL_LIBRARIES} nlohmann_json::nlohmann_json)
SET(INCLUDES include ${PROJECT_BINARY_DIR}/include ${OPENSSL_INCLUDE_DIR})
SET(SOURCES src/client.cc)

add_executable(${PROJECT_NAME} ${SOURCES} example/main.cc)
target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC ${INCLUDES})

if (WITH_TESTS)
    enable_testing()
    add_subdirectory(lib/gtest)

    function(add_gtest test_file)
        get_filename_component(test_target_name_dir ${test_file} DIRECTORY)
        get_filename_component(test_target_name_fn ${test_file} NAME_WE)
        string(REGEX REPLACE "[\\/]" "_" test_target_name
                "${PROJECT_NAME}_${test_target_name_dir}_${test_target_name_fn}")

        add_executable(${test_target_name} ${SOURCES} ${test_file})
        target_link_libraries(${test_target_name} gtest gtest_main ${LIBRARIES})
        target_include_directories(${test_target_name} PUBLIC ${INCLUDES})

        add_test(NAME ${test_target_name} COMMAND ${test_target_name})
    endfunction(add_gtest)

    add_gtest(test/client_test.cc)
    add_gtest(test/save_load_test.cc)
    add_gtest(test/epoll_test.cc)
endif (WITH_TESTS)
