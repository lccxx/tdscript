cmake_minimum_required(VERSION 3.11)

project(tdscript VERSION 0.1)

option(WITH_TESTS "make with tests" ON)
option(WITH_STATIC "link statically" OFF)
option(WITH_IPV6 "use IPv6 server address" OFF)

if (NOT TD_INSTALL_DIR)
  set(TD_INSTALL_DIR "${PROJECT_BINARY_DIR}/lib/td")
  set(Td_DIR "${TD_INSTALL_DIR}/lib/cmake/Td")
endif()

add_subdirectory(lib)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lib/libdns/CMake;${CMAKE_MODULE_PATH}")
find_package(LibreSSL REQUIRED)
find_package(Td REQUIRED NO_DEFAULT_PATH)

configure_file(config.h.in include/${PROJECT_NAME}/config.h)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(LIBRARIES libdns Td::TdStatic LibXml2)
set(INCLUDES include ${PROJECT_BINARY_DIR}/include)
set(SOURCES client.cc)

if (WITH_STATIC) # check: readelf -d build/tdscript
  set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++")
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

  add_ctest(test/01_client.cc)
  add_ctest(test/02_save_load.cc)
  add_ctest(test/03_xml.cc)
  add_ctest(test/04_https.cc)
  add_ctest(test/05_wiktionary.cc)
  add_ctest(test/06_dict_exerting.cc)
  add_ctest(test/07_dict_damper.cc)
  add_ctest(test/08_dict_raffle.cc)
  add_ctest(test/09_dict_architecture.cc)
  add_ctest(test/10_dict_difference.cc)
  add_ctest(test/11_dict_vile.cc)
  add_ctest(test/12_dict_slingshot.cc)
  add_ctest(test/13_dict_quarters.cc)
  add_ctest(test/14_dict_quarter.cc)
  add_ctest(test/15_dict_tuple.cc)
  add_ctest(test/16_dict_quintuple.cc)
  add_ctest(test/17_dict_nice.cc)
  add_ctest(test/18_dict_see_also.cc)
  add_ctest(test/19_dict_see.cc)
  add_ctest(test/20_dict_see.cc)
  add_ctest(test/21_dict_see.cc)
  add_ctest(test/22_dict_symbol.cc)
  add_ctest(test/23_dict_diktuma.cc)
  add_ctest(test/24_dict_somniferum.cc)
  add_ctest(test/25_dict_abk.cc)
  add_ctest(test/26_dict_so.cc)
endif (WITH_TESTS)
