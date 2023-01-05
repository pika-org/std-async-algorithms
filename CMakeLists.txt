# std-async-algorithms
#
# Copyright (c) 2021-2022, ETH Zurich
# All rights reserved.
#
# Please, refer to the LICENSE file in the root directory.
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.22)

project(std-async-algorithms CXX)

# TODO: This is missing from STDEXEC's config file.
find_package(Threads REQUIRED)
find_package(STDEXEC REQUIRED)
find_package(pika)

# Main target
add_library(std_async_algorithms INTERFACE)
target_include_directories(std_async_algorithms INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(std_async_algorithms INTERFACE STDEXEC::stdexec)

# Test executable
add_executable(for_each tests/for_each.cpp)
target_link_libraries(for_each PRIVATE std_async_algorithms $<TARGET_NAME_IF_EXISTS:pika::pika>)
target_compile_definitions(for_each PRIVATE $<$<TARGET_EXISTS:pika::pika>:STD_ASYNC_ALGOS_HAVE_PIKA>)
