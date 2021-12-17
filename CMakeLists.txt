cmake_minimum_required(VERSION 3.11)

project(tdscript VERSION 0.1)

add_subdirectory(lib)

find_package(Td REQUIRED NO_DEFAULT_PATH)

configure_file(version.h.in include/${PROJECT_NAME}/version.h)

option(WITH_STATIC "link statically" OFF)
option(WITH_TESTS "make with tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

set(LIBRARIES Td::TdStatic nlohmann_json::nlohmann_json LibXml2)
set(INCLUDES include ${PROJECT_BINARY_DIR}/include)
set(SOURCES client.cc)

# check: readelf -d build/tdscript
add_executable(${PROJECT_NAME} ${SOURCES} example/main.cc)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBRARIES} -static)
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDES})

if (WITH_TESTS)
  enable_testing()
  add_subdirectory(lib/gtest)

  function(add_gtest test_file)
    get_filename_component(test_target_name_dir ${test_file} DIRECTORY)
    get_filename_component(test_target_name_fn ${test_file} NAME_WE)
    string(REGEX REPLACE "[\\/]" "_" test_target_name
            "${PROJECT_NAME}_${test_target_name_dir}_${test_target_name_fn}")

    add_executable(${test_target_name} ${SOURCES} ${test_file})
    target_link_libraries(${test_target_name} gtest gtest_main ${LIBRARIES} -static)
    target_include_directories(${test_target_name} PRIVATE ${INCLUDES})

    add_test(NAME ${test_target_name} COMMAND ${test_target_name})
  endfunction(add_gtest)

  add_gtest(test/client_test.cc)
  add_gtest(test/save_load_test.cc)
  add_gtest(test/epoll_test.cc)
endif (WITH_TESTS)
