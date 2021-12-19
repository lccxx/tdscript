cmake_minimum_required(VERSION 3.11)

project(tdscript VERSION 0.1)

add_subdirectory(lib)

find_package(Td REQUIRED NO_DEFAULT_PATH)

option(WITH_TESTS "make with tests" ON)
option(WITH_STATIC "link statically" OFF)
option(WITH_IPV6 "use IPv6 server address" OFF)

configure_file(config.h.in include/${PROJECT_NAME}/config.h)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(LIBRARIES Td::TdStatic libdns LibXml2)
set(INCLUDES include ${PROJECT_BINARY_DIR}/include)
set(SOURCES client.cc)

if (WITH_STATIC) # check: readelf -d build/tdscript
  set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  set(LIBRARIES ${LIBRARIES} -static)
endif()

add_executable(${PROJECT_NAME} ${SOURCES} example/main.cc)
target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDES})

if (WITH_TESTS)
  enable_testing()

  function(add_ctest test_file)
    get_filename_component(test_target_name_dir ${test_file} DIRECTORY)
    get_filename_component(test_target_name_fn ${test_file} NAME_WE)
    string(REGEX REPLACE "[\\/]" "_" test_target_name
            "${PROJECT_NAME}_${test_target_name_dir}_${test_target_name_fn}")

    add_executable(${test_target_name} ${SOURCES} ${test_file})
    target_link_libraries(${test_target_name} ${LIBRARIES})
    target_include_directories(${test_target_name} PRIVATE ${INCLUDES})

    add_test(NAME ${test_target_name} COMMAND ${test_target_name})
  endfunction(add_ctest)

  add_ctest(test/client_test.cc)
  add_ctest(test/save_load_test.cc)
  add_ctest(test/epoll_test.cc)
endif (WITH_TESTS)
