# std-async-algorithms
#
# Copyright (c) 2021-2022, ETH Zurich
# All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.22)

project(std-async-algorithms CXX)

find_package(STDEXEC REQUIRED)
find_package(pika)

# Main target
add_library(std_async_algorithms INTERFACE)
target_include_directories(std_async_algorithms INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(std_async_algorithms INTERFACE STDEXEC::stdexec)

# Tests
option(STD_ASYNC_ALGORITHMS_WITH_TESTS "Enable tests" OFF)

if(STD_ASYNC_ALGORITHMS_WITH_TESTS)

  function(add_algo_test name)
    add_executable(${name}_test tests/${name}.cpp)
    target_link_libraries(${name}_test PRIVATE std_async_algorithms $<TARGET_NAME_IF_EXISTS:pika::pika>)
    target_compile_definitions(${name}_test PRIVATE $<$<TARGET_EXISTS:pika::pika>:STD_ASYNC_ALGOS_HAVE_PIKA>)
    add_test(NAME ${name} COMMAND ${name}_test)
  endfunction()

  enable_testing()
  include(CTest)

  add_algo_test(for_each)
  add_algo_test(merge)

endif()
